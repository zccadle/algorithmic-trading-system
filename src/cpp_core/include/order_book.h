#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include <map>
#include <unordered_map>
#include <memory>

struct Order {
    int order_id;
    double price;
    int quantity;
    bool is_buy_side;
    
    Order(int id, double p, int q, bool buy) 
        : order_id(id), price(p), quantity(q), is_buy_side(buy) {}
};

class OrderBook {
private:
    std::map<double, int, std::greater<double>> buy_levels;  // Price -> Total quantity (descending)
    std::map<double, int> sell_levels;                       // Price -> Total quantity (ascending)
    std::unordered_map<int, std::unique_ptr<Order>> orders;  // Order ID -> Order details

public:
    OrderBook() = default;
    
    void add_order(int order_id, double price, int quantity, bool is_buy_side);
    bool cancel_order(int order_id);
    
    double get_best_bid() const;
    double get_best_ask() const;
    
    int get_bid_quantity_at(double price) const;
    int get_ask_quantity_at(double price) const;
};

#endif // ORDER_BOOK_H