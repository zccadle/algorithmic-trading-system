use rust_core::order_book::OrderBook;
use std::time::{Duration, Instant};

fn main() {
    println!("=== Rust Detailed Performance Analysis ===\n");

    // Warm up
    for _ in 0..10 {
        run_matching_engine_scenario();
    }

    // Test 1: Order insertion performance
    let mut timings = Vec::new();
    for _ in 0..100 {
        let start = Instant::now();
        let mut book = OrderBook::new();
        for i in 0..1000 {
            book.add_order(i, 100.0 + (i % 20) as f64, 10, i.is_multiple_of(&2));
        }
        timings.push(start.elapsed());
    }
    print_stats("Order Insertion (1000 orders)", &timings);

    // Test 2: Matching engine performance
    timings.clear();
    for _ in 0..100 {
        let start = Instant::now();
        run_matching_engine_scenario();
        timings.push(start.elapsed());
    }
    print_stats("Matching Engine Scenario", &timings);

    // Test 3: Best price queries
    let book = setup_book();
    timings.clear();
    for _ in 0..10000 {
        let start = Instant::now();
        let _ = book.get_best_bid();
        let _ = book.get_best_ask();
        timings.push(start.elapsed());
    }
    print_stats("Best Price Queries", &timings);

    // Test 4: Memory allocation patterns
    println!("\n--- Memory Allocation Test ---");
    let start = Instant::now();
    let mut books = Vec::new();
    for i in 0..100 {
        let mut book = OrderBook::new();
        for j in 0..100 {
            book.add_order(i * 100 + j, 100.0 + j as f64, 10, j.is_multiple_of(&2));
        }
        books.push(book);
    }
    println!(
        "Created 100 order books with 100 orders each in {:?}",
        start.elapsed()
    );

    // Force deallocation
    let start = Instant::now();
    drop(books);
    println!("Deallocated all books in {:?}", start.elapsed());
}

fn setup_book() -> OrderBook {
    let mut book = OrderBook::new();
    for i in 0..100 {
        book.add_order(i * 2, 100.0 + i as f64, 100, true);
        book.add_order(i * 2 + 1, 110.0 + i as f64, 100, false);
    }
    book
}

fn run_matching_engine_scenario() {
    let mut book = OrderBook::new();

    // Add initial orders
    for i in 0..10 {
        book.add_order(i * 2, 100.0 + i as f64, 100, true);
        book.add_order(i * 2 + 1, 110.0 + i as f64, 100, false);
    }

    // Add crossing orders
    for i in 0..50 {
        let _ = book.add_order(1000 + i, 109.0, 50, true);
        let _ = book.add_order(2000 + i, 101.0, 50, false);
    }
}

fn print_stats(name: &str, timings: &[Duration]) {
    let sum: Duration = timings.iter().sum();
    let avg = sum / timings.len() as u32;
    let min = timings.iter().min().unwrap();
    let max = timings.iter().max().unwrap();

    // Calculate percentiles
    let mut sorted = timings.to_vec();
    sorted.sort();
    let p50 = sorted[sorted.len() / 2];
    let p95 = sorted[sorted.len() * 95 / 100];
    let p99 = sorted[sorted.len() * 99 / 100];

    println!("\n--- {name} ---");
    println!("Samples: {}", timings.len());
    println!("Average: {avg:?}");
    println!("Min: {min:?}");
    println!("Max: {max:?}");
    println!("P50: {p50:?}");
    println!("P95: {p95:?}");
    println!("P99: {p99:?}");
}
