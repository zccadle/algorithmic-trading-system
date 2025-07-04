#include "order_book.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iomanip>

using namespace std::chrono;

void run_matching_engine_scenario() {
    OrderBook book;
    
    // Add initial orders
    for (int i = 0; i < 10; ++i) {
        book.add_order(i * 2, 100.0 + i, 100, true);
        book.add_order(i * 2 + 1, 110.0 + i, 100, false);
    }
    
    // Add crossing orders
    for (int i = 0; i < 50; ++i) {
        auto trades = book.add_order(1000 + i, 109.0, 50, true);
        trades = book.add_order(2000 + i, 101.0, 50, false);
    }
}

OrderBook setup_book() {
    OrderBook book;
    for (int i = 0; i < 100; ++i) {
        book.add_order(i * 2, 100.0 + i, 100, true);
        book.add_order(i * 2 + 1, 110.0 + i, 100, false);
    }
    return book;
}

template<typename T>
void print_stats(const std::string& name, const std::vector<T>& timings) {
    auto sum = std::accumulate(timings.begin(), timings.end(), T{});
    auto avg = sum / timings.size();
    auto min = *std::min_element(timings.begin(), timings.end());
    auto max = *std::max_element(timings.begin(), timings.end());
    
    // Calculate percentiles
    std::vector<T> sorted = timings;
    std::sort(sorted.begin(), sorted.end());
    auto p50 = sorted[sorted.size() / 2];
    auto p95 = sorted[sorted.size() * 95 / 100];
    auto p99 = sorted[sorted.size() * 99 / 100];
    
    std::cout << "\n--- " << name << " ---\n";
    std::cout << "Samples: " << timings.size() << "\n";
    std::cout << "Average: " << duration_cast<nanoseconds>(avg).count() << " ns\n";
    std::cout << "Min: " << duration_cast<nanoseconds>(min).count() << " ns\n";
    std::cout << "Max: " << duration_cast<nanoseconds>(max).count() << " ns\n";
    std::cout << "P50: " << duration_cast<nanoseconds>(p50).count() << " ns\n";
    std::cout << "P95: " << duration_cast<nanoseconds>(p95).count() << " ns\n";
    std::cout << "P99: " << duration_cast<nanoseconds>(p99).count() << " ns\n";
}

int main() {
    std::cout << "=== C++ Detailed Performance Analysis ===\n";
    std::cout << std::fixed << std::setprecision(3);
    
    // Warm up
    for (int i = 0; i < 10; ++i) {
        run_matching_engine_scenario();
    }
    
    // Test 1: Order insertion performance
    std::vector<high_resolution_clock::duration> timings;
    for (int i = 0; i < 100; ++i) {
        auto start = high_resolution_clock::now();
        OrderBook book;
        for (int j = 0; j < 1000; ++j) {
            book.add_order(j, 100.0 + (j % 20), 10, j % 2 == 0);
        }
        auto end = high_resolution_clock::now();
        timings.push_back(end - start);
    }
    print_stats("Order Insertion (1000 orders)", timings);
    
    // Test 2: Matching engine performance
    timings.clear();
    for (int i = 0; i < 100; ++i) {
        auto start = high_resolution_clock::now();
        run_matching_engine_scenario();
        auto end = high_resolution_clock::now();
        timings.push_back(end - start);
    }
    print_stats("Matching Engine Scenario", timings);
    
    // Test 3: Best price queries
    auto book = setup_book();
    timings.clear();
    for (int i = 0; i < 10000; ++i) {
        auto start = high_resolution_clock::now();
        volatile double bid = book.get_best_bid();
        volatile double ask = book.get_best_ask();
        auto end = high_resolution_clock::now();
        timings.push_back(end - start);
    }
    print_stats("Best Price Queries", timings);
    
    // Test 4: Memory allocation patterns
    std::cout << "\n--- Memory Allocation Test ---\n";
    auto start = high_resolution_clock::now();
    std::vector<std::unique_ptr<OrderBook>> books;
    for (int i = 0; i < 100; ++i) {
        auto book = std::make_unique<OrderBook>();
        for (int j = 0; j < 100; ++j) {
            book->add_order(i * 100 + j, 100.0 + j, 10, j % 2 == 0);
        }
        books.push_back(std::move(book));
    }
    auto end = high_resolution_clock::now();
    std::cout << "Created 100 order books with 100 orders each in " 
              << duration_cast<microseconds>(end - start).count() << " µs\n";
    
    // Force deallocation
    start = high_resolution_clock::now();
    books.clear();
    end = high_resolution_clock::now();
    std::cout << "Deallocated all books in " 
              << duration_cast<microseconds>(end - start).count() << " µs\n";
    
    return 0;
}