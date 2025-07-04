use crate::order_book::OrderBook;
use std::fmt;
use std::time::Duration;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ExchangeID {
    Binance,
    Coinbase,
    Kraken,
    FTX,
    Unknown,
}

impl fmt::Display for ExchangeID {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ExchangeID::Binance => write!(f, "Binance"),
            ExchangeID::Coinbase => write!(f, "Coinbase"),
            ExchangeID::Kraken => write!(f, "Kraken"),
            ExchangeID::FTX => write!(f, "FTX"),
            ExchangeID::Unknown => write!(f, "Unknown"),
        }
    }
}

#[derive(Debug, Clone)]
pub struct FeeSchedule {
    pub maker_fee: f64, // Fee as percentage (e.g., 0.001 = 0.1%)
    pub taker_fee: f64, // Fee as percentage
}

impl FeeSchedule {
    pub fn new(maker: f64, taker: f64) -> Self {
        FeeSchedule {
            maker_fee: maker,
            taker_fee: taker,
        }
    }
}

impl Default for FeeSchedule {
    fn default() -> Self {
        FeeSchedule {
            maker_fee: 0.001,
            taker_fee: 0.002,
        }
    }
}

#[derive(Debug, Clone)]
pub struct RoutingDecision {
    pub exchange_id: ExchangeID,
    pub expected_price: f64,
    pub expected_fee: f64,
    pub total_cost: f64, // For buys: price + fee, For sells: price - fee
    pub available_quantity: u32,
    pub is_maker: bool,
}

impl Default for RoutingDecision {
    fn default() -> Self {
        RoutingDecision {
            exchange_id: ExchangeID::Unknown,
            expected_price: 0.0,
            expected_fee: 0.0,
            total_cost: 0.0,
            available_quantity: 0,
            is_maker: false,
        }
    }
}

#[derive(Debug, Clone)]
pub struct ExchangeMetrics {
    pub avg_latency: Duration,
    pub fill_rate: f64, // Percentage of orders that get filled
    pub uptime: f64,    // Percentage uptime over last 24h
}

impl ExchangeMetrics {
    pub fn new(latency_ms: u64, fill_rate: f64, uptime: f64) -> Self {
        ExchangeMetrics {
            avg_latency: Duration::from_millis(latency_ms),
            fill_rate,
            uptime,
        }
    }
}

impl Default for ExchangeMetrics {
    fn default() -> Self {
        ExchangeMetrics {
            avg_latency: Duration::from_millis(10),
            fill_rate: 0.95,
            uptime: 0.999,
        }
    }
}

// Trait for exchanges - Rust's idiomatic way to define interfaces
pub trait Exchange: Send + Sync {
    fn get_order_book(&self) -> &OrderBook;
    fn get_order_book_mut(&mut self) -> &mut OrderBook;
    fn get_id(&self) -> ExchangeID;
    fn get_name(&self) -> &str;
    fn is_available(&self) -> bool {
        true
    }
    fn get_metrics(&self) -> ExchangeMetrics {
        ExchangeMetrics::default()
    }
}

// Container for exchange info
struct ExchangeInfo {
    exchange: Box<dyn Exchange>,
    fees: FeeSchedule,
    is_active: bool,
}

pub struct SmartOrderRouter {
    exchanges: Vec<ExchangeInfo>,
    consider_latency: bool,
    consider_fees: bool,
}

impl SmartOrderRouter {
    pub fn new(consider_latency: bool, consider_fees: bool) -> Self {
        SmartOrderRouter {
            exchanges: Vec::new(),
            consider_latency,
            consider_fees,
        }
    }

    pub fn add_exchange(&mut self, exchange: Box<dyn Exchange>, fees: FeeSchedule) {
        self.exchanges.push(ExchangeInfo {
            exchange,
            fees,
            is_active: true,
        });
    }

    // Calculate the effective cost for a buy order
    fn calculate_buy_cost(&self, price: f64, quantity: u32, fee_rate: f64) -> f64 {
        let notional = price * quantity as f64;
        let fee = notional * fee_rate;
        notional + fee // Total cost including fees
    }

    // Calculate the effective proceeds for a sell order
    fn calculate_sell_proceeds(&self, price: f64, quantity: u32, fee_rate: f64) -> f64 {
        let notional = price * quantity as f64;
        let fee = notional * fee_rate;
        notional - fee // Net proceeds after fees
    }

