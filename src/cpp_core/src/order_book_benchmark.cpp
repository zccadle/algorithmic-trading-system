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

BENCHMARK(BM_AddOrders);
BENCHMARK(BM_MixedOperations);
BENCHMARK(BM_BestPriceQueries);

BENCHMARK_MAIN();