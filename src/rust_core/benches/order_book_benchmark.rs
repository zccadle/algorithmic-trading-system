use criterion::{black_box, criterion_group, criterion_main, Criterion};
use rand::prelude::*;
use rust_core::order_book::OrderBook;

fn benchmark_add_orders(c: &mut Criterion) {
    let mut rng = StdRng::seed_from_u64(42); // Deterministic seed for reproducibility
    
    c.bench_function("add_10k_orders", |b| {
        b.iter(|| {
            let mut book = OrderBook::new();
            
            for i in 0..10_000 {
                let price = 100.0 + (rng.gen::<f64>() * 10.0);
                let quantity = rng.gen_range(1..100);
                let is_buy = rng.gen_bool(0.5);
                
                book.add_order(i, black_box(price), black_box(quantity), black_box(is_buy));
            }
        });
    });
}

fn benchmark_mixed_operations(c: &mut Criterion) {
    let mut rng = StdRng::seed_from_u64(42);
    
    c.bench_function("mixed_10k_operations", |b| {
        b.iter(|| {
            let mut book = OrderBook::new();
            let mut order_ids = Vec::new();
            
            for i in 0..10_000 {
                let price = 100.0 + (rng.gen::<f64>() * 10.0);
                let quantity = rng.gen_range(1..100);
                let is_buy = rng.gen_bool(0.5);
                
                if rng.gen_bool(0.8) || order_ids.is_empty() {
                    // 80% add orders
                    book.add_order(i, price, quantity, is_buy);
                    order_ids.push(i);
                } else {
                    // 20% cancel orders
                    let idx = rng.gen_range(0..order_ids.len());
                    let order_id = order_ids.swap_remove(idx);
                    book.cancel_order(order_id);
                }
            }
        });
    });
}

fn benchmark_best_price_queries(c: &mut Criterion) {
    let mut rng = StdRng::seed_from_u64(42);
    let mut book = OrderBook::new();
    
    // Pre-populate order book
    for i in 0..1000 {
        let price = 100.0 + (rng.gen::<f64>() * 10.0);
        let quantity = rng.gen_range(1..100);
        let is_buy = rng.gen_bool(0.5);
        book.add_order(i, price, quantity, is_buy);
    }
    
    c.bench_function("best_price_queries", |b| {
        b.iter(|| {
            for _ in 0..1000 {
                black_box(book.get_best_bid());
                black_box(book.get_best_ask());
            }
        });
    });
}

fn benchmark_matching_engine(c: &mut Criterion) {
    let mut rng = StdRng::seed_from_u64(42); // Deterministic seed for reproducibility
    
    c.bench_function("matching_engine", |b| {
        b.iter(|| {
            let mut book = OrderBook::new();
            let mut total_trades = 0;
            
            // Pre-populate the book with 1000 orders to create liquidity
            // Create a tighter spread for more realistic matching
            for i in 0..1000 {
                let base_price = 100.0;
                let spread = 0.05; // 5 cent spread
                
                if i % 2 == 0 {
                    // Buy orders: 99.50 to 99.95
                    let price = base_price - spread - (i % 10) as f64 * 0.01;
                    book.add_order(i, price, 100, true);
                } else {
                    // Sell orders: 100.05 to 100.50
                    let price = base_price + spread + (i % 10) as f64 * 0.01;
                    book.add_order(i, price, 100, false);
                }
            }
            
            // Add 1000 aggressive "market-crossing" orders
            for i in 1000..2000 {
                let quantity = rng.gen_range(50..=150);
                
                if rng.gen::<f64>() < 0.5 {
                    // Aggressive buy order (crosses the spread)
                    let price = 100.10 + rng.gen::<f64>() * 0.40; // 100.10 to 100.50
                    let trades = book.add_order(i, price, quantity, true);
                    total_trades += trades.len();
                } else {
                    // Aggressive sell order (crosses the spread)
                    let price = 99.90 - rng.gen::<f64>() * 0.40; // 99.50 to 99.90
                    let trades = book.add_order(i, price, quantity, false);
                    total_trades += trades.len();
                }
            }
            
            black_box(total_trades);
        });
    });
}

criterion_group!(benches, benchmark_add_orders, benchmark_mixed_operations, benchmark_best_price_queries, benchmark_matching_engine);
criterion_main!(benches);