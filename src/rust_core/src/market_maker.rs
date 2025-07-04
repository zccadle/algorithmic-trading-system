use crate::smart_order_router::{ExchangeID, SmartOrderRouter};
use std::time::Instant;

#[derive(Debug, Clone)]
pub struct Quote {
    pub price: f64,
    pub quantity: u32,
    pub is_buy_side: bool,
    pub target_exchange: ExchangeID,
}

impl Quote {
    pub fn new(price: f64, quantity: u32, is_buy_side: bool, target_exchange: ExchangeID) -> Self {
        Quote {
            price,
            quantity,
            is_buy_side,
            target_exchange,
        }
    }
}

#[derive(Debug, Clone)]
pub struct MarketMakerQuotes {
    pub buy_quote: Quote,
    pub sell_quote: Quote,
    pub theoretical_edge: f64,  // Expected profit if both quotes fill
}

#[derive(Debug, Clone)]
pub struct InventoryPosition {
    pub base_inventory: f64,    // e.g., BTC
    pub quote_inventory: f64,   // e.g., USD
    pub base_value: f64,        // Current value of base inventory
    pub total_value: f64,       // Total portfolio value
    pub pnl: f64,              // Profit and loss
}

#[derive(Debug, Clone)]
pub struct MarketMakerParameters {
    // Spread parameters
    pub base_spread_bps: f64,      // Base spread in basis points (1 bp = 0.01%)
    pub min_spread_bps: f64,       // Minimum allowed spread
    pub max_spread_bps: f64,       // Maximum allowed spread
    
    // Inventory management
    pub max_base_inventory: f64,   // Maximum BTC to hold
    pub max_quote_inventory: f64,  // Maximum USD to hold
    pub target_base_inventory: f64, // Target BTC inventory
    
    // Risk parameters
    pub inventory_skew_factor: f64, // How much to skew quotes based on inventory
    pub volatility_adjustment: f64, // Spread adjustment based on volatility
    
    // Quote sizing
    pub base_quote_size: f64,      // Base size for quotes
    pub min_quote_size: f64,       // Minimum quote size
    pub max_quote_size: f64,       // Maximum quote size
}

impl Default for MarketMakerParameters {
    fn default() -> Self {
        MarketMakerParameters {
            base_spread_bps: 10.0,        // 0.10% spread
            min_spread_bps: 5.0,          // 0.05% minimum
            max_spread_bps: 50.0,         // 0.50% maximum
            max_base_inventory: 10.0,      // 10 BTC max
            max_quote_inventory: 500000.0, // $500k max
            target_base_inventory: 5.0,    // Target 5 BTC
            inventory_skew_factor: 0.1,    // 10% skew per unit of inventory imbalance
            volatility_adjustment: 1.0,    // No volatility adjustment by default
            base_quote_size: 0.1,         // 0.1 BTC base size
            min_quote_size: 0.01,         // 0.01 BTC minimum
            max_quote_size: 1.0,          // 1.0 BTC maximum
        }
    }
}

pub struct MarketMaker<'a> {
    sor: &'a SmartOrderRouter,
    params: MarketMakerParameters,
    
    // Inventory tracking
    base_inventory: f64,
    quote_inventory: f64,
    initial_base_inventory: f64,
    initial_quote_inventory: f64,
    
    // Market data
    last_midpoint: f64,
    volatility_estimate: f64,
    
    // Performance tracking
    quotes_placed: u32,
    quotes_filled: u32,
    total_volume: f64,
    realized_pnl: f64,
    start_time: Instant,
}

impl<'a> MarketMaker<'a> {
    pub fn new(sor: &'a SmartOrderRouter, params: MarketMakerParameters) -> Self {
        MarketMaker {
            sor,
            params,
            base_inventory: 0.0,
            quote_inventory: 0.0,
            initial_base_inventory: 0.0,
            initial_quote_inventory: 0.0,
            last_midpoint: 0.0,
            volatility_estimate: 0.001,  // 0.1% default volatility
            quotes_placed: 0,
            quotes_filled: 0,
            total_volume: 0.0,
            realized_pnl: 0.0,
            start_time: Instant::now(),
        }
    }
    
    pub fn initialize(&mut self, base_inventory: f64, quote_inventory: f64) {
        self.base_inventory = base_inventory;
        self.quote_inventory = quote_inventory;
        self.initial_base_inventory = base_inventory;
        self.initial_quote_inventory = quote_inventory;
        
        println!("Market Maker initialized with:");
        println!("  Base inventory: {} BTC", self.base_inventory);
        println!("  Quote inventory: ${}", self.quote_inventory);
    }
    
    fn calculate_midpoint(&mut self) -> f64 {
        let market_data = self.sor.get_aggregated_market_data();
        
        if market_data.best_bid <= 0.0 || market_data.best_ask >= f64::MAX {
            // No valid market, use last known midpoint
            return self.last_midpoint;
        }
        
        let midpoint = (market_data.best_bid + market_data.best_ask) / 2.0;
        self.last_midpoint = midpoint;
        midpoint
    }
    
