#ifndef MARKET_MAKER_H
#define MARKET_MAKER_H

#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include "smart_order_router.h"
#include "order_book.h"

struct Quote {
    double price;
    int quantity;
    bool is_buy_side;
    ExchangeID target_exchange;
    
    Quote(double p = 0.0, int q = 0, bool buy = true, ExchangeID ex = ExchangeID::Unknown)
        : price(p), quantity(q), is_buy_side(buy), target_exchange(ex) {}
};

struct MarketMakerQuotes {
    Quote buy_quote;
    Quote sell_quote;
    double theoretical_edge;  // Expected profit if both quotes fill
};

struct InventoryPosition {
    double base_inventory;    // e.g., BTC
    double quote_inventory;   // e.g., USD
    double base_value;        // Current value of base inventory
    double total_value;       // Total portfolio value
    double pnl;              // Profit and loss
};

struct MarketMakerParameters {
    // Spread parameters
    double base_spread_bps;      // Base spread in basis points (1 bp = 0.01%)
    double min_spread_bps;       // Minimum allowed spread
    double max_spread_bps;       // Maximum allowed spread
    
    // Inventory management
    double max_base_inventory;   // Maximum BTC to hold
    double max_quote_inventory;  // Maximum USD to hold
    double target_base_inventory; // Target BTC inventory
    
    // Risk parameters
    double inventory_skew_factor; // How much to skew quotes based on inventory
    double volatility_adjustment; // Spread adjustment based on volatility
    
    // Quote sizing
    double base_quote_size;      // Base size for quotes
    double min_quote_size;       // Minimum quote size
    double max_quote_size;       // Maximum quote size
    
    MarketMakerParameters()
        : base_spread_bps(10.0)       // 0.10% spread
        , min_spread_bps(5.0)         // 0.05% minimum
        , max_spread_bps(50.0)        // 0.50% maximum
        , max_base_inventory(10.0)     // 10 BTC max
        , max_quote_inventory(500000.0) // $500k max
        , target_base_inventory(5.0)    // Target 5 BTC
        , inventory_skew_factor(0.1)    // 10% skew per unit of inventory imbalance
        , volatility_adjustment(1.0)    // No volatility adjustment by default
        , base_quote_size(0.1)         // 0.1 BTC base size
        , min_quote_size(0.01)         // 0.01 BTC minimum
        , max_quote_size(1.0)          // 1.0 BTC maximum
    {}
};

class MarketMaker {
private:
    SmartOrderRouter* sor_;
    MarketMakerParameters params_;
    
    // Inventory tracking
    double base_inventory_;
    double quote_inventory_;
    double initial_base_inventory_;
    double initial_quote_inventory_;
    
    // Market data
    mutable double last_midpoint_;
    mutable double volatility_estimate_;
    
    // Performance tracking
    int quotes_placed_;
    int quotes_filled_;
    double total_volume_;
    double realized_pnl_;
    std::chrono::steady_clock::time_point start_time_;
    
    // Helper methods
    double calculate_midpoint() const;
    double calculate_spread() const;
    double calculate_inventory_skew() const;
    std::pair<double, double> calculate_quote_prices(double midpoint, double spread) const;
    int calculate_quote_size(bool is_buy_side) const;
    double estimate_volatility() const;
    
public:
    MarketMaker(SmartOrderRouter* sor, const MarketMakerParameters& params = MarketMakerParameters());
    
    // Initialize with starting inventory
    void initialize(double base_inventory, double quote_inventory);
    
    // Core market making logic - generates new quotes
    MarketMakerQuotes update_quotes();
    
    // Handle fill notifications
    void on_quote_filled(const Quote& filled_quote, double fill_price, int fill_quantity);
    
    // Risk management
    bool is_within_risk_limits() const;
    void adjust_parameters_for_risk();
    
    // Inventory management
    InventoryPosition get_inventory_position() const;
    double get_inventory_imbalance() const;
    
    // Performance metrics
    void print_performance_stats() const;
    double get_realized_pnl() const { return realized_pnl_; }
    double get_fill_rate() const;
    
    // Parameter updates
    void update_parameters(const MarketMakerParameters& new_params) { params_ = new_params; }
    const MarketMakerParameters& get_parameters() const { return params_; }
};

// Advanced market maker with multiple strategies
class AdvancedMarketMaker : public MarketMaker {
private:
    enum class Mode {
        Aggressive,    // Tighter spreads, larger sizes
        Neutral,       // Normal operation
        Defensive      // Wider spreads, smaller sizes
    };
    
    Mode current_mode_;
    double market_impact_estimate_;
    std::vector<double> recent_spreads_;
    
    Mode determine_mode() const;
    
public:
    AdvancedMarketMaker(SmartOrderRouter* sor, const MarketMakerParameters& params = MarketMakerParameters())
        : MarketMaker(sor, params)
        , current_mode_(Mode::Neutral)
        , market_impact_estimate_(0.0) {
        recent_spreads_.reserve(100);
    }
    
    // Enhanced quote generation with adaptive strategies
    MarketMakerQuotes update_quotes_advanced();
    
    // Market regime detection
    void analyze_market_conditions();
    
    // Print current strategy state
    void print_strategy_state() const;
};

#endif // MARKET_MAKER_H