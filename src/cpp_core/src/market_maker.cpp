#include "market_maker.h"
#include <iostream>
#include <iomanip>
#include <cmath>

MarketMaker::MarketMaker(SmartOrderRouter* sor, const MarketMakerParameters& params)
    : sor_(sor)
    , params_(params)
    , base_inventory_(0.0)
    , quote_inventory_(0.0)
    , initial_base_inventory_(0.0)
    , initial_quote_inventory_(0.0)
    , last_midpoint_(0.0)
    , volatility_estimate_(0.001)  // 0.1% default volatility
    , quotes_placed_(0)
    , quotes_filled_(0)
    , total_volume_(0.0)
    , realized_pnl_(0.0)
    , start_time_(std::chrono::steady_clock::now()) {
}

void MarketMaker::initialize(double base_inventory, double quote_inventory) {
    base_inventory_ = base_inventory;
    quote_inventory_ = quote_inventory;
    initial_base_inventory_ = base_inventory;
    initial_quote_inventory_ = quote_inventory;
    
    std::cout << "Market Maker initialized with:" << std::endl;
    std::cout << "  Base inventory: " << base_inventory_ << " BTC" << std::endl;
    std::cout << "  Quote inventory: $" << quote_inventory_ << std::endl;
}

double MarketMaker::calculate_midpoint() const {
    auto market_data = sor_->get_aggregated_market_data();
    
    if (market_data.best_bid <= 0 || market_data.best_ask >= std::numeric_limits<double>::max()) {
        // No valid market, use last known midpoint
        return last_midpoint_;
    }
    
    double midpoint = (market_data.best_bid + market_data.best_ask) / 2.0;
    last_midpoint_ = midpoint;  // Update cached value
    return midpoint;
}

double MarketMaker::calculate_spread() const {
    // Start with base spread
    double spread_bps = params_.base_spread_bps;
    
    // Adjust for volatility
    spread_bps *= (1.0 + volatility_estimate_ * params_.volatility_adjustment);
    
    // Adjust for inventory imbalance
    double inventory_skew = calculate_inventory_skew();
    spread_bps *= (1.0 + std::abs(inventory_skew) * 0.5);
    
    // Enforce limits
    spread_bps = std::max(params_.min_spread_bps, std::min(params_.max_spread_bps, spread_bps));
    
    return spread_bps / 10000.0;  // Convert basis points to decimal
}

double MarketMaker::calculate_inventory_skew() const {
    if (params_.target_base_inventory <= 0) {
        return 0.0;
    }
    
    double inventory_ratio = base_inventory_ / params_.target_base_inventory;
    double imbalance = inventory_ratio - 1.0;
    
    // Skew factor: positive means too much inventory (lower bid, raise ask)
    return imbalance * params_.inventory_skew_factor;
}

std::pair<double, double> MarketMaker::calculate_quote_prices(double midpoint, double spread) const {
    double half_spread = spread / 2.0;
    double inventory_skew = calculate_inventory_skew();
    
    // Adjust prices based on inventory
    // If we have too much inventory, lower bid and raise ask
    double bid_adjustment = 1.0 - half_spread - (inventory_skew * half_spread);
    double ask_adjustment = 1.0 + half_spread + (inventory_skew * half_spread);
    
    double bid_price = midpoint * bid_adjustment;
    double ask_price = midpoint * ask_adjustment;
    
    return {bid_price, ask_price};
}

int MarketMaker::calculate_quote_size(bool is_buy_side) const {
    double base_size = params_.base_quote_size;
    
    // Adjust size based on inventory
    if (is_buy_side) {
        // Reduce buy size if we have too much base inventory
        double inventory_ratio = base_inventory_ / params_.max_base_inventory;
        base_size *= (1.0 - inventory_ratio * 0.5);
    } else {
        // Reduce sell size if we have too little base inventory
        double inventory_ratio = base_inventory_ / params_.target_base_inventory;
        base_size *= std::min(1.0, inventory_ratio);
    }
    
    // Convert to integer quantity (assuming whole units for simplicity)
    int quantity = static_cast<int>(base_size * 100);  // Convert to smallest unit
    
    // Enforce limits
    quantity = std::max(static_cast<int>(params_.min_quote_size * 100), 
                       std::min(static_cast<int>(params_.max_quote_size * 100), quantity));
    
    return quantity;
}

MarketMakerQuotes MarketMaker::update_quotes() {
    MarketMakerQuotes quotes;
    
    // Get current market state
    double midpoint = calculate_midpoint();
    if (midpoint <= 0) {
        std::cerr << "Invalid market midpoint" << std::endl;
        return quotes;
    }
    
    // Update last known midpoint
    last_midpoint_ = midpoint;
    
    // Calculate spread and quote prices
    double spread = calculate_spread();
    auto [bid_price, ask_price] = calculate_quote_prices(midpoint, spread);
    
    // Calculate quote sizes
    int buy_size = calculate_quote_size(true);
    int sell_size = calculate_quote_size(false);
    
    // Determine best exchanges for each quote
    auto buy_routing = sor_->route_order(quotes_placed_++, bid_price, buy_size, true);
    auto sell_routing = sor_->route_order(quotes_placed_++, ask_price, sell_size, false);
    
    // Create quotes
    quotes.buy_quote = Quote(bid_price, buy_size, true, buy_routing.exchange_id);
    quotes.sell_quote = Quote(ask_price, sell_size, false, sell_routing.exchange_id);
    
    // Calculate theoretical edge
    quotes.theoretical_edge = (ask_price - bid_price) - 
                             (buy_routing.expected_fee + sell_routing.expected_fee);
    
    quotes_placed_ += 2;
    
    return quotes;
}

