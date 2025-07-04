#include <iostream>
#include <iomanip>
#include <memory>
#include <thread>
#include <chrono>
#include <random>
#include "market_maker.h"
#include "smart_order_router.h"
#include "order_book.h"

// Mock exchange implementation (same as in sor_test.cpp)
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

void setup_market_data(MockExchange* binance, MockExchange* coinbase, MockExchange* kraken) {
    // Clear existing orders
    binance->get_order_book() = OrderBook();
    coinbase->get_order_book() = OrderBook();
    kraken->get_order_book() = OrderBook();
    
    // Binance: Tight spread
    binance->get_order_book().add_order(1, 45000.00, 10, true);   // Buy
    binance->get_order_book().add_order(2, 44999.50, 5, true);    // Buy
    binance->get_order_book().add_order(3, 45001.00, 8, false);   // Sell
    binance->get_order_book().add_order(4, 45001.50, 12, false);  // Sell
    
    // Coinbase: Wider spread
    coinbase->get_order_book().add_order(5, 44999.00, 7, true);   // Buy
    coinbase->get_order_book().add_order(6, 44998.00, 3, true);   // Buy
    coinbase->get_order_book().add_order(7, 45002.00, 6, false);  // Sell
    coinbase->get_order_book().add_order(8, 45003.00, 9, false);  // Sell
    
    // Kraken: Different prices
    kraken->get_order_book().add_order(9, 45000.50, 15, true);    // Buy
    kraken->get_order_book().add_order(10, 45000.00, 5, true);    // Buy
    kraken->get_order_book().add_order(11, 45002.50, 10, false);  // Sell
    kraken->get_order_book().add_order(12, 45003.50, 8, false);   // Sell
}

void simulate_market_movement(MockExchange* exchange, std::mt19937& gen, 
                            std::uniform_real_distribution<>& price_dist,
                            std::uniform_int_distribution<>& size_dist) {
    // Add some randomness to the market
    double price_change = price_dist(gen);
    int size_change = size_dist(gen);
    
    // Update best bid/ask
    auto& book = exchange->get_order_book();
    double best_bid = book.get_best_bid();
    double best_ask = book.get_best_ask();
    
    if (best_bid > 0) {
        book.cancel_order(1);  // Cancel old best bid
        book.add_order(1, best_bid + price_change, 10 + size_change, true);
    }
    
    if (best_ask < std::numeric_limits<double>::infinity()) {
        book.cancel_order(3);  // Cancel old best ask
        book.add_order(3, best_ask + price_change, 8 + size_change, false);
    }
}

