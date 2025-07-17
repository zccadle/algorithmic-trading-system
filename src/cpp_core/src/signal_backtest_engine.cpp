#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>
#include <deque>
#include <algorithm>
#include <cmath>
#include <numeric>
#include "order_book.h"

// Enhanced trade structure for signal-based execution
struct SignalTrade {
    int64_t trade_id;
    std::string timestamp;
    std::string symbol;
    std::string side;  // "BUY" or "SELL"
    double price;
    double quantity;
    double fee;
    double slippage;
    std::string signal_type;  // "ENTRY", "EXIT", "REBALANCE"
};

// Signal-based backtest configuration
struct SignalBacktestConfig {
    double initial_capital = 100000.0;
    double position_size_fraction = 0.1;  // Fraction of capital per trade
    double maker_fee = 0.0010;  // 10 bps
    double taker_fee = 0.0015;  // 15 bps
    double market_impact_factor = 0.0001;
    bool use_market_orders = true;  // Always use market orders for signals
    double max_slippage_bps = 50.0;  // Maximum allowed slippage
};

// Mock exchange for signal-based trading
class SignalMockExchange {
private:
    std::string name_;
    double current_bid_;
    double current_ask_;
    double bid_size_;
    double ask_size_;
    
public:
    SignalMockExchange(const std::string& name)
        : name_(name)
        , current_bid_(0.0)
        , current_ask_(0.0)
        , bid_size_(0.0)
        , ask_size_(0.0) {}
    
    void update_market(double bid, double ask, double bid_size, double ask_size) {
        current_bid_ = bid;
        current_ask_ = ask;
        bid_size_ = bid_size;
        ask_size_ = ask_size;
    }
    
    SignalTrade execute_market_order(const std::string& side, double quantity, 
                                   double market_impact_factor, bool is_taker = true) {
        SignalTrade trade;
        trade.side = side;
        trade.quantity = quantity;
        
        if (side == "BUY") {
            // Buy at ask (taker)
            double base_price = current_ask_;
            
            // Apply market impact (price moves against us)
            double impact = quantity * market_impact_factor;
            trade.price = base_price * (1 + impact);
            
            // Calculate slippage
            trade.slippage = (trade.price - base_price) * quantity;
        } else {
            // Sell at bid (taker)
            double base_price = current_bid_;
            
            // Apply market impact
            double impact = quantity * market_impact_factor;
            trade.price = base_price * (1 - impact);
            
            // Calculate slippage
            trade.slippage = (base_price - trade.price) * quantity;
        }
        
        // Calculate fee (always taker for market orders)
        trade.fee = trade.price * trade.quantity * 0.0015;  // 15 bps taker fee
        
        return trade;
    }
};

// Main signal-based backtest engine
class SignalBacktestEngine {
private:
    SignalBacktestConfig config_;
    std::unique_ptr<SignalMockExchange> exchange_;
    
    // Portfolio state
    double cash_;
    double position_;  // Current position in base asset
    double last_signal_position_;  // Track signal changes
    
    // Trade tracking
    int64_t next_trade_id_;
    std::vector<SignalTrade> trades_;
    
    // Market state
    double last_price_;
    std::string current_timestamp_;
    
public:
    SignalBacktestEngine(const SignalBacktestConfig& config)
        : config_(config)
        , cash_(config.initial_capital)
        , position_(0.0)
        , last_signal_position_(0.0)
        , next_trade_id_(1)
        , last_price_(0.0) {
        
        // Initialize exchange
        exchange_ = std::make_unique<SignalMockExchange>("PRIMARY");
    }
    
    void process_market_data_with_signal(const std::string& line) {
        // Parse CSV: timestamp,symbol,bid,ask,bid_size,ask_size,last_price,volume,signal_position
        std::istringstream iss(line);
        std::string timestamp, symbol;
        double bid, ask, bid_size, ask_size, last_price, volume, signal_position;
        
        // Parse fields
        std::getline(iss, timestamp, ',');
        std::getline(iss, symbol, ',');
        
        std::string field;
        std::getline(iss, field, ','); bid = std::stod(field);
        std::getline(iss, field, ','); ask = std::stod(field);
        std::getline(iss, field, ','); bid_size = std::stod(field);
        std::getline(iss, field, ','); ask_size = std::stod(field);
        std::getline(iss, field, ','); last_price = std::stod(field);
        std::getline(iss, field, ','); volume = std::stod(field);
        std::getline(iss, field, ','); signal_position = std::stod(field);
        
        current_timestamp_ = timestamp;
        last_price_ = last_price;
        
        // Update exchange with current market
        exchange_->update_market(bid, ask, bid_size, ask_size);
        
        // Check for signal change
        if (signal_position != last_signal_position_) {
            handle_signal_change(timestamp, symbol, last_signal_position_, signal_position);
            last_signal_position_ = signal_position;
        }
        
        // Output current state
        output_state();
    }
    
private:
    void handle_signal_change(const std::string& timestamp, const std::string& symbol,
                            double old_position, double new_position) {
        double position_delta = new_position - old_position;
        
        if (std::abs(position_delta) < 1e-9) {
            return;  // No real change
        }
        
        // Calculate trade size based on available capital and position sizing
        double trade_quantity = std::abs(position_delta);
        
        // Determine side
        std::string side = position_delta > 0 ? "BUY" : "SELL";
        
        // Execute trade through exchange
        SignalTrade trade = exchange_->execute_market_order(
            side, trade_quantity, config_.market_impact_factor
        );
        
        // Fill in additional trade details
        trade.trade_id = next_trade_id_++;
        trade.timestamp = timestamp;
        trade.symbol = symbol;
        trade.signal_type = determine_signal_type(old_position, new_position);
        
        // Update portfolio
        if (side == "BUY") {
            cash_ -= (trade.price * trade.quantity + trade.fee);
            position_ += trade.quantity;
        } else {
            cash_ += (trade.price * trade.quantity - trade.fee);
            position_ -= trade.quantity;
        }
        
        // Record trade
        trades_.push_back(trade);
        
        // Output trade immediately
        output_trade(trade);
    }
    
