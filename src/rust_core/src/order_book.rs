use std::collections::{BTreeMap, HashMap};
use std::cmp::{Reverse, min};

#[derive(Debug, Clone)]
pub struct Trade {
    pub trade_id: u32,
    pub price: f64,
    pub quantity: u32,
    pub buy_order_id: u32,
    pub sell_order_id: u32,
}

impl Trade {
    pub fn new(trade_id: u32, price: f64, quantity: u32, buy_order_id: u32, sell_order_id: u32) -> Self {
        Trade {
            trade_id,
            price,
            quantity,
            buy_order_id,
            sell_order_id,
        }
    }
}

#[derive(Debug, Clone)]
pub struct Order {
    pub order_id: u32,
    pub price: f64,
    pub quantity: u32,
    pub is_buy_side: bool,
}

impl Order {
    pub fn new(order_id: u32, price: f64, quantity: u32, is_buy_side: bool) -> Self {
        Order {
            order_id,
            price,
            quantity,
            is_buy_side,
        }
    }
}

pub struct OrderBook {
    buy_levels: BTreeMap<Reverse<u64>, u32>,           // Price (as fixed point) -> Total quantity
    sell_levels: BTreeMap<u64, u32>,                   // Price (as fixed point) -> Total quantity
    buy_orders_at_level: BTreeMap<Reverse<u64>, Vec<u32>>, // Price -> Order IDs
    sell_orders_at_level: BTreeMap<u64, Vec<u32>>,         // Price -> Order IDs
    orders: HashMap<u32, Order>,                       // Order ID -> Order details
    next_trade_id: u32,
}

impl Default for OrderBook {
    fn default() -> Self {
        Self::new()
    }
}

impl OrderBook {
    pub fn new() -> Self {
        OrderBook {
            buy_levels: BTreeMap::new(),
            sell_levels: BTreeMap::new(),
            buy_orders_at_level: BTreeMap::new(),
            sell_orders_at_level: BTreeMap::new(),
            orders: HashMap::new(),
            next_trade_id: 1,
        }
    }
    
    pub fn add_order(&mut self, order_id: u32, price: f64, quantity: u32, is_buy_side: bool) -> Vec<Trade> {
        let mut trades = Vec::new();
        let mut remaining_quantity = quantity;
        let price_key = (price * 100.0) as u64;
        
        // Matching logic
        if is_buy_side {
            // Match with sell orders
            let mut levels_to_update = Vec::new();
            
            // Collect price levels to process
            let sell_prices: Vec<u64> = self.sell_levels.keys().copied().collect();
            
            for sell_price_key in sell_prices {
                if remaining_quantity == 0 || price < (sell_price_key as f64 / 100.0) {
                    break;
                }
                
                let match_price = sell_price_key as f64 / 100.0;
                let order_ids = self.sell_orders_at_level.get(&sell_price_key).cloned().unwrap_or_default();
                let mut orders_to_remove = Vec::new();
                
                for &passive_order_id in &order_ids {
                    if remaining_quantity == 0 {
                        break;
                    }
                    
                    if let Some(passive_order) = self.orders.get_mut(&passive_order_id) {
                        let trade_quantity = min(remaining_quantity, passive_order.quantity);
                        
                        // Create trade
                        trades.push(Trade::new(
                            self.next_trade_id,
                            match_price,
                            trade_quantity,
                            order_id,
                            passive_order_id,
                        ));
                        self.next_trade_id += 1;
                        
                        // Update quantities
                        remaining_quantity -= trade_quantity;
                        passive_order.quantity -= trade_quantity;
                        
                        if passive_order.quantity == 0 {
                            orders_to_remove.push(passive_order_id);
                        }
                    }
                }
                
                // Remove filled orders
                for &order_to_remove in &orders_to_remove {
                    self.orders.remove(&order_to_remove);
                    if let Some(order_list) = self.sell_orders_at_level.get_mut(&sell_price_key) {
                        order_list.retain(|&id| id != order_to_remove);
                    }
                }
                
                // Calculate remaining level quantity
                let level_quantity: u32 = order_ids.iter()
                    .filter(|&&id| !orders_to_remove.contains(&id))
                    .filter_map(|&id| self.orders.get(&id))
                    .map(|o| o.quantity)
                    .sum();
                
                levels_to_update.push((sell_price_key, level_quantity));
            }
            
            // Update levels after iteration
            for (price_key, quantity) in levels_to_update {
                if quantity == 0 {
                    self.sell_levels.remove(&price_key);
                    self.sell_orders_at_level.remove(&price_key);
                } else {
                    self.sell_levels.insert(price_key, quantity);
                }
            }
        } else {
            // Match with buy orders
            let mut levels_to_update = Vec::new();
            
            // Collect price levels to process
            let buy_prices: Vec<Reverse<u64>> = self.buy_levels.keys().copied().collect();
            
            for Reverse(buy_price_key) in buy_prices {
                if remaining_quantity == 0 || price > (buy_price_key as f64 / 100.0) {
                    break;
                }
                
                let match_price = buy_price_key as f64 / 100.0;
                let order_ids = self.buy_orders_at_level.get(&Reverse(buy_price_key)).cloned().unwrap_or_default();
                let mut orders_to_remove = Vec::new();
                
                for &passive_order_id in &order_ids {
                    if remaining_quantity == 0 {
                        break;
                    }
                    
                    if let Some(passive_order) = self.orders.get_mut(&passive_order_id) {
                        let trade_quantity = min(remaining_quantity, passive_order.quantity);
                        
                        // Create trade
                        trades.push(Trade::new(
                            self.next_trade_id,
                            match_price,
                            trade_quantity,
                            passive_order_id,
                            order_id,
                        ));
                        self.next_trade_id += 1;
                        
                        // Update quantities
                        remaining_quantity -= trade_quantity;
                        passive_order.quantity -= trade_quantity;
                        
                        if passive_order.quantity == 0 {
                            orders_to_remove.push(passive_order_id);
                        }
                    }
                }
                
                // Remove filled orders
                for &order_to_remove in &orders_to_remove {
                    self.orders.remove(&order_to_remove);
                    if let Some(order_list) = self.buy_orders_at_level.get_mut(&Reverse(buy_price_key)) {
                        order_list.retain(|&id| id != order_to_remove);
                    }
                }
                
                // Calculate remaining level quantity
                let level_quantity: u32 = order_ids.iter()
                    .filter(|&&id| !orders_to_remove.contains(&id))
                    .filter_map(|&id| self.orders.get(&id))
                    .map(|o| o.quantity)
                    .sum();
                
                levels_to_update.push((Reverse(buy_price_key), level_quantity));
            }
            
            // Update levels after iteration
            for (price_key, quantity) in levels_to_update {
                if quantity == 0 {
                    self.buy_levels.remove(&price_key);
                    self.buy_orders_at_level.remove(&price_key);
                } else {
                    self.buy_levels.insert(price_key, quantity);
                }
            }
        }
        
        // Add remaining quantity to book if not fully matched
        if remaining_quantity > 0 {
            let order = Order::new(order_id, price, remaining_quantity, is_buy_side);
            
            if is_buy_side {
                *self.buy_levels.entry(Reverse(price_key)).or_insert(0) += remaining_quantity;
                self.buy_orders_at_level.entry(Reverse(price_key)).or_default().push(order_id);
            } else {
                *self.sell_levels.entry(price_key).or_insert(0) += remaining_quantity;
                self.sell_orders_at_level.entry(price_key).or_default().push(order_id);
            }
            
            self.orders.insert(order_id, order);
        }
        
        trades
    }
    
