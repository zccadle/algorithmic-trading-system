#include <iostream>
#include <iomanip>
#include <memory>
#include "smart_order_router.h"
#include "order_book.h"

// Mock exchange implementation
class MockExchange : public IExchange {
private:
    ExchangeID id_;
    std::string name_;
    OrderBook order_book_;
    ExchangeMetrics metrics_;
    bool is_available_;
    
public:
    MockExchange(ExchangeID id, const std::string& name, const ExchangeMetrics& metrics = ExchangeMetrics())
        : id_(id)
        , name_(name)
        , metrics_(metrics)
        , is_available_(true) {}
    
    OrderBook& get_order_book() override { return order_book_; }
    const OrderBook& get_order_book() const override { return order_book_; }
    ExchangeID get_id() const override { return id_; }
    std::string get_name() const override { return name_; }
    bool is_available() const override { return is_available_; }
    ExchangeMetrics get_metrics() const override { return metrics_; }
    
    void set_available(bool available) { is_available_ = available; }
};

void print_routing_decision(const RoutingDecision& decision, const std::string& order_type) {
    std::cout << "\n" << order_type << " Routing Decision:" << std::endl;
    std::cout << "  Best Exchange: " << exchange_to_string(decision.exchange_id) << std::endl;
    std::cout << "  Expected Price: $" << decision.expected_price << std::endl;
    std::cout << "  Expected Fee: $" << decision.expected_fee 
              << " (" << (decision.is_maker ? "Maker" : "Taker") << ")" << std::endl;
    std::cout << "  Total Cost/Proceeds: $" << decision.total_cost << std::endl;
    std::cout << "  Available Quantity: " << decision.available_quantity << std::endl;
}