    std::string determine_signal_type(double old_pos, double new_pos) {
        if (old_pos == 0 && new_pos != 0) return "ENTRY";
        if (old_pos != 0 && new_pos == 0) return "EXIT";
        return "REBALANCE";
    }
    
    void output_trade(const SignalTrade& trade) {
        // Output format: TRADE,timestamp,symbol,trade_id,side,price,quantity,fee,slippage,signal_type
        std::cout << "TRADE,"
                  << trade.timestamp << ","
                  << trade.symbol << ","
                  << trade.trade_id << ","
                  << trade.side << ","
                  << std::fixed << std::setprecision(2) << trade.price << ","
                  << std::fixed << std::setprecision(6) << trade.quantity << ","
                  << std::fixed << std::setprecision(4) << trade.fee << ","
                  << std::fixed << std::setprecision(4) << trade.slippage << ","
                  << trade.signal_type << std::endl;
    }
    
    void output_state() {
        // Output portfolio state to stderr for monitoring
        double holdings_value = position_ * last_price_;
        double total_value = cash_ + holdings_value;
        
        std::cerr << "STATE,"
                  << current_timestamp_ << ","
                  << std::fixed << std::setprecision(2) << cash_ << ","
                  << std::fixed << std::setprecision(6) << position_ << ","
                  << std::fixed << std::setprecision(2) << holdings_value << ","
                  << std::fixed << std::setprecision(2) << total_value << ","
                  << std::fixed << std::setprecision(2) << last_price_ << std::endl;
    }
    
public:
    void print_summary() {
        std::cerr << "\n=== Signal Backtest Summary ===" << std::endl;
        std::cerr << "Total Trades: " << trades_.size() << std::endl;
        
        if (!trades_.empty()) {
            double total_fees = 0.0;
            double total_slippage = 0.0;
            int buy_trades = 0;
            int sell_trades = 0;
            
            for (const auto& trade : trades_) {
                total_fees += trade.fee;
                total_slippage += trade.slippage;
                if (trade.side == "BUY") buy_trades++;
                else sell_trades++;
            }
            
            std::cerr << "Buy Trades: " << buy_trades << std::endl;
            std::cerr << "Sell Trades: " << sell_trades << std::endl;
            std::cerr << "Total Fees: $" << std::fixed << std::setprecision(2) 
                      << total_fees << std::endl;
            std::cerr << "Total Slippage: $" << std::fixed << std::setprecision(2) 
                      << total_slippage << std::endl;
            
            double final_value = cash_ + position_ * last_price_;
            double total_return = (final_value - config_.initial_capital) / config_.initial_capital;
            
            std::cerr << "Initial Capital: $" << std::fixed << std::setprecision(2) 
                      << config_.initial_capital << std::endl;
            std::cerr << "Final Value: $" << std::fixed << std::setprecision(2) 
                      << final_value << std::endl;
            std::cerr << "Total Return: " << std::fixed << std::setprecision(2) 
                      << (total_return * 100) << "%" << std::endl;
        }
    }
};

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [options]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --capital AMOUNT     Initial capital (default: 100000)" << std::endl;
    std::cerr << "  --size FRACTION      Position size fraction (default: 0.1)" << std::endl;
    std::cerr << "  --impact FACTOR      Market impact factor (default: 0.0001)" << std::endl;
    std::cerr << "  --maker-fee BPS      Maker fee in basis points (default: 10)" << std::endl;
    std::cerr << "  --taker-fee BPS      Taker fee in basis points (default: 15)" << std::endl;
}

int main(int argc, char* argv[]) {
    SignalBacktestConfig config;
    
    // Parse command line options
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--capital" && i + 1 < argc) {
            config.initial_capital = std::stod(argv[++i]);
        } else if (arg == "--size" && i + 1 < argc) {
            config.position_size_fraction = std::stod(argv[++i]);
        } else if (arg == "--impact" && i + 1 < argc) {
            config.market_impact_factor = std::stod(argv[++i]);
        } else if (arg == "--maker-fee" && i + 1 < argc) {
            config.maker_fee = std::stod(argv[++i]) / 10000.0;  // Convert bps to decimal
        } else if (arg == "--taker-fee" && i + 1 < argc) {
            config.taker_fee = std::stod(argv[++i]) / 10000.0;
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    SignalBacktestEngine engine(config);
    
    // Skip header if present
    std::string line;
    bool first_line = true;
    
    while (std::getline(std::cin, line)) {
        if (first_line) {
            first_line = false;
            // Check if it's a header
            if (line.find("timestamp") != std::string::npos) {
                continue;  // Skip header
            }
        }
        
        if (!line.empty()) {
            try {
                engine.process_market_data_with_signal(line);
            } catch (const std::exception& e) {
                std::cerr << "Error processing line: " << e.what() << std::endl;
                std::cerr << "Line: " << line << std::endl;
            }
        }
    }
    
    // Print summary
    engine.print_summary();
    
    return 0;
}