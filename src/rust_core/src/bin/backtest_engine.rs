use rust_core::order_book::{OrderBook, Trade};
use std::io::{self, BufRead, BufReader};
use std::fs::File;
use rand::Rng;

#[derive(Debug)]
struct MarketDataPoint {
    timestamp: i64,
    symbol: String,
    bid: f64,
    ask: f64,
    bid_size: f64,
    ask_size: f64,
    last_price: f64,
    volume: f64,
}

#[derive(Debug, Clone)]
struct MarketLevel {
    price: f64,
    quantity: f64,
}

#[derive(Debug, Clone)]
struct MarketDepth {
    bids: Vec<MarketLevel>,
    asks: Vec<MarketLevel>,
}

impl MarketDepth {
    fn new() -> Self {
        Self {
            bids: Vec::new(),
            asks: Vec::new(),
        }
    }
}

#[derive(Debug, Clone)]
struct BacktestConfig {
    enable_market_maker: bool,
    enable_sor: bool,
    num_exchanges: usize,
    initial_base_inventory: f64,
    initial_quote_inventory: f64,
    enable_market_impact: bool,
    enable_latency_simulation: bool,
    base_latency_us: f64,
    market_impact_factor: f64,
    aggressive_market_making: bool,
    cross_spread_probability: f64,
    order_book_depth: usize,
    base_depth_size: f64,
    depth_decay_factor: f64,
}

impl Default for BacktestConfig {
    fn default() -> Self {
        Self {
            enable_market_maker: true,
            enable_sor: true,
            num_exchanges: 1,
            initial_base_inventory: 1.0,
            initial_quote_inventory: 10000.0,
            enable_market_impact: true,
            enable_latency_simulation: true,
            base_latency_us: 100.0,
            market_impact_factor: 0.0001,
            aggressive_market_making: false,
            cross_spread_probability: 0.3,
            order_book_depth: 10,
            base_depth_size: 0.5,
            depth_decay_factor: 0.8,
        }
    }
}

#[derive(Debug)]
struct PerformanceMetrics {
    total_trades: usize,
    total_volume: f64,
    realized_pnl: f64,
    unrealized_pnl: f64,
    sharpe_ratio: f64,
    max_drawdown: f64,
    win_rate: f64,
    avg_trade_size: f64,
    total_fees_paid: f64,
    market_maker_trades: usize,
    market_trades: usize,
    final_base_inventory: f64,
    final_quote_inventory: f64,
}

struct BacktestEngine {
    config: BacktestConfig,
    exchange_books: Vec<OrderBook>,
    current_timestamp: i64,
    last_market_price: f64,
    metrics: PerformanceMetrics,
    pnl_history: Vec<f64>,
    high_water_mark: f64,
    trade_results: Vec<f64>,
    market_depths: Vec<MarketDepth>,
    next_order_id: u32,
    base_inventory: f64,
    quote_inventory: f64,
}

impl BacktestEngine {
    fn new(config: BacktestConfig) -> Self {
        let mut exchange_books = Vec::new();
        
        for _ in 0..config.num_exchanges {
            exchange_books.push(OrderBook::new());
        }
        
        let mut market_depths = Vec::new();
        for _ in 0..config.num_exchanges {
            market_depths.push(MarketDepth::new());
        }
        
        Self {
            base_inventory: config.initial_base_inventory,
            quote_inventory: config.initial_quote_inventory,
            config,
            exchange_books,
            current_timestamp: 0,
            last_market_price: 0.0,
            metrics: PerformanceMetrics {
                total_trades: 0,
                total_volume: 0.0,
                realized_pnl: 0.0,
                unrealized_pnl: 0.0,
                sharpe_ratio: 0.0,
                max_drawdown: 0.0,
                win_rate: 0.0,
                avg_trade_size: 0.0,
                total_fees_paid: 0.0,
                market_maker_trades: 0,
                market_trades: 0,
                final_base_inventory: 0.0,
                final_quote_inventory: 0.0,
            },
            pnl_history: Vec::new(),
            high_water_mark: 0.0,
            trade_results: Vec::new(),
            market_depths,
            next_order_id: 1000,
        }
    }
    
    fn get_next_order_id(&mut self) -> u32 {
        let id = self.next_order_id;
        self.next_order_id += 1;
        id
    }
    
