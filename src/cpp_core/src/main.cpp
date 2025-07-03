#include <iostream>
#include <iomanip>
#include "order_book.h"

int main() {
    std::cout << "=== Order Book Test ===" << std::endl;
    
    OrderBook book;
    
    // Add buy orders
    book.add_order(1, 100.50, 10, true);
    book.add_order(2, 100.75, 5, true);
    book.add_order(3, 100.25, 15, true);
    book.add_order(4, 100.50, 20, true);  // Same price as order 1
    
    // Add sell orders
    book.add_order(5, 101.00, 10, false);
    book.add_order(6, 101.25, 15, false);
    book.add_order(7, 101.50, 5, false);
    book.add_order(8, 101.00, 10, false);  // Same price as order 5
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\nBest Bid: $" << book.get_best_bid() 
              << " (Quantity: " << book.get_bid_quantity_at(book.get_best_bid()) << ")" << std::endl;
    std::cout << "Best Ask: $" << book.get_best_ask() 
              << " (Quantity: " << book.get_ask_quantity_at(book.get_best_ask()) << ")" << std::endl;
    
    // Test order cancellation
    std::cout << "\nCancelling order 2 (Buy @ $100.75)..." << std::endl;
    book.cancel_order(2);
    
    std::cout << "New Best Bid: $" << book.get_best_bid() 
              << " (Quantity: " << book.get_bid_quantity_at(book.get_best_bid()) << ")" << std::endl;
    
    // Cancel one of the aggregated orders
    std::cout << "\nCancelling order 1 (Buy @ $100.50)..." << std::endl;
    book.cancel_order(1);
    
    std::cout << "Quantity at $100.50: " << book.get_bid_quantity_at(100.50) << std::endl;
    
    return 0;
}