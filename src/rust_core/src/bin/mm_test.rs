use rand::prelude::*;
use rust_core::market_maker::{MarketMaker, MarketMakerParameters};
use rust_core::order_book::OrderBook;
use rust_core::smart_order_router::{
    Exchange, ExchangeID, ExchangeMetrics, FeeSchedule, SmartOrderRouter,
};
use std::thread;
use std::time::Duration;

// Mock exchange implementation (same as in sor_test.rs)
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

fn setup_market_data(
    binance: &mut MockExchange,
    coinbase: &mut MockExchange,
    kraken: &mut MockExchange,
) {
    // Clear existing orders
    *binance.get_order_book_mut() = OrderBook::new();
    *coinbase.get_order_book_mut() = OrderBook::new();
    *kraken.get_order_book_mut() = OrderBook::new();

    // Binance: Tight spread
    binance
        .get_order_book_mut()
        .add_order(1, 45000.00, 10, true); // Buy
    binance.get_order_book_mut().add_order(2, 44999.50, 5, true); // Buy
    binance
        .get_order_book_mut()
        .add_order(3, 45001.00, 8, false); // Sell
    binance
        .get_order_book_mut()
        .add_order(4, 45001.50, 12, false); // Sell

    // Coinbase: Wider spread
    coinbase
        .get_order_book_mut()
        .add_order(5, 44999.00, 7, true); // Buy
    coinbase
        .get_order_book_mut()
        .add_order(6, 44998.00, 3, true); // Buy
    coinbase
        .get_order_book_mut()
        .add_order(7, 45002.00, 6, false); // Sell
    coinbase
        .get_order_book_mut()
        .add_order(8, 45003.00, 9, false); // Sell

    // Kraken: Different prices
    kraken.get_order_book_mut().add_order(9, 45000.50, 15, true); // Buy
    kraken.get_order_book_mut().add_order(10, 45000.00, 5, true); // Buy
    kraken
        .get_order_book_mut()
        .add_order(11, 45002.50, 10, false); // Sell
    kraken
        .get_order_book_mut()
        .add_order(12, 45003.50, 8, false); // Sell
}

#[allow(dead_code)]
fn simulate_market_movement(exchange: &mut MockExchange, rng: &mut ThreadRng) {
    // Add some randomness to the market
    let price_change = rng.gen_range(-5.0..5.0);
    let size_change = rng.gen_range(-2..3);

    // Update best bid/ask
    let book = exchange.get_order_book_mut();
    if let Some(best_bid) = book.get_best_bid() {
        book.cancel_order(1); // Cancel old best bid
        book.add_order(
            1,
            best_bid + price_change,
            (10 + size_change).max(1) as u32,
            true,
        );
    }

    if let Some(best_ask) = book.get_best_ask() {
        book.cancel_order(3); // Cancel old best ask
        book.add_order(
            3,
            best_ask + price_change,
            (8 + size_change).max(1) as u32,
            false,
        );
    }
}