    fn simulate_market_depth(&mut self, exchange_idx: usize, best_bid: f64, best_ask: f64) {
        let depth = &mut self.market_depths[exchange_idx];
        depth.bids.clear();
        depth.asks.clear();
        
        for i in 0..self.config.order_book_depth {
            let bid_price = best_bid - (i as f64 * 0.01);
            let ask_price = best_ask + (i as f64 * 0.01);
            
            let size_multiplier = self.config.depth_decay_factor.powi(i as i32);
            let bid_size = self.config.base_depth_size * size_multiplier;
            let ask_size = self.config.base_depth_size * size_multiplier;
            
            depth.bids.push(MarketLevel {
                price: bid_price,
                quantity: bid_size,
            });
            
            depth.asks.push(MarketLevel {
                price: ask_price,
                quantity: ask_size,
            });
        }
    }
    
    fn apply_market_impact(&mut self, exchange_idx: usize, is_buy: bool, quantity: f64) -> f64 {
        if !self.config.enable_market_impact {
            return 0.0;
        }
        
        let depth = &self.market_depths[exchange_idx];
        let levels = if is_buy { &depth.asks } else { &depth.bids };
        
        let mut remaining_qty = quantity;
        let mut total_impact = 0.0;
        let mut level_idx = 0;
        
        while remaining_qty > 0.0 && level_idx < levels.len() {
            let level = &levels[level_idx];
            let taken_qty = remaining_qty.min(level.quantity);
            
            let level_impact = if is_buy {
                level.price - levels[0].price
            } else {
                levels[0].price - level.price
            };
            
            total_impact += level_impact * (taken_qty / quantity);
            remaining_qty -= taken_qty;
            level_idx += 1;
        }
        
        total_impact + remaining_qty * self.config.market_impact_factor * levels[0].price
    }
    
    fn simulate_latency(&self) -> u64 {
        if !self.config.enable_latency_simulation {
            return 0;
        }
        
        let mut rng = rand::thread_rng();
        let variation = (rng.gen::<f64>() - 0.5) * 2.0 * 50.0;
        ((self.config.base_latency_us + variation).max(0.0)) as u64
    }
    
    fn calculate_fees(&self, price: f64, quantity: f64, is_maker: bool) -> f64 {
        let fee_rate = if is_maker { 0.001 } else { 0.002 };
        price * quantity * fee_rate
    }
    
    fn process_market_data(&mut self, data: &MarketDataPoint) {
        self.current_timestamp = data.timestamp;
        self.last_market_price = data.last_price;
        
        // First, simulate market depths
        for idx in 0..self.exchange_books.len() {
            self.simulate_market_depth(idx, data.bid, data.ask);
        }
        
        // Then process orders for each exchange
        for idx in 0..self.exchange_books.len() {
            let mut orders_to_add = Vec::new();
            
            // Collect bid orders
            for level in self.market_depths[idx].bids.clone() {
                let order_id = self.get_next_order_id();
                let quantity = (level.quantity * 100.0) as u32;
                orders_to_add.push((order_id, level.price, quantity, true));
            }
            
            // Collect ask orders
            for level in self.market_depths[idx].asks.clone() {
                let order_id = self.get_next_order_id();
                let quantity = (level.quantity * 100.0) as u32;
                orders_to_add.push((order_id, level.price, quantity, false));
            }
            
            // Add all orders to the book
            let book = &mut self.exchange_books[idx];
            for (order_id, price, quantity, is_buy) in orders_to_add {
                book.add_order(order_id, price, quantity, is_buy);
            }
        }
        
        self.simulate_market_orders(data);
    }
    
    fn simulate_market_orders(&mut self, data: &MarketDataPoint) {
        let mut rng = rand::thread_rng();
        let market_activity = data.volume / 1000.0;
        let should_generate = rng.gen::<f64>() < market_activity.min(0.5);
        
        if should_generate && self.config.enable_market_maker {
            for idx in 0..self.exchange_books.len() {
                let is_buy = rng.gen::<bool>();
                let quantity = 0.01 + rng.gen::<f64>() * 0.1;
                let quantity_units = (quantity * 100.0) as u32;
                
                let order_id = self.get_next_order_id();
                
                let trades = if is_buy {
                    self.exchange_books[idx].add_order(order_id, f64::MAX, quantity_units, true)
                } else {
                    self.exchange_books[idx].add_order(order_id, 0.01, quantity_units, false)
                };
                
                self.process_trades(&trades, idx, false);
            }
            
            self.generate_market_maker_quotes(data);
        }
    }
    