    pub fn cancel_order(&mut self, order_id: u32) -> bool {
        if let Some(order) = self.orders.remove(&order_id) {
            let price_key = (order.price * 100.0) as u64;
            
            if order.is_buy_side {
                if let Some(level) = self.buy_levels.get_mut(&Reverse(price_key)) {
                    *level = level.saturating_sub(order.quantity);
                    if *level == 0 {
                        self.buy_levels.remove(&Reverse(price_key));
                    }
                }
                
                if let Some(order_list) = self.buy_orders_at_level.get_mut(&Reverse(price_key)) {
                    order_list.retain(|&id| id != order_id);
                    if order_list.is_empty() {
                        self.buy_orders_at_level.remove(&Reverse(price_key));
                    }
                }
            } else {
                if let Some(level) = self.sell_levels.get_mut(&price_key) {
                    *level = level.saturating_sub(order.quantity);
                    if *level == 0 {
                        self.sell_levels.remove(&price_key);
                    }
                }
                
                if let Some(order_list) = self.sell_orders_at_level.get_mut(&price_key) {
                    order_list.retain(|&id| id != order_id);
                    if order_list.is_empty() {
                        self.sell_orders_at_level.remove(&price_key);
                    }
                }
            }
            true
        } else {
            false
        }
    }
    
    pub fn get_best_bid(&self) -> Option<f64> {
        self.buy_levels
            .first_key_value()
            .map(|(Reverse(price_key), _)| *price_key as f64 / 100.0)
    }
    
    pub fn get_best_ask(&self) -> Option<f64> {
        self.sell_levels
            .first_key_value()
            .map(|(price_key, _)| *price_key as f64 / 100.0)
    }
    
    pub fn get_bid_quantity_at(&self, price: f64) -> u32 {
        let price_key = (price * 100.0) as u64;
        self.buy_levels
            .get(&Reverse(price_key))
            .copied()
            .unwrap_or(0)
    }
    
    pub fn get_ask_quantity_at(&self, price: f64) -> u32 {
        let price_key = (price * 100.0) as u64;
        self.sell_levels
            .get(&price_key)
            .copied()
            .unwrap_or(0)
    }
}