fn main() {
    println!("=== Market Maker Test (Rust) ===");

    // Create mock exchanges
    let mut binance = MockExchange::new(
        ExchangeID::Binance,
        "Binance".to_string(),
        ExchangeMetrics::new(5, 0.98, 0.999),
    );

    let mut coinbase = MockExchange::new(
        ExchangeID::Coinbase,
        "Coinbase".to_string(),
        ExchangeMetrics::new(15, 0.95, 0.998),
    );

    let mut kraken = MockExchange::new(
        ExchangeID::Kraken,
        "Kraken".to_string(),
        ExchangeMetrics::new(25, 0.92, 0.997),
    );

    // Setup initial market data
    setup_market_data(&mut binance, &mut coinbase, &mut kraken);

    // Create Smart Order Router
    let mut sor = SmartOrderRouter::new(true, true);
    sor.add_exchange(Box::new(binance), FeeSchedule::new(0.0010, 0.0010));
    sor.add_exchange(Box::new(coinbase), FeeSchedule::new(0.0005, 0.0015));
    sor.add_exchange(Box::new(kraken), FeeSchedule::new(0.0002, 0.0012));

    // Create Market Maker with custom parameters
    let params = MarketMakerParameters {
        base_spread_bps: 20.0,      // 0.20% spread
        base_quote_size: 0.5,       // 0.5 BTC per quote
        target_base_inventory: 5.0, // Target 5 BTC
        inventory_skew_factor: 0.2, // 20% skew adjustment
        ..Default::default()
    };

    let mut mm = MarketMaker::new(&sor, params);

    // Initialize with starting inventory
    let starting_btc = 5.0;
    let starting_usd = 250000.0;
    mm.initialize(starting_btc, starting_usd);

    // Test 1: Generate initial quotes
    println!("\n1. Generating Initial Quotes");
    println!("{}", "=".repeat(50));

    if let Some(quotes) = mm.update_quotes() {
        println!("Buy Quote:");
        println!("  Price: ${:.2}", quotes.buy_quote.price);
        println!(
            "  Size: {:.2} BTC",
            quotes.buy_quote.quantity as f64 / 100.0
        );
        println!("  Exchange: {}", quotes.buy_quote.target_exchange);

        println!("\nSell Quote:");
        println!("  Price: ${:.2}", quotes.sell_quote.price);
        println!(
            "  Size: {:.2} BTC",
            quotes.sell_quote.quantity as f64 / 100.0
        );
        println!("  Exchange: {}", quotes.sell_quote.target_exchange);

        println!("\nTheoretical Edge: ${:.2}", quotes.theoretical_edge);

        // Test 2: Simulate a buy quote fill
        println!("\n2. Simulating Buy Quote Fill");
        println!("{}", "=".repeat(50));

        mm.on_quote_filled(
            &quotes.buy_quote,
            quotes.buy_quote.price,
            quotes.buy_quote.quantity,
        );

        let pos = mm.get_inventory_position();
        println!("Updated Inventory:");
        println!("  BTC: {:.2}", pos.base_inventory);
        println!("  USD: ${:.2}", pos.quote_inventory);
        println!("  Total Value: ${:.2}", pos.total_value);
        println!("  P&L: ${:.2}", pos.pnl);

        // Test 3: Generate new quotes with updated inventory
        println!("\n3. Generating Quotes with New Inventory");
        println!("{}", "=".repeat(50));

        if let Some(new_quotes) = mm.update_quotes() {
            println!("New quotes (notice inventory skew effect):");
            println!(
                "  Buy: ${:.2} for {:.2} BTC",
                new_quotes.buy_quote.price,
                new_quotes.buy_quote.quantity as f64 / 100.0
            );
            println!(
                "  Sell: ${:.2} for {:.2} BTC",
                new_quotes.sell_quote.price,
                new_quotes.sell_quote.quantity as f64 / 100.0
            );
            println!(
                "  Inventory imbalance: {:.1}%",
                mm.get_inventory_imbalance() * 100.0
            );
        }
    }

    // Test 4: Simulate multiple trades
    println!("\n4. Simulating Trading Session");
    println!("{}", "=".repeat(50));

    let mut rng = thread_rng();

    // Need mutable access to exchanges for market simulation
    // In a real system, this would be handled differently
    println!("(Note: Market simulation skipped in Rust version due to ownership constraints)");
    println!("(In production, exchanges would have separate update mechanisms)");

    for i in 0..10 {
        // Generate new quotes
        if let Some(quotes) = mm.update_quotes() {
            // Randomly fill some quotes
            if rng.gen::<f64>() < 0.3 {
                // 30% fill rate
                if rng.gen::<f64>() < 0.5 {
                    // Fill buy quote
                    mm.on_quote_filled(
                        &quotes.buy_quote,
                        quotes.buy_quote.price,
                        quotes.buy_quote.quantity,
                    );
                    println!(
                        "Trade {}: Bought {:.2} BTC @ ${:.2}",
                        i + 1,
                        quotes.buy_quote.quantity as f64 / 100.0,
                        quotes.buy_quote.price
                    );
                } else {
                    // Fill sell quote
                    mm.on_quote_filled(
                        &quotes.sell_quote,
                        quotes.sell_quote.price,
                        quotes.sell_quote.quantity,
                    );
                    println!(
                        "Trade {}: Sold {:.2} BTC @ ${:.2}",
                        i + 1,
                        quotes.sell_quote.quantity as f64 / 100.0,
                        quotes.sell_quote.price
                    );
                }
            }
        }

        // Brief pause to simulate time passing
        thread::sleep(Duration::from_millis(100));
    }

    // Test 5: Print final performance stats
    println!("\n5. Final Performance Report");
    println!("{}", "=".repeat(50));

    mm.print_performance_stats();

    // Test 6: Risk management demonstration
    println!("\n6. Risk Management Check");
    println!("{}", "=".repeat(50));

    println!(
        "Within risk limits: {}",
        if mm.is_within_risk_limits() {
            "YES"
        } else {
            "NO"
        }
    );

    // Force inventory imbalance
    println!("\nSimulating large inventory imbalance...");
    if let Some(quotes) = mm.update_quotes() {
        for _i in 0..5 {
            mm.on_quote_filled(&quotes.buy_quote, quotes.buy_quote.price, 100); // Buy 1 BTC each time
        }

        let pos = mm.get_inventory_position();
        println!("After buying 5 BTC:");
        println!("  BTC inventory: {:.2}", pos.base_inventory);
        println!(
            "  Inventory imbalance: {:.1}%",
            mm.get_inventory_imbalance() * 100.0
        );
        println!(
            "  Within risk limits: {}",
            if mm.is_within_risk_limits() {
                "YES"
            } else {
                "NO"
            }
        );

        // Generate quotes with high inventory
        if let Some(new_quotes) = mm.update_quotes() {
            println!("\nQuotes with high inventory (notice the skew):");
            println!(
                "  Buy: ${:.2} (smaller size: {:.2} BTC)",
                new_quotes.buy_quote.price,
                new_quotes.buy_quote.quantity as f64 / 100.0
            );
            println!(
                "  Sell: ${:.2} (larger size: {:.2} BTC)",
                new_quotes.sell_quote.price,
                new_quotes.sell_quote.quantity as f64 / 100.0
            );
        }
    }

    // Test 7: Rust-specific features
    println!("\n7. Rust-Specific Features");
    println!("{}", "=".repeat(50));
    println!("The Rust implementation showcases:");
    println!("  - Lifetime annotations ('a) for safe references to SOR");
    println!("  - Option<T> for fallible operations (update_quotes returns Option)");
    println!("  - Ownership model prevents data races in concurrent scenarios");
    println!("  - Pattern matching for elegant error handling");
    println!("  - No manual memory management while maintaining performance");
}