    // Check if this would be a maker or taker order
    fn would_be_maker_order(&self, book: &OrderBook, price: f64, is_buy: bool) -> bool {
        if is_buy {
            // Buy order is maker if price < best_ask
            match book.get_best_ask() {
                Some(best_ask) => price < best_ask,
                None => true,
            }
        } else {
            // Sell order is maker if price > best_bid
            match book.get_best_bid() {
                Some(best_bid) => price > best_bid,
                None => true,
            }
        }
    }

    pub fn route_order(
        &self,
        _order_id: u32,
        price: f64,
        quantity: u32,
        is_buy_side: bool,
    ) -> RoutingDecision {
        let mut best_decision = RoutingDecision::default();

        if is_buy_side {
            // For buy orders, find lowest effective cost (price + fees)
            let mut best_cost = f64::MAX;

            for exchange_info in &self.exchanges {
                if !exchange_info.is_active || !exchange_info.exchange.is_available() {
                    continue;
                }

                let book = exchange_info.exchange.get_order_book();
                let best_ask = match book.get_best_ask() {
                    Some(ask) => ask,
                    None => continue,
                };

                // Check available quantity
                let available_qty = book.get_ask_quantity_at(best_ask);
                if available_qty == 0 {
                    continue;
                }

                // Determine if maker or taker
                let is_maker = self.would_be_maker_order(book, price, is_buy_side);
                let fee_rate = if is_maker {
                    exchange_info.fees.maker_fee
                } else {
                    exchange_info.fees.taker_fee
                };

                // Calculate total cost
                let fill_qty = quantity.min(available_qty);
                let mut total_cost = if self.consider_fees {
                    self.calculate_buy_cost(best_ask, fill_qty, fee_rate)
                } else {
                    best_ask * fill_qty as f64
                };

                // Consider latency if enabled
                if self.consider_latency {
                    let metrics = exchange_info.exchange.get_metrics();
                    // Add a small penalty for high latency exchanges
                    total_cost *= 1.0 + metrics.avg_latency.as_millis() as f64 / 10000.0;
                }

                if total_cost < best_cost {
                    best_cost = total_cost;
                    best_decision = RoutingDecision {
                        exchange_id: exchange_info.exchange.get_id(),
                        expected_price: best_ask,
                        expected_fee: if self.consider_fees {
                            best_ask * fill_qty as f64 * fee_rate
                        } else {
                            0.0
                        },
                        total_cost,
                        available_quantity: available_qty,
                        is_maker,
                    };
                }
            }
        } else {
            // For sell orders, find highest effective proceeds (price - fees)
            let mut best_proceeds = f64::MIN;

            for exchange_info in &self.exchanges {
                if !exchange_info.is_active || !exchange_info.exchange.is_available() {
                    continue;
                }

                let book = exchange_info.exchange.get_order_book();
                let best_bid = match book.get_best_bid() {
                    Some(bid) => bid,
                    None => continue,
                };

                // Check available quantity
                let available_qty = book.get_bid_quantity_at(best_bid);
                if available_qty == 0 {
                    continue;
                }

                // Determine if maker or taker
                let is_maker = self.would_be_maker_order(book, price, is_buy_side);
                let fee_rate = if is_maker {
                    exchange_info.fees.maker_fee
                } else {
                    exchange_info.fees.taker_fee
                };

                // Calculate net proceeds
                let fill_qty = quantity.min(available_qty);
                let mut net_proceeds = if self.consider_fees {
                    self.calculate_sell_proceeds(best_bid, fill_qty, fee_rate)
                } else {
                    best_bid * fill_qty as f64
                };

                // Consider latency if enabled
                if self.consider_latency {
                    let metrics = exchange_info.exchange.get_metrics();
                    // Reduce proceeds slightly for high latency exchanges
                    net_proceeds *= 1.0 - metrics.avg_latency.as_millis() as f64 / 10000.0;
                }

                if net_proceeds > best_proceeds {
                    best_proceeds = net_proceeds;
                    best_decision = RoutingDecision {
                        exchange_id: exchange_info.exchange.get_id(),
                        expected_price: best_bid,
                        expected_fee: if self.consider_fees {
                            best_bid * fill_qty as f64 * fee_rate
                        } else {
                            0.0
                        },
                        total_cost: net_proceeds,
                        available_quantity: available_qty,
                        is_maker,
                    };
                }
            }
        }

        best_decision
    }