int main() {
    std::cout << "=== Smart Order Router Test ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    
    // Create mock exchanges with different characteristics
    auto binance = std::make_unique<MockExchange>(
        ExchangeID::Binance, 
        "Binance",
        ExchangeMetrics(5, 0.98, 0.999)  // 5ms latency, 98% fill rate
    );
    
    auto coinbase = std::make_unique<MockExchange>(
        ExchangeID::Coinbase,
        "Coinbase",
        ExchangeMetrics(15, 0.95, 0.998)  // 15ms latency, 95% fill rate
    );
    
    auto kraken = std::make_unique<MockExchange>(
        ExchangeID::Kraken,
        "Kraken",
        ExchangeMetrics(25, 0.92, 0.997)  // 25ms latency, 92% fill rate
    );
    
    // Populate order books with different prices
    std::cout << "\n1. Setting up mock order books..." << std::endl;
    
    // Binance: Tight spread, high liquidity
    binance->get_order_book().add_order(1, 45000.00, 10, true);   // Buy
    binance->get_order_book().add_order(2, 44999.50, 5, true);    // Buy
    binance->get_order_book().add_order(3, 45001.00, 8, false);   // Sell
    binance->get_order_book().add_order(4, 45001.50, 12, false);  // Sell
    std::cout << "  Binance: Bid $45000.00, Ask $45001.00 (Spread: $1.00)" << std::endl;
    
    // Coinbase: Wider spread, medium liquidity
    coinbase->get_order_book().add_order(5, 44999.00, 7, true);   // Buy
    coinbase->get_order_book().add_order(6, 44998.00, 3, true);   // Buy
    coinbase->get_order_book().add_order(7, 45002.00, 6, false);  // Sell
    coinbase->get_order_book().add_order(8, 45003.00, 9, false);  // Sell
    std::cout << "  Coinbase: Bid $44999.00, Ask $45002.00 (Spread: $3.00)" << std::endl;
    
    // Kraken: Best bid, higher ask
    kraken->get_order_book().add_order(9, 45000.50, 15, true);    // Buy (best bid)
    kraken->get_order_book().add_order(10, 45000.00, 5, true);    // Buy
    kraken->get_order_book().add_order(11, 45002.50, 10, false);  // Sell
    kraken->get_order_book().add_order(12, 45003.50, 8, false);   // Sell
    std::cout << "  Kraken: Bid $45000.50, Ask $45002.50 (Spread: $2.00)" << std::endl;
    
    // Create Smart Order Router
    SmartOrderRouter sor(true, true);  // Consider both latency and fees
    
    // Add exchanges with different fee structures
    sor.add_exchange(std::move(binance), FeeSchedule(0.0010, 0.0010));   // 0.10% maker/taker
    sor.add_exchange(std::move(coinbase), FeeSchedule(0.0005, 0.0015));  // 0.05% maker, 0.15% taker
    sor.add_exchange(std::move(kraken), FeeSchedule(0.0002, 0.0012));    // 0.02% maker, 0.12% taker
    
    // Test 1: Route a market buy order
    std::cout << "\n2. Testing Buy Order Routing" << std::endl;
    std::cout << "   Order: BUY 5 BTC at market" << std::endl;
    
    RoutingDecision buy_decision = sor.route_order(101, 50000.0, 5, true);
    print_routing_decision(buy_decision, "Buy");
    
    // Test 2: Route a market sell order
    std::cout << "\n3. Testing Sell Order Routing" << std::endl;
    std::cout << "   Order: SELL 5 BTC at market" << std::endl;
    
    RoutingDecision sell_decision = sor.route_order(102, 40000.0, 5, false);
    print_routing_decision(sell_decision, "Sell");
    
    // Test 3: Route a large order that needs splitting
    std::cout << "\n4. Testing Large Order Splitting" << std::endl;
    std::cout << "   Order: BUY 20 BTC at market" << std::endl;
    
    auto splits = sor.route_order_split(103, 50000.0, 20, true);
    std::cout << "\n   Order split across " << splits.size() << " exchanges:" << std::endl;
    double total_cost = 0.0;
    for (const auto& split : splits) {
        std::cout << "   - " << exchange_to_string(split.exchange_id) 
                  << ": " << split.quantity << " BTC @ $" << split.expected_price
                  << " (Fee: $" << split.expected_fee << ")" << std::endl;
        total_cost += (split.expected_price * split.quantity) + split.expected_fee;
    }
    std::cout << "   Total Cost: $" << total_cost << std::endl;
    
    // Test 4: Show routing statistics
    sor.print_routing_stats();
    
    // Test 5: Disable an exchange and re-route
    std::cout << "\n5. Testing Exchange Failover" << std::endl;
    std::cout << "   Disabling Binance..." << std::endl;
    sor.set_exchange_active(ExchangeID::Binance, false);
    
    RoutingDecision failover_decision = sor.route_order(104, 50000.0, 5, true);
    std::cout << "   New routing decision after Binance disabled:" << std::endl;
    print_routing_decision(failover_decision, "Failover Buy");
    
    // Test 6: Compare with/without fee consideration
    std::cout << "\n6. Testing Fee Impact on Routing" << std::endl;
    
    SmartOrderRouter sor_no_fees(true, false);  // Don't consider fees
    // Re-create exchanges for the no-fee router
    auto binance2 = std::make_unique<MockExchange>(ExchangeID::Binance, "Binance");
    auto coinbase2 = std::make_unique<MockExchange>(ExchangeID::Coinbase, "Coinbase");
    auto kraken2 = std::make_unique<MockExchange>(ExchangeID::Kraken, "Kraken");
    
    // Same order books
    binance2->get_order_book().add_order(1, 45000.00, 10, true);
    binance2->get_order_book().add_order(3, 45001.00, 8, false);
    coinbase2->get_order_book().add_order(5, 44999.00, 7, true);
    coinbase2->get_order_book().add_order(7, 45002.00, 6, false);
    kraken2->get_order_book().add_order(9, 45000.50, 15, true);
    kraken2->get_order_book().add_order(11, 45002.50, 10, false);
    
    sor_no_fees.add_exchange(std::move(binance2), FeeSchedule(0.0010, 0.0010));
    sor_no_fees.add_exchange(std::move(coinbase2), FeeSchedule(0.0005, 0.0015));
    sor_no_fees.add_exchange(std::move(kraken2), FeeSchedule(0.0002, 0.0012));
    
    RoutingDecision no_fee_decision = sor_no_fees.route_order(105, 50000.0, 5, true);
    std::cout << "   Without fee consideration: Route to " 
              << exchange_to_string(no_fee_decision.exchange_id)
              << " @ $" << no_fee_decision.expected_price << std::endl;
    std::cout << "   With fee consideration: Route to "
              << exchange_to_string(buy_decision.exchange_id)
              << " @ $" << buy_decision.expected_price
              << " (Total: $" << buy_decision.total_cost << ")" << std::endl;
    
    return 0;
}