void MarketMaker::on_quote_filled(const Quote& filled_quote, double fill_price, int fill_quantity) {
    quotes_filled_++;
    total_volume_ += fill_quantity;
    
    if (filled_quote.is_buy_side) {
        // We bought, increase base inventory, decrease quote inventory
        base_inventory_ += fill_quantity / 100.0;  // Convert from smallest unit
        quote_inventory_ -= fill_price * fill_quantity / 100.0;
        
        std::cout << "Buy quote filled: +" << fill_quantity / 100.0 << " BTC @ $" << fill_price << std::endl;
    } else {
        // We sold, decrease base inventory, increase quote inventory
        base_inventory_ -= fill_quantity / 100.0;
        quote_inventory_ += fill_price * fill_quantity / 100.0;
        
        std::cout << "Sell quote filled: -" << fill_quantity / 100.0 << " BTC @ $" << fill_price << std::endl;
    }
    
    // Update realized PnL (simplified - assumes we can always close at midpoint)
    double current_midpoint = calculate_midpoint();
    double position_value = base_inventory_ * current_midpoint + quote_inventory_;
    double initial_value = initial_base_inventory_ * current_midpoint + initial_quote_inventory_;
    realized_pnl_ = position_value - initial_value;
}

bool MarketMaker::is_within_risk_limits() const {
    // Check inventory limits
    if (base_inventory_ > params_.max_base_inventory || 
        base_inventory_ < 0) {
        return false;
    }
    
    if (quote_inventory_ > params_.max_quote_inventory || 
        quote_inventory_ < -params_.max_quote_inventory * 0.1) {  // Allow small negative
        return false;
    }
    
    // Check position limits
    double current_midpoint = calculate_midpoint();
    double position_value = std::abs(base_inventory_ * current_midpoint);
    double max_position_value = params_.max_base_inventory * current_midpoint;
    
    return position_value <= max_position_value * 1.1;  // 10% buffer
}

void MarketMaker::adjust_parameters_for_risk() {
    if (!is_within_risk_limits()) {
        // Widen spreads if outside risk limits
        params_.base_spread_bps *= 1.5;
        params_.base_quote_size *= 0.5;
        
        std::cout << "Risk limits exceeded - adjusting parameters" << std::endl;
    }
}

InventoryPosition MarketMaker::get_inventory_position() const {
    InventoryPosition pos;
    pos.base_inventory = base_inventory_;
    pos.quote_inventory = quote_inventory_;
    
    double current_midpoint = calculate_midpoint();
    pos.base_value = base_inventory_ * current_midpoint;
    pos.total_value = pos.base_value + quote_inventory_;
    
    double initial_value = initial_base_inventory_ * current_midpoint + initial_quote_inventory_;
    pos.pnl = pos.total_value - initial_value;
    
    return pos;
}

double MarketMaker::get_inventory_imbalance() const {
    if (params_.target_base_inventory <= 0) {
        return 0.0;
    }
    
    return (base_inventory_ - params_.target_base_inventory) / params_.target_base_inventory;
}

double MarketMaker::get_fill_rate() const {
    if (quotes_placed_ == 0) {
        return 0.0;
    }
    
    return static_cast<double>(quotes_filled_) / quotes_placed_;
}

void MarketMaker::print_performance_stats() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    
    std::cout << "\n=== Market Maker Performance Stats ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    
    std::cout << "Runtime: " << duration.count() << " seconds" << std::endl;
    std::cout << "Quotes placed: " << quotes_placed_ << std::endl;
    std::cout << "Quotes filled: " << quotes_filled_ << std::endl;
    std::cout << "Fill rate: " << (get_fill_rate() * 100) << "%" << std::endl;
    std::cout << "Total volume: " << (total_volume_ / 100.0) << " BTC" << std::endl;
    
    auto pos = get_inventory_position();
    std::cout << "\nInventory Position:" << std::endl;
    std::cout << "  Base: " << pos.base_inventory << " BTC (value: $" << pos.base_value << ")" << std::endl;
    std::cout << "  Quote: $" << pos.quote_inventory << std::endl;
    std::cout << "  Total value: $" << pos.total_value << std::endl;
    std::cout << "  P&L: $" << pos.pnl << " (" 
              << ((pos.pnl / (initial_base_inventory_ * calculate_midpoint() + initial_quote_inventory_)) * 100) 
              << "%)" << std::endl;
    
    std::cout << "\nCurrent Parameters:" << std::endl;
    std::cout << "  Base spread: " << params_.base_spread_bps << " bps" << std::endl;
    std::cout << "  Quote size: " << params_.base_quote_size << " BTC" << std::endl;
    std::cout << "  Inventory skew: " << (calculate_inventory_skew() * 100) << "%" << std::endl;
}

double MarketMaker::estimate_volatility() const {
    // Simplified volatility estimate based on spread
    auto market_data = sor_->get_aggregated_market_data();
    
    if (market_data.best_bid <= 0 || market_data.best_ask >= std::numeric_limits<double>::max()) {
        return volatility_estimate_;  // Return last estimate
    }
    
    double spread = (market_data.best_ask - market_data.best_bid) / market_data.best_bid;
    
    // Smooth the estimate
    volatility_estimate_ = volatility_estimate_ * 0.9 + spread * 0.1;
    
    return volatility_estimate_;
}