    fn generate_market_maker_quotes(&mut self, data: &MarketDataPoint) {
        if !self.config.enable_market_maker {
            return;
        }
        
        let spread = data.ask - data.bid;
        let midpoint = (data.bid + data.ask) / 2.0;
        
        if spread <= 0.0 || midpoint <= 0.0 {
            return;
        }
        
        let mut rng = rand::thread_rng();
        
        for idx in 0..self.exchange_books.len() {
            let cross_spread = self.config.aggressive_market_making && 
                              rng.gen::<f64>() < self.config.cross_spread_probability;
            
            let buy_price = if cross_spread {
                data.bid + spread * 0.25
            } else {
                data.bid - spread * 0.1
            };
            
            let sell_price = if cross_spread {
                data.ask - spread * 0.25
            } else {
                data.ask + spread * 0.1
            };
            
            let quote_size = 0.05 + rng.gen::<f64>() * 0.15;
            let quote_units = (quote_size * 100.0) as u32;
            
            // Collect order IDs first
            let buy_order_id = self.get_next_order_id();
            let sell_order_id = self.get_next_order_id();
            
            // Add buy order and process trades
            let buy_trades = self.exchange_books[idx].add_order(buy_order_id, buy_price, quote_units, true);
            self.process_trades(&buy_trades, idx, true);
            
            // Add sell order and process trades
            let sell_trades = self.exchange_books[idx].add_order(sell_order_id, sell_price, quote_units, false);
            self.process_trades(&sell_trades, idx, true);
        }
    }
    
    fn process_trades(&mut self, trades: &[Trade], exchange_idx: usize, is_mm_trade: bool) {
        for trade in trades {
            self.metrics.total_trades += 1;
            self.metrics.total_volume += trade.quantity as f64 / 100.0;
            
            if is_mm_trade {
                self.metrics.market_maker_trades += 1;
                
                let quantity = trade.quantity as f64 / 100.0;
                let is_buy = trade.buy_order_id > trade.sell_order_id;
                
                if is_buy {
                    self.base_inventory += quantity;
                    self.quote_inventory -= trade.price * quantity;
                } else {
                    self.base_inventory -= quantity;
                    self.quote_inventory += trade.price * quantity;
                }
                
                let fee = self.calculate_fees(trade.price, quantity, true);
                self.metrics.total_fees_paid += fee;
                self.quote_inventory -= fee;
            } else {
                self.metrics.market_trades += 1;
            }
            
            let impact = self.apply_market_impact(exchange_idx, 
                trade.buy_order_id > trade.sell_order_id, 
                trade.quantity as f64 / 100.0);
            
            println!("TRADE,{},{},{:.4},{:.6},{},{},{},{:.6}",
                self.current_timestamp,
                exchange_idx,
                trade.price,
                trade.quantity as f64 / 100.0,
                if trade.buy_order_id > trade.sell_order_id { "BUY" } else { "SELL" },
                if is_mm_trade { "MARKET_MAKER" } else { "MARKET" },
                trade.trade_id,
                impact
            );
        }
    }
    
    fn update_metrics(&mut self) {
        let base_value = self.base_inventory * self.last_market_price;
        let total_value = base_value + self.quote_inventory;
        
        let initial_value = self.config.initial_base_inventory * self.last_market_price + 
                           self.config.initial_quote_inventory;
        
        let pnl = total_value - initial_value;
        self.metrics.realized_pnl = pnl;
        
        self.pnl_history.push(pnl);
        
        if pnl > self.high_water_mark {
            self.high_water_mark = pnl;
        }
        
        let drawdown = if self.high_water_mark > 0.0 {
            (self.high_water_mark - pnl) / self.high_water_mark
        } else {
            0.0
        };
        
        if drawdown > self.metrics.max_drawdown {
            self.metrics.max_drawdown = drawdown;
        }
        
        println!("MM_STATE,{},{},{:.6},{:.2},{:.2},{:.2}",
            self.current_timestamp,
            0,
            self.base_inventory,
            self.quote_inventory,
            pnl,
            0.0
        );
    }
    
