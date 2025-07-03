#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include "order_book.h"

struct MarketOrder {
    bool is_buy;
    double price;
    int quantity;
};

std::vector<MarketOrder> read_market_data(const std::string& filename) {
    std::vector<MarketOrder> orders;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }
    
    std::string line;
    // Skip header line
    std::getline(file, line);
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string is_buy_str, price_str, quantity_str;
        
        if (std::getline(ss, is_buy_str, ',') &&
            std::getline(ss, price_str, ',') &&
            std::getline(ss, quantity_str, ',')) {
            
            MarketOrder order;
            order.is_buy = (std::stoi(is_buy_str) == 1);
            order.price = std::stod(price_str);
            order.quantity = std::stoi(quantity_str);
            orders.push_back(order);
        }
    }
    
    file.close();
    return orders;
}

void print_trades(const std::vector<Trade>& trades) {
    for (const auto& trade : trades) {
        std::cout << "  Trade #" << trade.trade_id 
                  << ": " << trade.quantity << " @ $" << trade.price
                  << " (Buy Order: " << trade.buy_order_id 
                  << ", Sell Order: " << trade.sell_order_id << ")" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=== Order Book Replay Tool ===" << std::endl;
    
    // Determine the CSV file path
    std::string csv_path = "../../../market_data.csv";  // Default path from build directory
    if (argc > 1) {
        csv_path = argv[1];
    }
    
    try {
        // Read market data from CSV
        std::cout << "\nReading market data from: " << csv_path << std::endl;
        auto orders = read_market_data(csv_path);
        std::cout << "Loaded " << orders.size() << " orders from file." << std::endl;
        
        // Create order book and replay orders
        OrderBook book;
        int total_trades = 0;
        int order_id = 1;
        
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n--- Replaying Market Data ---" << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (const auto& order : orders) {
            std::cout << "\nOrder #" << order_id << ": " 
                      << (order.is_buy ? "BUY" : "SELL") 
                      << " " << order.quantity << " @ $" << order.price << std::endl;
            
            auto trades = book.add_order(order_id++, order.price, order.quantity, order.is_buy);
            
            if (!trades.empty()) {
                std::cout << "Generated " << trades.size() << " trade(s):" << std::endl;
                print_trades(trades);
                total_trades += trades.size();
            } else {
                std::cout << "Order added to book (no trades)." << std::endl;
            }
            
            // Print current book state
            double best_bid = book.get_best_bid();
            double best_ask = book.get_best_ask();
            
            std::cout << "Book State - Best Bid: ";
            if (best_bid > -std::numeric_limits<double>::infinity()) {
                std::cout << "$" << best_bid << " (Qty: " << book.get_bid_quantity_at(best_bid) << ")";
            } else {
                std::cout << "None";
            }
            
            std::cout << ", Best Ask: ";
            if (best_ask < std::numeric_limits<double>::infinity()) {
                std::cout << "$" << best_ask << " (Qty: " << book.get_ask_quantity_at(best_ask) << ")";
            } else {
                std::cout << "None";
            }
            std::cout << std::endl;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // Print summary
        std::cout << "\n=== Replay Summary ===" << std::endl;
        std::cout << "Total orders processed: " << orders.size() << std::endl;
        std::cout << "Total trades generated: " << total_trades << std::endl;
        std::cout << "Processing time: " << duration.count() << " microseconds" << std::endl;
        std::cout << "Average time per order: " << (duration.count() / static_cast<double>(orders.size())) 
                  << " microseconds" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}