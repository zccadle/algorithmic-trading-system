#include "order_book.h"
#include <limits>
#include <algorithm>

std::vector<Trade> OrderBook::add_order(int order_id, double price, int quantity, bool is_buy_side) {
    std::vector<Trade> trades;
    int remaining_quantity = quantity;
    
    // Matching logic
    if (is_buy_side) {
        // Match with sell orders
        auto sell_it = sell_levels.begin();
        while (sell_it != sell_levels.end() && remaining_quantity > 0 && price >= sell_it->first) {
            double match_price = sell_it->first;
            auto& order_ids = sell_orders_at_level[match_price];
            
            auto order_it = order_ids.begin();
            while (order_it != order_ids.end() && remaining_quantity > 0) {
                auto& passive_order = orders[*order_it];
                int trade_quantity = std::min(remaining_quantity, passive_order->quantity);
                
                // Create trade
                trades.emplace_back(next_trade_id++, match_price, trade_quantity, 
                                  order_id, passive_order->order_id);
                
                // Update quantities
                remaining_quantity -= trade_quantity;
                passive_order->quantity -= trade_quantity;
                sell_it->second -= trade_quantity;
                
                if (passive_order->quantity == 0) {
                    orders.erase(*order_it);
                    order_it = order_ids.erase(order_it);
                } else {
                    ++order_it;
                }
            }
            
            if (sell_it->second <= 0) {
                sell_orders_at_level.erase(match_price);
                sell_it = sell_levels.erase(sell_it);
            } else {
                ++sell_it;
            }
        }
    } else {
        // Match with buy orders
        auto buy_it = buy_levels.begin();
        while (buy_it != buy_levels.end() && remaining_quantity > 0 && price <= buy_it->first) {
            double match_price = buy_it->first;
            auto& order_ids = buy_orders_at_level[match_price];
            
            auto order_it = order_ids.begin();
            while (order_it != order_ids.end() && remaining_quantity > 0) {
                auto& passive_order = orders[*order_it];
                int trade_quantity = std::min(remaining_quantity, passive_order->quantity);
                
                // Create trade
                trades.emplace_back(next_trade_id++, match_price, trade_quantity, 
                                  passive_order->order_id, order_id);
                
                // Update quantities
                remaining_quantity -= trade_quantity;
                passive_order->quantity -= trade_quantity;
                buy_it->second -= trade_quantity;
                
                if (passive_order->quantity == 0) {
                    orders.erase(*order_it);
                    order_it = order_ids.erase(order_it);
                } else {
                    ++order_it;
                }
            }
            
            if (buy_it->second <= 0) {
                buy_orders_at_level.erase(match_price);
                buy_it = buy_levels.erase(buy_it);
            } else {
                ++buy_it;
            }
        }
    }
    
    // Add remaining quantity to book if not fully matched
    if (remaining_quantity > 0) {
        auto order = std::make_unique<Order>(order_id, price, remaining_quantity, is_buy_side);
        
        if (is_buy_side) {
            buy_levels[price] += remaining_quantity;
            buy_orders_at_level[price].push_back(order_id);
        } else {
            sell_levels[price] += remaining_quantity;
            sell_orders_at_level[price].push_back(order_id);
        }
        
        orders[order_id] = std::move(order);
    }
    
    return trades;
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
        
        auto& order_ids = buy_orders_at_level[order->price];
        order_ids.erase(std::remove(order_ids.begin(), order_ids.end(), order_id), order_ids.end());
        if (order_ids.empty()) {
            buy_orders_at_level.erase(order->price);
        }
    } else {
        auto level_it = sell_levels.find(order->price);
        if (level_it != sell_levels.end()) {
            level_it->second -= order->quantity;
            if (level_it->second <= 0) {
                sell_levels.erase(level_it);
            }
        }
        
        auto& order_ids = sell_orders_at_level[order->price];
        order_ids.erase(std::remove(order_ids.begin(), order_ids.end(), order_id), order_ids.end());
        if (order_ids.empty()) {
            sell_orders_at_level.erase(order->price);
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