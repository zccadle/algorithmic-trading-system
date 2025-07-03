use std::collections::{BTreeMap, HashMap};
use std::cmp::Reverse;

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
    buy_levels: BTreeMap<Reverse<u64>, u32>,  // Price (as fixed point) -> Total quantity
    sell_levels: BTreeMap<u64, u32>,           // Price (as fixed point) -> Total quantity
    orders: HashMap<u32, Order>,               // Order ID -> Order details
}

impl OrderBook {
    pub fn new() -> Self {
        OrderBook {
            buy_levels: BTreeMap::new(),
            sell_levels: BTreeMap::new(),
            orders: HashMap::new(),
        }
    }
    
    pub fn add_order(&mut self, order_id: u32, price: f64, quantity: u32, is_buy_side: bool) {
        let order = Order::new(order_id, price, quantity, is_buy_side);
        let price_key = (price * 100.0) as u64;  // Convert to fixed point for accurate comparison
        
        if is_buy_side {
            *self.buy_levels.entry(Reverse(price_key)).or_insert(0) += quantity;
        } else {
            *self.sell_levels.entry(price_key).or_insert(0) += quantity;
        }
        
        self.orders.insert(order_id, order);
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
            } else {
                if let Some(level) = self.sell_levels.get_mut(&price_key) {
                    *level = level.saturating_sub(order.quantity);
                    if *level == 0 {
                        self.sell_levels.remove(&price_key);
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