    fn calculate_final_metrics(&mut self) {
        self.metrics.final_base_inventory = self.base_inventory;
        self.metrics.final_quote_inventory = self.quote_inventory;
        
        if self.metrics.total_trades > 0 {
            self.metrics.avg_trade_size = self.metrics.total_volume / self.metrics.total_trades as f64;
        }
        
        if self.pnl_history.len() > 1 {
            let returns: Vec<f64> = self.pnl_history.windows(2)
                .map(|w| if w[0] != 0.0 { (w[1] - w[0]) / w[0].abs() } else { 0.0 })
                .collect();
            
            if !returns.is_empty() {
                let mean_return = returns.iter().sum::<f64>() / returns.len() as f64;
                let variance = returns.iter()
                    .map(|r| (r - mean_return).powi(2))
                    .sum::<f64>() / returns.len() as f64;
                let std_dev = variance.sqrt();
                
                if std_dev > 0.0 {
                    self.metrics.sharpe_ratio = (mean_return * 252.0f64.sqrt()) / std_dev;
                }
            }
        }
        
        let winning_trades = self.trade_results.iter().filter(|&&r| r > 0.0).count();
        if !self.trade_results.is_empty() {
            self.metrics.win_rate = winning_trades as f64 / self.trade_results.len() as f64;
        }
    }
    
    fn print_summary(&self) {
        println!("\n========== BACKTEST SUMMARY ==========");
        println!("Total Trades: {}", self.metrics.total_trades);
        println!("Total Volume: {:.2}", self.metrics.total_volume);
        println!("Average Trade Size: {:.4}", self.metrics.avg_trade_size);
        println!("Market Maker Trades: {}", self.metrics.market_maker_trades);
        println!("Market Trades: {}", self.metrics.market_trades);
        println!("\nP&L METRICS:");
        println!("Realized P&L: ${:.2}", self.metrics.realized_pnl);
        println!("Total Fees Paid: ${:.2}", self.metrics.total_fees_paid);
        println!("Net P&L: ${:.2}", self.metrics.realized_pnl - self.metrics.total_fees_paid);
        println!("Sharpe Ratio: {:.4}", self.metrics.sharpe_ratio);
        println!("Max Drawdown: {:.2}%", self.metrics.max_drawdown * 100.0);
        println!("Win Rate: {:.2}%", self.metrics.win_rate * 100.0);
        println!("\nFINAL INVENTORY:");
        println!("Base: {:.6}", self.metrics.final_base_inventory);
        println!("Quote: ${:.2}", self.metrics.final_quote_inventory);
        println!("=====================================");
    }
    
    fn run(&mut self, input_file: Option<&str>) -> io::Result<()> {
        println!("timestamp,exchange_id,price,quantity,side,maker,taker,impact");
        
        let reader: Box<dyn BufRead> = if let Some(file_path) = input_file {
            Box::new(BufReader::new(File::open(file_path)?))
        } else {
            Box::new(io::stdin().lock())
        };
        
        let mut csv_reader = csv::ReaderBuilder::new()
            .has_headers(true)
            .from_reader(reader);
        
        for result in csv_reader.records() {
            let record = result?;
            
            let data = MarketDataPoint {
                timestamp: record.get(0).unwrap().parse().unwrap_or(0),
                symbol: record.get(1).unwrap_or("BTC-USD").to_string(),
                bid: record.get(2).unwrap().parse().unwrap_or(0.0),
                ask: record.get(3).unwrap().parse().unwrap_or(0.0),
                bid_size: record.get(4).unwrap().parse().unwrap_or(0.0),
                ask_size: record.get(5).unwrap().parse().unwrap_or(0.0),
                last_price: record.get(6).unwrap().parse().unwrap_or(0.0),
                volume: record.get(7).unwrap().parse().unwrap_or(0.0),
            };
            
            if data.bid > 0.0 && data.ask > 0.0 && data.last_price > 0.0 {
                self.process_market_data(&data);
                self.update_metrics();
            }
        }
        
        self.calculate_final_metrics();
        self.print_summary();
        
        Ok(())
    }
}

fn main() -> io::Result<()> {
    let args: Vec<String> = std::env::args().collect();
    
    let mut config = BacktestConfig::default();
    
    let mut i = 1;
    let mut input_file = None;
    while i < args.len() {
        match args[i].as_str() {
            "--aggressive" => config.aggressive_market_making = true,
            "--no-mm" => config.enable_market_maker = false,
            "--no-sor" => config.enable_sor = false,
            "--exchanges" => {
                if i + 1 < args.len() {
                    config.num_exchanges = args[i + 1].parse().unwrap_or(1);
                    i += 1;
                }
            }
            "--no-impact" => config.enable_market_impact = false,
            "--no-latency" => config.enable_latency_simulation = false,
            "--file" => {
                if i + 1 < args.len() {
                    input_file = Some(args[i + 1].clone());
                    i += 1;
                }
            }
            _ => {}
        }
        i += 1;
    }
    
    let mut engine = BacktestEngine::new(config);
    engine.run(input_file.as_deref())?;
    
    Ok(())
}