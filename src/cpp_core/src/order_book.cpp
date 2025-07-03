#include "order_book.h"
#include <limits>

void OrderBook::add_order(int order_id, double price, int quantity, bool is_buy_side) {
    auto order = std::make_unique<Order>(order_id, price, quantity, is_buy_side);
    
    if (is_buy_side) {
        buy_levels[price] += quantity;
    } else {
        sell_levels[price] += quantity;
    }
    
    orders[order_id] = std::move(order);
}

bool OrderBook::cancel_order(int order_id) {
    auto it = orders.find(order_id);
    if (it == orders.end()) {
        return false;
    }
    
    const auto& order = it->second;
    
    if (order->is_buy_side) {
        auto level_it = buy_levels.find(order->price);
        if (level_it != buy_levels.end()) {
            level_it->second -= order->quantity;
            if (level_it->second <= 0) {
                buy_levels.erase(level_it);
            }
        }
    } else {
        auto level_it = sell_levels.find(order->price);
        if (level_it != sell_levels.end()) {
            level_it->second -= order->quantity;
            if (level_it->second <= 0) {
                sell_levels.erase(level_it);
            }
        }
    }
    
    orders.erase(it);
    return true;
}

double OrderBook::get_best_bid() const {
    if (buy_levels.empty()) {
        return -std::numeric_limits<double>::infinity();
    }
    return buy_levels.begin()->first;
}

double OrderBook::get_best_ask() const {
    if (sell_levels.empty()) {
        return std::numeric_limits<double>::infinity();
    }
    return sell_levels.begin()->first;
}

int OrderBook::get_bid_quantity_at(double price) const {
    auto it = buy_levels.find(price);
    return (it != buy_levels.end()) ? it->second : 0;
}

int OrderBook::get_ask_quantity_at(double price) const {
    auto it = sell_levels.find(price);
    return (it != sell_levels.end()) ? it->second : 0;
}