    pub fn get_aggregated_market_data(&self) -> AggregatedMarketData {
        let mut data = AggregatedMarketData {
            best_bid: f64::MIN,
            best_ask: f64::MAX,
            total_bid_quantity: 0,
            total_ask_quantity: 0,
            best_bid_exchange: ExchangeID::Unknown,
            best_ask_exchange: ExchangeID::Unknown,
        };

        for exchange_info in &self.exchanges {
            if !exchange_info.is_active || !exchange_info.exchange.is_available() {
                continue;
            }

            let book = exchange_info.exchange.get_order_book();

            // Check best bid
            if let Some(bid) = book.get_best_bid() {
                if bid > data.best_bid {
                    data.best_bid = bid;
                    data.best_bid_exchange = exchange_info.exchange.get_id();
                }
                data.total_bid_quantity += book.get_bid_quantity_at(bid);
            }

            // Check best ask
            if let Some(ask) = book.get_best_ask() {
                if ask < data.best_ask {
                    data.best_ask = ask;
                    data.best_ask_exchange = exchange_info.exchange.get_id();
                }
                data.total_ask_quantity += book.get_ask_quantity_at(ask);
            }
        }

        data
    }

    pub fn route_order_split(
        &self,
        order_id: u32,
        price: f64,
        mut total_quantity: u32,
        is_buy_side: bool,
    ) -> Vec<SplitOrder> {
        let mut splits = Vec::new();

        // Keep routing portions until all quantity is allocated
        while total_quantity > 0 {
            let decision = self.route_order(order_id, price, total_quantity, is_buy_side);

            if decision.exchange_id == ExchangeID::Unknown {
                break; // No more liquidity available
            }

            let fill_quantity = total_quantity.min(decision.available_quantity);

            splits.push(SplitOrder {
                exchange_id: decision.exchange_id,
                quantity: fill_quantity,
                expected_price: decision.expected_price,
                expected_fee: decision.expected_fee * fill_quantity as f64
                    / decision.available_quantity as f64,
            });

            total_quantity -= fill_quantity;

            // Prevent infinite loop
            if splits.len() >= self.exchanges.len() {
                break;
            }
        }

        splits
    }

    pub fn set_exchange_active(&mut self, id: ExchangeID, active: bool) {
        for exchange_info in &mut self.exchanges {
            if exchange_info.exchange.get_id() == id {
                exchange_info.is_active = active;
                break;
            }
        }
    }

    pub fn print_routing_stats(&self) {
        println!("\n=== Smart Order Router Statistics ===");

        for exchange_info in &self.exchanges {
            let exchange = &exchange_info.exchange;
            let book = exchange.get_order_book();
            let metrics = exchange.get_metrics();

            println!(
                "\n{} (ID: {:?}) - {}",
                exchange.get_name(),
                exchange.get_id(),
                if exchange_info.is_active {
                    "ACTIVE"
                } else {
                    "INACTIVE"
                }
            );

            print!("  Best Bid: ");
            if let Some(bid) = book.get_best_bid() {
                print!("${:.2} (Qty: {})", bid, book.get_bid_quantity_at(bid));
            } else {
                print!("None");
            }

            print!(" | Best Ask: ");
            if let Some(ask) = book.get_best_ask() {
                println!("${:.2} (Qty: {})", ask, book.get_ask_quantity_at(ask));
            } else {
                println!("None");
            }

            println!(
                "  Fees: Maker {:.2}% / Taker {:.2}%",
                exchange_info.fees.maker_fee * 100.0,
                exchange_info.fees.taker_fee * 100.0
            );

            println!(
                "  Metrics: Latency {}ms, Fill Rate {:.1}%, Uptime {:.1}%",
                metrics.avg_latency.as_millis(),
                metrics.fill_rate * 100.0,
                metrics.uptime * 100.0
            );
        }

        let aggregated = self.get_aggregated_market_data();
        println!("\n=== Aggregated Market Data ===");
        println!(
            "Best Bid: ${:.2} on {} (Total Qty: {})",
            aggregated.best_bid, aggregated.best_bid_exchange, aggregated.total_bid_quantity
        );
        println!(
            "Best Ask: ${:.2} on {} (Total Qty: {})",
            aggregated.best_ask, aggregated.best_ask_exchange, aggregated.total_ask_quantity
        );
    }
}

#[derive(Debug)]
pub struct AggregatedMarketData {
    pub best_bid: f64,
    pub best_ask: f64,
    pub total_bid_quantity: u32,
    pub total_ask_quantity: u32,
    pub best_bid_exchange: ExchangeID,
    pub best_ask_exchange: ExchangeID,
}

#[derive(Debug)]
pub struct SplitOrder {
    pub exchange_id: ExchangeID,
    pub quantity: u32,
    pub expected_price: f64,
    pub expected_fee: f64,
}