    fn calculate_spread(&self) -> f64 {
        // Start with base spread
        let mut spread_bps = self.params.base_spread_bps;
        
        // Adjust for volatility
        spread_bps *= 1.0 + self.volatility_estimate * self.params.volatility_adjustment;
        
        // Adjust for inventory imbalance
        let inventory_skew = self.calculate_inventory_skew();
        spread_bps *= 1.0 + inventory_skew.abs() * 0.5;
        
        // Enforce limits
        spread_bps = spread_bps.max(self.params.min_spread_bps).min(self.params.max_spread_bps);
        
        spread_bps / 10000.0  // Convert basis points to decimal
    }
    
    fn calculate_inventory_skew(&self) -> f64 {
        if self.params.target_base_inventory <= 0.0 {
            return 0.0;
        }
        
        let inventory_ratio = self.base_inventory / self.params.target_base_inventory;
        let imbalance = inventory_ratio - 1.0;
        
        // Skew factor: positive means too much inventory (lower bid, raise ask)
        imbalance * self.params.inventory_skew_factor
    }
    
    fn calculate_quote_prices(&self, midpoint: f64, spread: f64) -> (f64, f64) {
        let half_spread = spread / 2.0;
        let inventory_skew = self.calculate_inventory_skew();
        
        // Adjust prices based on inventory
        // If we have too much inventory, lower bid and raise ask
        let bid_adjustment = 1.0 - half_spread - (inventory_skew * half_spread);
        let ask_adjustment = 1.0 + half_spread + (inventory_skew * half_spread);
        
        let bid_price = midpoint * bid_adjustment;
        let ask_price = midpoint * ask_adjustment;
        
        (bid_price, ask_price)
    }
    
    fn calculate_quote_size(&self, is_buy_side: bool) -> u32 {
        let mut base_size = self.params.base_quote_size;
        
        // Adjust size based on inventory
        if is_buy_side {
            // Reduce buy size if we have too much base inventory
            let inventory_ratio = self.base_inventory / self.params.max_base_inventory;
            base_size *= 1.0 - inventory_ratio * 0.5;
        } else {
            // Reduce sell size if we have too little base inventory
            let inventory_ratio = self.base_inventory / self.params.target_base_inventory;
            base_size *= inventory_ratio.min(1.0);
        }
        
        // Convert to integer quantity (assuming whole units for simplicity)
        let quantity = (base_size * 100.0) as u32;  // Convert to smallest unit
        
        // Enforce limits
        quantity.max((self.params.min_quote_size * 100.0) as u32)
                .min((self.params.max_quote_size * 100.0) as u32)
    }
    
    pub fn update_quotes(&mut self) -> Option<MarketMakerQuotes> {
        // Get current market state
        let midpoint = self.calculate_midpoint();
        if midpoint <= 0.0 {
            eprintln!("Invalid market midpoint");
            return None;
        }
        
        // Calculate spread and quote prices
        let spread = self.calculate_spread();
        let (bid_price, ask_price) = self.calculate_quote_prices(midpoint, spread);
        
        // Calculate quote sizes
        let buy_size = self.calculate_quote_size(true);
        let sell_size = self.calculate_quote_size(false);
        
        // Determine best exchanges for each quote
        self.quotes_placed += 1;
        let buy_routing = self.sor.route_order(self.quotes_placed, bid_price, buy_size, true);
        self.quotes_placed += 1;
        let sell_routing = self.sor.route_order(self.quotes_placed, ask_price, sell_size, false);
        
        // Create quotes
        let buy_quote = Quote::new(bid_price, buy_size, true, buy_routing.exchange_id);
        let sell_quote = Quote::new(ask_price, sell_size, false, sell_routing.exchange_id);
        
        // Calculate theoretical edge
        let theoretical_edge = (ask_price - bid_price) - 
                              (buy_routing.expected_fee + sell_routing.expected_fee);
        
        Some(MarketMakerQuotes {
            buy_quote,
            sell_quote,
            theoretical_edge,
        })
    }
    
    pub fn on_quote_filled(&mut self, filled_quote: &Quote, fill_price: f64, fill_quantity: u32) {
        self.quotes_filled += 1;
        self.total_volume += fill_quantity as f64;
        
        if filled_quote.is_buy_side {
            // We bought, increase base inventory, decrease quote inventory
            self.base_inventory += fill_quantity as f64 / 100.0;  // Convert from smallest unit
            self.quote_inventory -= fill_price * fill_quantity as f64 / 100.0;
            
            println!("Buy quote filled: +{} BTC @ ${}", fill_quantity as f64 / 100.0, fill_price);
        } else {
            // We sold, decrease base inventory, increase quote inventory
            self.base_inventory -= fill_quantity as f64 / 100.0;
            self.quote_inventory += fill_price * fill_quantity as f64 / 100.0;
            
            println!("Sell quote filled: -{} BTC @ ${}", fill_quantity as f64 / 100.0, fill_price);
        }
        
        // Update realized PnL (simplified - assumes we can always close at midpoint)
        let current_midpoint = self.last_midpoint;
        let position_value = self.base_inventory * current_midpoint + self.quote_inventory;
        let initial_value = self.initial_base_inventory * current_midpoint + self.initial_quote_inventory;
        self.realized_pnl = position_value - initial_value;
    }
    
