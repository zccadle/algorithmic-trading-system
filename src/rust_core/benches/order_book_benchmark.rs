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

criterion_group!(benches, benchmark_add_orders, benchmark_mixed_operations, benchmark_best_price_queries);
criterion_main!(benches);