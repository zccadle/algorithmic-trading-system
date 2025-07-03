#include <iostream>
#include <iomanip>
#include "order_book.h"

void print_trades(const std::vector<Trade>& trades) {
    if (trades.empty()) {
        std::cout << "No trades generated." << std::endl;
    } else {
        std::cout << "Trades generated:" << std::endl;
        for (const auto& trade : trades) {
            std::cout << "  Trade #" << trade.trade_id 
                      << ": " << trade.quantity << " @ $" << trade.price
                      << " (Buy Order: " << trade.buy_order_id 
                      << ", Sell Order: " << trade.sell_order_id << ")" << std::endl;
        }
    }
}

int main() {
    std::cout << "=== Order Book & Matching Engine Test ===" << std::endl;
    
    OrderBook book;
    std::vector<Trade> trades;
    
    std::cout << std::fixed << std::setprecision(2);
    
    // Build initial order book
    std::cout << "\n--- Building Initial Order Book ---" << std::endl;
    
    // Add buy orders (no matches expected)
    trades = book.add_order(1, 100.50, 10, true);
    print_trades(trades);
    trades = book.add_order(2, 100.75, 5, true);
    print_trades(trades);
    trades = book.add_order(3, 100.25, 15, true);
    print_trades(trades);
    
    // Add sell orders (no matches expected)
    trades = book.add_order(4, 101.00, 10, false);
    print_trades(trades);
    trades = book.add_order(5, 101.25, 15, false);
    print_trades(trades);
    
    std::cout << "\nInitial Best Bid: $" << book.get_best_bid() 
              << " (Quantity: " << book.get_bid_quantity_at(book.get_best_bid()) << ")" << std::endl;
    std::cout << "Initial Best Ask: $" << book.get_best_ask() 
              << " (Quantity: " << book.get_ask_quantity_at(book.get_best_ask()) << ")" << std::endl;
    
    // Test market-crossing orders
    std::cout << "\n--- Testing Market-Crossing Orders ---" << std::endl;
    
    // Add aggressive buy order that crosses the spread
    std::cout << "\nAdding Buy Order #6: 25 @ $101.10 (crosses spread)..." << std::endl;
    trades = book.add_order(6, 101.10, 25, true);
    print_trades(trades);
    
    std::cout << "\nBest Bid after crossing: $" << book.get_best_bid() 
              << " (Quantity: " << book.get_bid_quantity_at(book.get_best_bid()) << ")" << std::endl;
    std::cout << "Best Ask after crossing: $" << book.get_best_ask() 
              << " (Quantity: " << book.get_ask_quantity_at(book.get_best_ask()) << ")" << std::endl;
    
    // Add aggressive sell order that crosses the spread
    std::cout << "\nAdding Sell Order #7: 30 @ $100.00 (crosses spread)..." << std::endl;
    trades = book.add_order(7, 100.00, 30, false);
    print_trades(trades);
    
    std::cout << "\nFinal Best Bid: $" << book.get_best_bid() 
              << " (Quantity: " << book.get_bid_quantity_at(book.get_best_bid()) << ")" << std::endl;
    std::cout << "Final Best Ask: $" << book.get_best_ask() 
              << " (Quantity: " << book.get_ask_quantity_at(book.get_best_ask()) << ")" << std::endl;
    
    return 0;
}