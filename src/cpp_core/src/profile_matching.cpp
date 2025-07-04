#include "order_book.h"
#include <iostream>
#include <chrono>

void run_matching_engine_scenario() {
    OrderBook book;
    
    // Add initial orders (same as benchmark)
    for (int i = 0; i < 10; ++i) {
        book.add_order(i * 2, 100.0 + i, 100, true);      // Bids
        book.add_order(i * 2 + 1, 110.0 + i, 100, false); // Asks
    }
    
    // Add crossing orders to trigger matches
    for (int i = 0; i < 50; ++i) {
        auto trades = book.add_order(1000 + i, 109.0, 50, true);  // Buy order that crosses
        trades.clear(); // Force destruction
        
        trades = book.add_order(2000 + i, 101.0, 50, false); // Sell order that crosses
        trades.clear();
    }
    
    // Add more regular orders
    for (int i = 0; i < 100; ++i) {
        book.add_order(3000 + i, 95.0 + (i % 10), 100, true);
        book.add_order(4000 + i, 115.0 + (i % 10), 100, false);
    }
}

int main() {
    std::cout << "Starting profiling run...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Run the matching engine scenario many more times
    for (int i = 0; i < 1000; ++i) {
        if (i % 100 == 0) {
            std::cout << "Iteration " << i << "/1000\n";
        }
        run_matching_engine_scenario();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Completed in " << duration.count() << " ms\n";
    
    return 0;
}