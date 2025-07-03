#include <benchmark/benchmark.h>
#include "order_book.h"
#include <random>

static void BM_AddOrders(benchmark::State& state) {
    std::mt19937 rng(42); // Deterministic seed for reproducibility
    std::uniform_real_distribution<double> price_dist(100.0, 110.0);
    std::uniform_int_distribution<int> quantity_dist(1, 99);
    std::uniform_int_distribution<int> side_dist(0, 1);
    
    for (auto _ : state) {
        OrderBook book;
        
        for (int i = 0; i < 10000; ++i) {
            double price = price_dist(rng);
            int quantity = quantity_dist(rng);
            bool is_buy = side_dist(rng) == 1;
            
            book.add_order(i, price, quantity, is_buy);
        }
        
        benchmark::DoNotOptimize(book);
    }
}

static void BM_MixedOperations(benchmark::State& state) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> price_dist(100.0, 110.0);
    std::uniform_int_distribution<int> quantity_dist(1, 99);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_real_distribution<double> operation_dist(0.0, 1.0);
    
    for (auto _ : state) {
        OrderBook book;
        std::vector<int> order_ids;
        
        for (int i = 0; i < 10000; ++i) {
            double price = price_dist(rng);
            int quantity = quantity_dist(rng);
            bool is_buy = side_dist(rng) == 1;
            
            if (operation_dist(rng) < 0.8 || order_ids.empty()) {
                // 80% add orders
                book.add_order(i, price, quantity, is_buy);
                order_ids.push_back(i);
            } else {
                // 20% cancel orders
                std::uniform_int_distribution<size_t> idx_dist(0, order_ids.size() - 1);
                size_t idx = idx_dist(rng);
                int order_id = order_ids[idx];
                order_ids.erase(order_ids.begin() + idx);
                book.cancel_order(order_id);
            }
        }
        
        benchmark::DoNotOptimize(book);
    }
}

static void BM_BestPriceQueries(benchmark::State& state) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> price_dist(100.0, 110.0);
    std::uniform_int_distribution<int> quantity_dist(1, 99);
    std::uniform_int_distribution<int> side_dist(0, 1);
    
    // Pre-populate order book
    OrderBook book;
    for (int i = 0; i < 1000; ++i) {
        double price = price_dist(rng);
        int quantity = quantity_dist(rng);
        bool is_buy = side_dist(rng) == 1;
        book.add_order(i, price, quantity, is_buy);
    }
    
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            benchmark::DoNotOptimize(book.get_best_bid());
            benchmark::DoNotOptimize(book.get_best_ask());
        }
    }
}

static void BM_MatchingEngine(benchmark::State& state) {
    std::mt19937 rng(42); // Deterministic seed for reproducibility
    
    for (auto _ : state) {
        OrderBook book;
        int total_trades = 0;
        
        // Pre-populate the book with 1000 orders to create liquidity
        // Create a tighter spread for more realistic matching
        for (int i = 0; i < 1000; ++i) {
            double base_price = 100.0;
            double spread = 0.05; // 5 cent spread
            
            if (i % 2 == 0) {
                // Buy orders: 99.50 to 99.95
                double price = base_price - spread - (i % 10) * 0.01;
                book.add_order(i, price, 100, true);
            } else {
                // Sell orders: 100.05 to 100.50
                double price = base_price + spread + (i % 10) * 0.01;
                book.add_order(i, price, 100, false);
            }
        }
        
        // Add 1000 aggressive "market-crossing" orders
        std::uniform_real_distribution<double> aggressive_dist(0.0, 1.0);
        std::uniform_int_distribution<int> quantity_dist(50, 150);
        
        for (int i = 1000; i < 2000; ++i) {
            int quantity = quantity_dist(rng);
            
            if (aggressive_dist(rng) < 0.5) {
                // Aggressive buy order (crosses the spread)
                double price = 100.10 + aggressive_dist(rng) * 0.40; // 100.10 to 100.50
                auto trades = book.add_order(i, price, quantity, true);
                total_trades += trades.size();
            } else {
                // Aggressive sell order (crosses the spread)
                double price = 99.90 - aggressive_dist(rng) * 0.40; // 99.50 to 99.90
                auto trades = book.add_order(i, price, quantity, false);
                total_trades += trades.size();
            }
        }
        
        benchmark::DoNotOptimize(book);
        benchmark::DoNotOptimize(total_trades);
    }
}

BENCHMARK(BM_AddOrders);
BENCHMARK(BM_MixedOperations);
BENCHMARK(BM_BestPriceQueries);
BENCHMARK(BM_MatchingEngine);

BENCHMARK_MAIN();