int main() {
    std::cout << "=== Market Maker Test ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    
    // Create mock exchanges
    auto binance = std::make_unique<MockExchange>(
        ExchangeID::Binance, 
        "Binance",
        ExchangeMetrics(5, 0.98, 0.999)
    );
    
    auto coinbase = std::make_unique<MockExchange>(
        ExchangeID::Coinbase,
        "Coinbase",
        ExchangeMetrics(15, 0.95, 0.998)
    );
    
    auto kraken = std::make_unique<MockExchange>(
        ExchangeID::Kraken,
        "Kraken",
        ExchangeMetrics(25, 0.92, 0.997)
    );
    
    // Keep raw pointers for market simulation
    auto* binance_ptr = binance.get();
    auto* coinbase_ptr = coinbase.get();
    auto* kraken_ptr = kraken.get();
    
    // Setup initial market data
    setup_market_data(binance_ptr, coinbase_ptr, kraken_ptr);
    
    // Create Smart Order Router
    SmartOrderRouter sor(true, true);
    sor.add_exchange(std::move(binance), FeeSchedule(0.0010, 0.0010));
    sor.add_exchange(std::move(coinbase), FeeSchedule(0.0005, 0.0015));
    sor.add_exchange(std::move(kraken), FeeSchedule(0.0002, 0.0012));
    
    // Create Market Maker with custom parameters
    MarketMakerParameters params;
    params.base_spread_bps = 20.0;        // 0.20% spread
    params.base_quote_size = 0.5;         // 0.5 BTC per quote
    params.target_base_inventory = 5.0;    // Target 5 BTC
    params.inventory_skew_factor = 0.2;    // 20% skew adjustment
    
    MarketMaker mm(&sor, params);
    
    // Initialize with starting inventory
    double starting_btc = 5.0;
    double starting_usd = 250000.0;
    mm.initialize(starting_btc, starting_usd);
    
    // Test 1: Generate initial quotes
    std::cout << "\n1. Generating Initial Quotes" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    auto quotes = mm.update_quotes();
    
    std::cout << "Buy Quote:" << std::endl;
    std::cout << "  Price: $" << quotes.buy_quote.price << std::endl;
    std::cout << "  Size: " << quotes.buy_quote.quantity / 100.0 << " BTC" << std::endl;
    std::cout << "  Exchange: " << exchange_to_string(quotes.buy_quote.target_exchange) << std::endl;
    
    std::cout << "\nSell Quote:" << std::endl;
    std::cout << "  Price: $" << quotes.sell_quote.price << std::endl;
    std::cout << "  Size: " << quotes.sell_quote.quantity / 100.0 << " BTC" << std::endl;
    std::cout << "  Exchange: " << exchange_to_string(quotes.sell_quote.target_exchange) << std::endl;
    
    std::cout << "\nTheoretical Edge: $" << quotes.theoretical_edge << std::endl;
    
    // Test 2: Simulate a buy quote fill
    std::cout << "\n2. Simulating Buy Quote Fill" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    mm.on_quote_filled(quotes.buy_quote, quotes.buy_quote.price, quotes.buy_quote.quantity);
    
    auto pos = mm.get_inventory_position();
    std::cout << "Updated Inventory:" << std::endl;
    std::cout << "  BTC: " << pos.base_inventory << std::endl;
    std::cout << "  USD: $" << pos.quote_inventory << std::endl;
    std::cout << "  Total Value: $" << pos.total_value << std::endl;
    std::cout << "  P&L: $" << pos.pnl << std::endl;
    
    // Test 3: Generate new quotes with updated inventory
    std::cout << "\n3. Generating Quotes with New Inventory" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    quotes = mm.update_quotes();
    
    std::cout << "New quotes (notice inventory skew effect):" << std::endl;
    std::cout << "  Buy: $" << quotes.buy_quote.price << " for " 
              << quotes.buy_quote.quantity / 100.0 << " BTC" << std::endl;
    std::cout << "  Sell: $" << quotes.sell_quote.price << " for " 
              << quotes.sell_quote.quantity / 100.0 << " BTC" << std::endl;
    std::cout << "  Inventory imbalance: " << (mm.get_inventory_imbalance() * 100) << "%" << std::endl;
    
    // Test 4: Simulate multiple trades
    std::cout << "\n4. Simulating Trading Session" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> fill_prob(0.0, 1.0);
    std::uniform_real_distribution<> price_movement(-5.0, 5.0);
    std::uniform_int_distribution<> size_variation(-2, 2);
    
    for (int i = 0; i < 10; ++i) {
        // Simulate market movement
        simulate_market_movement(binance_ptr, gen, price_movement, size_variation);
        
        // Generate new quotes
        quotes = mm.update_quotes();
        
        // Randomly fill some quotes
        if (fill_prob(gen) < 0.3) {  // 30% fill rate
            if (fill_prob(gen) < 0.5) {
                // Fill buy quote
                mm.on_quote_filled(quotes.buy_quote, quotes.buy_quote.price, quotes.buy_quote.quantity);
                std::cout << "Trade " << i+1 << ": Bought " << quotes.buy_quote.quantity / 100.0 
                          << " BTC @ $" << quotes.buy_quote.price << std::endl;
            } else {
                // Fill sell quote
                mm.on_quote_filled(quotes.sell_quote, quotes.sell_quote.price, quotes.sell_quote.quantity);
                std::cout << "Trade " << i+1 << ": Sold " << quotes.sell_quote.quantity / 100.0 
                          << " BTC @ $" << quotes.sell_quote.price << std::endl;
            }
        }
        
        // Brief pause to simulate time passing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Test 5: Print final performance stats
    std::cout << "\n5. Final Performance Report" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    mm.print_performance_stats();
    
    // Test 6: Risk management demonstration
    std::cout << "\n6. Risk Management Check" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    std::cout << "Within risk limits: " << (mm.is_within_risk_limits() ? "YES" : "NO") << std::endl;
    
    // Force inventory imbalance
    std::cout << "\nSimulating large inventory imbalance..." << std::endl;
    for (int i = 0; i < 5; ++i) {
        mm.on_quote_filled(quotes.buy_quote, quotes.buy_quote.price, 100);  // Buy 1 BTC each time
    }
    
    pos = mm.get_inventory_position();
    std::cout << "After buying 5 BTC:" << std::endl;
    std::cout << "  BTC inventory: " << pos.base_inventory << std::endl;
    std::cout << "  Inventory imbalance: " << (mm.get_inventory_imbalance() * 100) << "%" << std::endl;
    std::cout << "  Within risk limits: " << (mm.is_within_risk_limits() ? "YES" : "NO") << std::endl;
    
    // Generate quotes with high inventory
    quotes = mm.update_quotes();
    std::cout << "\nQuotes with high inventory (notice the skew):" << std::endl;
    std::cout << "  Buy: $" << quotes.buy_quote.price << " (smaller size: " 
              << quotes.buy_quote.quantity / 100.0 << " BTC)" << std::endl;
    std::cout << "  Sell: $" << quotes.sell_quote.price << " (larger size: " 
              << quotes.sell_quote.quantity / 100.0 << " BTC)" << std::endl;
    
    return 0;
}