    pub fn is_within_risk_limits(&self) -> bool {
        // Check inventory limits
        if self.base_inventory > self.params.max_base_inventory || 
           self.base_inventory < 0.0 {
            return false;
        }
        
        if self.quote_inventory > self.params.max_quote_inventory || 
           self.quote_inventory < -self.params.max_quote_inventory * 0.1 {  // Allow small negative
            return false;
        }
        
        // Check position limits
        let current_midpoint = self.last_midpoint;
        let position_value = (self.base_inventory * current_midpoint).abs();
        let max_position_value = self.params.max_base_inventory * current_midpoint;
        
        position_value <= max_position_value * 1.1  // 10% buffer
    }
    
    pub fn adjust_parameters_for_risk(&mut self) {
        if !self.is_within_risk_limits() {
            // Widen spreads if outside risk limits
            self.params.base_spread_bps *= 1.5;
            self.params.base_quote_size *= 0.5;
            
            println!("Risk limits exceeded - adjusting parameters");
        }
    }
    
    pub fn get_inventory_position(&self) -> InventoryPosition {
        let current_midpoint = self.last_midpoint;
        let base_value = self.base_inventory * current_midpoint;
        let total_value = base_value + self.quote_inventory;
        
        let initial_value = self.initial_base_inventory * current_midpoint + self.initial_quote_inventory;
        let pnl = total_value - initial_value;
        
        InventoryPosition {
            base_inventory: self.base_inventory,
            quote_inventory: self.quote_inventory,
            base_value,
            total_value,
            pnl,
        }
    }
    
    pub fn get_inventory_imbalance(&self) -> f64 {
        if self.params.target_base_inventory <= 0.0 {
            return 0.0;
        }
        
        (self.base_inventory - self.params.target_base_inventory) / self.params.target_base_inventory
    }
    
    pub fn get_fill_rate(&self) -> f64 {
        if self.quotes_placed == 0 {
            return 0.0;
        }
        
        self.quotes_filled as f64 / self.quotes_placed as f64
    }
    
    pub fn print_performance_stats(&self) {
        let duration = self.start_time.elapsed();
        
        println!("\n=== Market Maker Performance Stats ===");
        println!("Runtime: {} seconds", duration.as_secs());
        println!("Quotes placed: {}", self.quotes_placed);
        println!("Quotes filled: {}", self.quotes_filled);
        println!("Fill rate: {:.1}%", self.get_fill_rate() * 100.0);
        println!("Total volume: {:.2} BTC", self.total_volume / 100.0);
        
        let pos = self.get_inventory_position();
        println!("\nInventory Position:");
        println!("  Base: {:.2} BTC (value: ${:.2})", pos.base_inventory, pos.base_value);
        println!("  Quote: ${:.2}", pos.quote_inventory);
        println!("  Total value: ${:.2}", pos.total_value);
        
        let initial_value = self.initial_base_inventory * self.last_midpoint + self.initial_quote_inventory;
        if initial_value > 0.0 {
            println!("  P&L: ${:.2} ({:.2}%)", pos.pnl, (pos.pnl / initial_value) * 100.0);
        } else {
            println!("  P&L: ${:.2}", pos.pnl);
        }
        
        println!("\nCurrent Parameters:");
        println!("  Base spread: {:.1} bps", self.params.base_spread_bps);
        println!("  Quote size: {:.2} BTC", self.params.base_quote_size);
        println!("  Inventory skew: {:.1}%", self.calculate_inventory_skew() * 100.0);
    }
    
    pub fn estimate_volatility(&mut self) -> f64 {
        // Simplified volatility estimate based on spread
        let market_data = self.sor.get_aggregated_market_data();
        
        if market_data.best_bid <= 0.0 || market_data.best_ask >= f64::MAX {
            return self.volatility_estimate;  // Return last estimate
        }
        
        let spread = (market_data.best_ask - market_data.best_bid) / market_data.best_bid;
        
        // Smooth the estimate
        self.volatility_estimate = self.volatility_estimate * 0.9 + spread * 0.1;
        
        self.volatility_estimate
    }
    
    pub fn update_parameters(&mut self, new_params: MarketMakerParameters) {
        self.params = new_params;
    }
    
    pub fn get_parameters(&self) -> &MarketMakerParameters {
        &self.params
    }
    
    pub fn get_realized_pnl(&self) -> f64 {
        self.realized_pnl
    }
}