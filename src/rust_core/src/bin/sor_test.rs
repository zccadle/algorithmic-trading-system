use rust_core::order_book::OrderBook;
use rust_core::smart_order_router::{
    Exchange, ExchangeID, ExchangeMetrics, FeeSchedule, SmartOrderRouter,
};

// Mock exchange implementation
struct MockExchange {
    id: ExchangeID,
    name: String,
    order_book: OrderBook,
    metrics: ExchangeMetrics,
    is_available: bool,
}

impl MockExchange {
    fn new(id: ExchangeID, name: String, metrics: ExchangeMetrics) -> Self {
        MockExchange {
            id,
            name,
            order_book: OrderBook::new(),
            metrics,
            is_available: true,
        }
    }
}

impl Exchange for MockExchange {
    fn get_order_book(&self) -> &OrderBook {
        &self.order_book
    }

    fn get_order_book_mut(&mut self) -> &mut OrderBook {
        &mut self.order_book
    }

    fn get_id(&self) -> ExchangeID {
        self.id
    }

    fn get_name(&self) -> &str {
        &self.name
    }

    fn is_available(&self) -> bool {
        self.is_available
    }

    fn get_metrics(&self) -> ExchangeMetrics {
        self.metrics.clone()
    }
}

fn print_routing_decision(decision: &rust_core::smart_order_router::RoutingDecision, order_type: &str) {
    println!("\n{} Routing Decision:", order_type);
    println!("  Best Exchange: {}", decision.exchange_id);
    println!("  Expected Price: ${:.2}", decision.expected_price);
    println!(
        "  Expected Fee: ${:.2} ({})",
        decision.expected_fee,
        if decision.is_maker { "Maker" } else { "Taker" }
    );
    println!("  Total Cost/Proceeds: ${:.2}", decision.total_cost);
    println!("  Available Quantity: {}", decision.available_quantity);
}

