#ifndef SMART_ORDER_ROUTER_H
#define SMART_ORDER_ROUTER_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <optional>
#include <limits>
#include <algorithm>
#include <chrono>
#include <string>
#include "order_book.h"

enum class ExchangeID {
    Binance,
    Coinbase,
    Kraken,
    FTX,
    Unknown
};

// Convert ExchangeID to string for display
inline std::string exchange_to_string(ExchangeID id) {
    switch (id) {
        case ExchangeID::Binance: return "Binance";
        case ExchangeID::Coinbase: return "Coinbase";
        case ExchangeID::Kraken: return "Kraken";
        case ExchangeID::FTX: return "FTX";
        default: return "Unknown";
    }
}

struct FeeSchedule {
    double maker_fee;  // Fee as percentage (e.g., 0.001 = 0.1%)
    double taker_fee;  // Fee as percentage
    
    FeeSchedule(double maker = 0.001, double taker = 0.002) 
        : maker_fee(maker), taker_fee(taker) {}
};

struct RoutingDecision {
    ExchangeID exchange_id;
    double expected_price;
    double expected_fee;
    double total_cost;  // For buys: price + fee, For sells: price - fee
    int available_quantity;
    bool is_maker;
    
    RoutingDecision() 
        : exchange_id(ExchangeID::Unknown)
        , expected_price(0.0)
        , expected_fee(0.0)
        , total_cost(0.0)
        , available_quantity(0)
        , is_maker(false) {}
};

struct ExchangeMetrics {
    std::chrono::milliseconds avg_latency;
    double fill_rate;  // Percentage of orders that get filled
    double uptime;     // Percentage uptime over last 24h
    
    ExchangeMetrics(int latency_ms = 10, double fill = 0.95, double up = 0.999)
        : avg_latency(latency_ms)
        , fill_rate(fill)
        , uptime(up) {}
};

// Abstract interface for exchanges
class IExchange {
public:
    virtual ~IExchange() = default;
    virtual OrderBook& get_order_book() = 0;
    virtual const OrderBook& get_order_book() const = 0;
    virtual ExchangeID get_id() const = 0;
    virtual std::string get_name() const = 0;
    virtual bool is_available() const { return true; }
    virtual ExchangeMetrics get_metrics() const = 0;
};

class SmartOrderRouter {
private:
    struct ExchangeInfo {
        std::unique_ptr<IExchange> exchange;
        FeeSchedule fees;
        bool is_active;
        
        ExchangeInfo(std::unique_ptr<IExchange> ex, FeeSchedule f)
            : exchange(std::move(ex))
            , fees(f)
            , is_active(true) {}
    };
    
    std::vector<ExchangeInfo> exchanges_;
    bool consider_latency_;
    bool consider_fees_;
    
    // Calculate the effective cost for a buy order
    double calculate_buy_cost(double price, int quantity, double fee_rate) const;
    
    // Calculate the effective proceeds for a sell order
    double calculate_sell_proceeds(double price, int quantity, double fee_rate) const;
    
    // Check if this would be a maker or taker order
    bool would_be_maker_order(const OrderBook& book, double price, bool is_buy) const;
    
public:
    SmartOrderRouter(bool consider_latency = true, bool consider_fees = true)
        : consider_latency_(consider_latency)
        , consider_fees_(consider_fees) {}
    
    // Add an exchange to the router
    void add_exchange(std::unique_ptr<IExchange> exchange, const FeeSchedule& fees);
    
    // Route a single order to the best exchange
    RoutingDecision route_order(int order_id, double price, int quantity, bool is_buy_side);
    
    // Get aggregated market data across all exchanges
    struct AggregatedMarketData {
        double best_bid;
        double best_ask;
        int total_bid_quantity;
        int total_ask_quantity;
        ExchangeID best_bid_exchange;
        ExchangeID best_ask_exchange;
    };
    
    AggregatedMarketData get_aggregated_market_data() const;
    
    // Advanced routing for large orders (splits across exchanges)
    struct SplitOrder {
        ExchangeID exchange_id;
        int quantity;
        double expected_price;
        double expected_fee;
    };
    
    std::vector<SplitOrder> route_order_split(int order_id, double price, int total_quantity, bool is_buy_side);
    
    // Enable/disable specific exchanges
    void set_exchange_active(ExchangeID id, bool active);
    
    // Get current routing statistics
    void print_routing_stats() const;
};

#endif // SMART_ORDER_ROUTER_H