// Profiling harness for the matching engine
use rust_core::order_book::OrderBook;

fn main() {
    println!("Starting profiling run...");
    let start = std::time::Instant::now();
    
    // Run the matching engine scenario many more times for profiling
    for i in 0..1000 {
        if i % 100 == 0 {
            println!("Iteration {i}/1000");
        }
        run_matching_engine_scenario();
    }
    
    let duration = start.elapsed();
    println!("Completed in {duration:?}");
}

fn run_matching_engine_scenario() {
    let mut book = OrderBook::new();
    
    // Add initial orders (same as benchmark)
    for i in 0..10 {
        book.add_order(i * 2, 100.0 + i as f64, 100, true);      // Bids
        book.add_order(i * 2 + 1, 110.0 + i as f64, 100, false); // Asks
    }
    
    // Add crossing orders to trigger matches
    for i in 0..50 {
        let trades = book.add_order(1000 + i, 109.0, 50, true);  // Buy order that crosses
        // Force some work to happen
        drop(trades);
        
        let trades = book.add_order(2000 + i, 101.0, 50, false); // Sell order that crosses
        drop(trades);
    }
    
    // Add more regular orders
    for i in 0..100 {
        book.add_order(3000 + i, 95.0 + (i % 10) as f64, 100, true);
        book.add_order(4000 + i, 115.0 + (i % 10) as f64, 100, false);
    }
}