fn main() {
    println!("=== Smart Order Router Test (Rust) ===");

    // Create mock exchanges with different characteristics
    let mut binance = MockExchange::new(
        ExchangeID::Binance,
        "Binance".to_string(),
        ExchangeMetrics::new(5, 0.98, 0.999), // 5ms latency, 98% fill rate
    );

    let mut coinbase = MockExchange::new(
        ExchangeID::Coinbase,
        "Coinbase".to_string(),
        ExchangeMetrics::new(15, 0.95, 0.998), // 15ms latency, 95% fill rate
    );

    let mut kraken = MockExchange::new(
        ExchangeID::Kraken,
        "Kraken".to_string(),
        ExchangeMetrics::new(25, 0.92, 0.997), // 25ms latency, 92% fill rate
    );

    // Populate order books with different prices
    println!("\n1. Setting up mock order books...");

    // Binance: Tight spread, high liquidity
    binance.get_order_book_mut().add_order(1, 45000.00, 10, true);  // Buy
    binance.get_order_book_mut().add_order(2, 44999.50, 5, true);   // Buy
    binance.get_order_book_mut().add_order(3, 45001.00, 8, false);  // Sell
    binance.get_order_book_mut().add_order(4, 45001.50, 12, false); // Sell
    println!("  Binance: Bid $45000.00, Ask $45001.00 (Spread: $1.00)");

    // Coinbase: Wider spread, medium liquidity
    coinbase.get_order_book_mut().add_order(5, 44999.00, 7, true);  // Buy
    coinbase.get_order_book_mut().add_order(6, 44998.00, 3, true);  // Buy
    coinbase.get_order_book_mut().add_order(7, 45002.00, 6, false); // Sell
    coinbase.get_order_book_mut().add_order(8, 45003.00, 9, false); // Sell
    println!("  Coinbase: Bid $44999.00, Ask $45002.00 (Spread: $3.00)");

    // Kraken: Best bid, higher ask
    kraken.get_order_book_mut().add_order(9, 45000.50, 15, true);   // Buy (best bid)
    kraken.get_order_book_mut().add_order(10, 45000.00, 5, true);   // Buy
    kraken.get_order_book_mut().add_order(11, 45002.50, 10, false); // Sell
    kraken.get_order_book_mut().add_order(12, 45003.50, 8, false);  // Sell
    println!("  Kraken: Bid $45000.50, Ask $45002.50 (Spread: $2.00)");

    // Create Smart Order Router
    let mut sor = SmartOrderRouter::new(true, true); // Consider both latency and fees

    // Add exchanges with different fee structures
    sor.add_exchange(Box::new(binance), FeeSchedule::new(0.0010, 0.0010));   // 0.10% maker/taker
    sor.add_exchange(Box::new(coinbase), FeeSchedule::new(0.0005, 0.0015));  // 0.05% maker, 0.15% taker
    sor.add_exchange(Box::new(kraken), FeeSchedule::new(0.0002, 0.0012));    // 0.02% maker, 0.12% taker

    // Test 1: Route a market buy order
    println!("\n2. Testing Buy Order Routing");
    println!("   Order: BUY 5 BTC at market");

    let buy_decision = sor.route_order(101, 50000.0, 5, true);
    print_routing_decision(&buy_decision, "Buy");

    // Test 2: Route a market sell order
    println!("\n3. Testing Sell Order Routing");
    println!("   Order: SELL 5 BTC at market");

    let sell_decision = sor.route_order(102, 40000.0, 5, false);
    print_routing_decision(&sell_decision, "Sell");

    // Test 3: Route a large order that needs splitting
    println!("\n4. Testing Large Order Splitting");
    println!("   Order: BUY 20 BTC at market");

    let splits = sor.route_order_split(103, 50000.0, 20, true);
    println!("\n   Order split across {} exchanges:", splits.len());
    let mut total_cost = 0.0;
    for split in &splits {
        println!(
            "   - {}: {} BTC @ ${:.2} (Fee: ${:.2})",
            split.exchange_id, split.quantity, split.expected_price, split.expected_fee
        );
        total_cost += (split.expected_price * split.quantity as f64) + split.expected_fee;
    }
    println!("   Total Cost: ${:.2}", total_cost);

    // Test 4: Show routing statistics
    sor.print_routing_stats();

    // Test 5: Disable an exchange and re-route
    println!("\n5. Testing Exchange Failover");
    println!("   Disabling Binance...");
    sor.set_exchange_active(ExchangeID::Binance, false);

    let failover_decision = sor.route_order(104, 50000.0, 5, true);
    println!("   New routing decision after Binance disabled:");
    print_routing_decision(&failover_decision, "Failover Buy");

    // Test 6: Compare with/without fee consideration
    println!("\n6. Testing Fee Impact on Routing");

    let mut sor_no_fees = SmartOrderRouter::new(true, false); // Don't consider fees

    // Re-create exchanges for the no-fee router
    let mut binance2 = MockExchange::new(
        ExchangeID::Binance,
        "Binance".to_string(),
        ExchangeMetrics::default(),
    );
    let mut coinbase2 = MockExchange::new(
        ExchangeID::Coinbase,
        "Coinbase".to_string(),
        ExchangeMetrics::default(),
    );
    let mut kraken2 = MockExchange::new(
        ExchangeID::Kraken,
        "Kraken".to_string(),
        ExchangeMetrics::default(),
    );

    // Same order books
    binance2.get_order_book_mut().add_order(1, 45000.00, 10, true);
    binance2.get_order_book_mut().add_order(3, 45001.00, 8, false);
    coinbase2.get_order_book_mut().add_order(5, 44999.00, 7, true);
    coinbase2.get_order_book_mut().add_order(7, 45002.00, 6, false);
    kraken2.get_order_book_mut().add_order(9, 45000.50, 15, true);
    kraken2.get_order_book_mut().add_order(11, 45002.50, 10, false);

    sor_no_fees.add_exchange(Box::new(binance2), FeeSchedule::new(0.0010, 0.0010));
    sor_no_fees.add_exchange(Box::new(coinbase2), FeeSchedule::new(0.0005, 0.0015));
    sor_no_fees.add_exchange(Box::new(kraken2), FeeSchedule::new(0.0002, 0.0012));

    let no_fee_decision = sor_no_fees.route_order(105, 50000.0, 5, true);
    println!(
        "   Without fee consideration: Route to {} @ ${:.2}",
        no_fee_decision.exchange_id, no_fee_decision.expected_price
    );
    println!(
        "   With fee consideration: Route to {} @ ${:.2} (Total: ${:.2})",
        buy_decision.exchange_id, buy_decision.expected_price, buy_decision.total_cost
    );

    // Test 7: Rust-specific - Demonstrate trait object flexibility
    println!("\n7. Rust-Specific Feature: Dynamic Exchange Types");
    println!("   The Rust implementation uses trait objects (Box<dyn Exchange>)");
    println!("   This allows runtime polymorphism without inheritance");
    println!("   Each exchange can have different internal implementations");
    println!("   while conforming to the same Exchange trait interface.");
}