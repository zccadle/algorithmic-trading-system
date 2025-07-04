#include "smart_order_router.h"
#include <iostream>
#include <iomanip>

// SmartOrderRouter implementation

void SmartOrderRouter::add_exchange(std::unique_ptr<IExchange> exchange, const FeeSchedule& fees) {
    exchanges_.emplace_back(std::move(exchange), fees);
}

double SmartOrderRouter::calculate_buy_cost(double price, int quantity, double fee_rate) const {
    double notional = price * quantity;
    double fee = notional * fee_rate;
    return notional + fee;  // Total cost including fees
}

double SmartOrderRouter::calculate_sell_proceeds(double price, int quantity, double fee_rate) const {
    double notional = price * quantity;
    double fee = notional * fee_rate;
    return notional - fee;  // Net proceeds after fees
}

bool SmartOrderRouter::would_be_maker_order(const OrderBook& book, double price, bool is_buy) const {
    if (is_buy) {
        // Buy order is maker if price < best_ask
        double best_ask = book.get_best_ask();
        return best_ask == std::numeric_limits<double>::infinity() || price < best_ask;
    } else {
        // Sell order is maker if price > best_bid
        double best_bid = book.get_best_bid();
        return best_bid == -std::numeric_limits<double>::infinity() || price > best_bid;
    }
}

RoutingDecision SmartOrderRouter::route_order(int order_id, double price, int quantity, bool is_buy_side) {
    RoutingDecision best_decision;
    
    if (is_buy_side) {
        // For buy orders, find lowest effective cost (price + fees)
        double best_cost = std::numeric_limits<double>::max();
        
        for (const auto& exchange_info : exchanges_) {
            if (!exchange_info.is_active || !exchange_info.exchange->is_available()) {
                continue;
            }
            
            const OrderBook& book = exchange_info.exchange->get_order_book();
            double best_ask = book.get_best_ask();
            
            // Skip if no asks available
            if (best_ask == std::numeric_limits<double>::infinity()) {
                continue;
            }
            
            // Check available quantity
            int available_qty = book.get_ask_quantity_at(best_ask);
            if (available_qty == 0) {
                continue;
            }
            
            // Determine if maker or taker
            bool is_maker = would_be_maker_order(book, price, is_buy_side);
            double fee_rate = is_maker ? exchange_info.fees.maker_fee : exchange_info.fees.taker_fee;
            
            // Calculate total cost
            double total_cost = consider_fees_ ? 
                calculate_buy_cost(best_ask, std::min(quantity, available_qty), fee_rate) :
                best_ask * std::min(quantity, available_qty);
            
            // Consider latency if enabled
            if (consider_latency_) {
                auto metrics = exchange_info.exchange->get_metrics();
                // Add a small penalty for high latency exchanges
                total_cost *= (1.0 + metrics.avg_latency.count() / 10000.0);
            }
            
            if (total_cost < best_cost) {
                best_cost = total_cost;
                best_decision.exchange_id = exchange_info.exchange->get_id();
                best_decision.expected_price = best_ask;
                best_decision.expected_fee = consider_fees_ ? 
                    (best_ask * std::min(quantity, available_qty) * fee_rate) : 0.0;
                best_decision.total_cost = total_cost;
                best_decision.available_quantity = available_qty;
                best_decision.is_maker = is_maker;
            }
        }
    } else {
        // For sell orders, find highest effective proceeds (price - fees)
        double best_proceeds = -std::numeric_limits<double>::max();
        
        for (const auto& exchange_info : exchanges_) {
            if (!exchange_info.is_active || !exchange_info.exchange->is_available()) {
                continue;
            }
            
            const OrderBook& book = exchange_info.exchange->get_order_book();
            double best_bid = book.get_best_bid();
            
            // Skip if no bids available
            if (best_bid == -std::numeric_limits<double>::infinity()) {
                continue;
            }
            
            // Check available quantity
            int available_qty = book.get_bid_quantity_at(best_bid);
            if (available_qty == 0) {
                continue;
            }
            
            // Determine if maker or taker
            bool is_maker = would_be_maker_order(book, price, is_buy_side);
            double fee_rate = is_maker ? exchange_info.fees.maker_fee : exchange_info.fees.taker_fee;
            
            // Calculate net proceeds
            double net_proceeds = consider_fees_ ?
                calculate_sell_proceeds(best_bid, std::min(quantity, available_qty), fee_rate) :
                best_bid * std::min(quantity, available_qty);
            
            // Consider latency if enabled
            if (consider_latency_) {
                auto metrics = exchange_info.exchange->get_metrics();
                // Reduce proceeds slightly for high latency exchanges
                net_proceeds *= (1.0 - metrics.avg_latency.count() / 10000.0);
            }
            
            if (net_proceeds > best_proceeds) {
                best_proceeds = net_proceeds;
                best_decision.exchange_id = exchange_info.exchange->get_id();
                best_decision.expected_price = best_bid;
                best_decision.expected_fee = consider_fees_ ?
                    (best_bid * std::min(quantity, available_qty) * fee_rate) : 0.0;
                best_decision.total_cost = net_proceeds;
                best_decision.available_quantity = available_qty;
                best_decision.is_maker = is_maker;
            }
        }
    }
    
    return best_decision;
}

SmartOrderRouter::AggregatedMarketData SmartOrderRouter::get_aggregated_market_data() const {
    AggregatedMarketData data;
    data.best_bid = -std::numeric_limits<double>::infinity();
    data.best_ask = std::numeric_limits<double>::infinity();
    data.total_bid_quantity = 0;
    data.total_ask_quantity = 0;
    data.best_bid_exchange = ExchangeID::Unknown;
    data.best_ask_exchange = ExchangeID::Unknown;
    
    for (const auto& exchange_info : exchanges_) {
        if (!exchange_info.is_active || !exchange_info.exchange->is_available()) {
            continue;
        }
        
        const OrderBook& book = exchange_info.exchange->get_order_book();
        
        // Check best bid
        double bid = book.get_best_bid();
        if (bid > data.best_bid) {
            data.best_bid = bid;
            data.best_bid_exchange = exchange_info.exchange->get_id();
        }
        if (bid > -std::numeric_limits<double>::infinity()) {
            data.total_bid_quantity += book.get_bid_quantity_at(bid);
        }
        
        // Check best ask
        double ask = book.get_best_ask();
        if (ask < data.best_ask) {
            data.best_ask = ask;
            data.best_ask_exchange = exchange_info.exchange->get_id();
        }
        if (ask < std::numeric_limits<double>::infinity()) {
            data.total_ask_quantity += book.get_ask_quantity_at(ask);
        }
    }
    
    return data;
}

std::vector<SmartOrderRouter::SplitOrder> SmartOrderRouter::route_order_split(
    int order_id, double price, int total_quantity, bool is_buy_side) {
    
    std::vector<SplitOrder> splits;
    int remaining_quantity = total_quantity;
    
    // Keep routing portions until all quantity is allocated
    while (remaining_quantity > 0) {
        RoutingDecision decision = route_order(order_id, price, remaining_quantity, is_buy_side);
        
        if (decision.exchange_id == ExchangeID::Unknown) {
            break;  // No more liquidity available
        }
        
        int fill_quantity = std::min(remaining_quantity, decision.available_quantity);
        
        SplitOrder split;
        split.exchange_id = decision.exchange_id;
        split.quantity = fill_quantity;
        split.expected_price = decision.expected_price;
        split.expected_fee = decision.expected_fee * fill_quantity / decision.available_quantity;
        
        splits.push_back(split);
        remaining_quantity -= fill_quantity;
        
        // Mark this liquidity as consumed (in real implementation, would update order book)
        // For now, we'll break to avoid infinite loop
        if (splits.size() >= exchanges_.size()) {
            break;
        }
    }
    
    return splits;
}

void SmartOrderRouter::set_exchange_active(ExchangeID id, bool active) {
    for (auto& exchange_info : exchanges_) {
        if (exchange_info.exchange->get_id() == id) {
            exchange_info.is_active = active;
            break;
        }
    }
}

void SmartOrderRouter::print_routing_stats() const {
    std::cout << "\n=== Smart Order Router Statistics ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    
    for (const auto& exchange_info : exchanges_) {
        const auto& exchange = exchange_info.exchange;
        const auto& book = exchange->get_order_book();
        auto metrics = exchange->get_metrics();
        
        std::cout << "\n" << exchange->get_name() 
                  << " (ID: " << static_cast<int>(exchange->get_id()) << ")"
                  << " - " << (exchange_info.is_active ? "ACTIVE" : "INACTIVE") << std::endl;
        
        std::cout << "  Best Bid: ";
        double bid = book.get_best_bid();
        if (bid > -std::numeric_limits<double>::infinity()) {
            std::cout << "$" << bid << " (Qty: " << book.get_bid_quantity_at(bid) << ")";
        } else {
            std::cout << "None";
        }
        
        std::cout << " | Best Ask: ";
        double ask = book.get_best_ask();
        if (ask < std::numeric_limits<double>::infinity()) {
            std::cout << "$" << ask << " (Qty: " << book.get_ask_quantity_at(ask) << ")";
        } else {
            std::cout << "None";
        }
        std::cout << std::endl;
        
        std::cout << "  Fees: Maker " << (exchange_info.fees.maker_fee * 100) << "% / "
                  << "Taker " << (exchange_info.fees.taker_fee * 100) << "%" << std::endl;
        
        std::cout << "  Metrics: Latency " << metrics.avg_latency.count() << "ms, "
                  << "Fill Rate " << (metrics.fill_rate * 100) << "%, "
                  << "Uptime " << (metrics.uptime * 100) << "%" << std::endl;
    }
    
    auto aggregated = get_aggregated_market_data();
    std::cout << "\n=== Aggregated Market Data ===" << std::endl;
    std::cout << "Best Bid: $" << aggregated.best_bid 
              << " on " << exchange_to_string(aggregated.best_bid_exchange)
              << " (Total Qty: " << aggregated.total_bid_quantity << ")" << std::endl;
    std::cout << "Best Ask: $" << aggregated.best_ask
              << " on " << exchange_to_string(aggregated.best_ask_exchange)
              << " (Total Qty: " << aggregated.total_ask_quantity << ")" << std::endl;
}