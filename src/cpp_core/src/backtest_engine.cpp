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
#include <numeric>  // for std::accumulate
#include "order_book.h"
#include "market_maker.h"
#include "smart_order_router.h"

// Production-grade trade structure
struct DetailedTrade {
    int64_t trade_id;
    std::string timestamp;
    std::string symbol;
    double price;
    double quantity;  // Use double for precise quantities
    bool is_buy;
    int64_t buy_order_id;
    int64_t sell_order_id;
    double fee;
    double slippage;
    std::chrono::microseconds latency;
};

// Backtest metrics for analysis
struct BacktestMetrics {
    double total_pnl = 0.0;
    double realized_pnl = 0.0;
    double unrealized_pnl = 0.0;
    double sharpe_ratio = 0.0;
    double max_drawdown = 0.0;
    double win_rate = 0.0;
    int total_trades = 0;
    int winning_trades = 0;
    double total_volume = 0.0;
    double total_fees = 0.0;
    double avg_slippage = 0.0;
    std::vector<double> pnl_curve;
    std::vector<DetailedTrade> trades;
};

// Enhanced Mock Exchange with realistic features
class ProductionMockExchange : public IExchange {
public:
    // Market depth simulation
    struct MarketLevel {
        double price;
        double quantity;
        int num_orders;
    };
    
private:
    ExchangeID id_;
    std::string name_;
    OrderBook order_book_;
    ExchangeMetrics metrics_;
    bool is_available_;
    
    std::vector<MarketLevel> bid_depth_;
    std::vector<MarketLevel> ask_depth_;
    
    // Realistic features
    double base_latency_us_;
    double latency_stddev_us_;
    double market_impact_factor_;
    double adverse_fill_probability_;
    
public:
    ProductionMockExchange(ExchangeID id, const std::string& name, 
                          double base_latency_us = 100.0,
                          double market_impact_factor = 0.0001)
        : id_(id)
        , name_(name)
        , order_book_()
        , metrics_(base_latency_us / 1000.0, 0.95, 0.999)
        , is_available_(true)
        , base_latency_us_(base_latency_us)
        , latency_stddev_us_(base_latency_us * 0.2)
        , market_impact_factor_(market_impact_factor)
        , adverse_fill_probability_(0.1) {}
    
    OrderBook& get_order_book() override { return order_book_; }
    const OrderBook& get_order_book() const override { return order_book_; }
    ExchangeID get_id() const override { return id_; }
    std::string get_name() const override { return name_; }
    bool is_available() const override { return is_available_; }
    ExchangeMetrics get_metrics() const override { return metrics_; }
    
    void set_available(bool available) { is_available_ = available; }
    
    // Set market depth (multiple levels)
    void set_market_depth(const std::vector<MarketLevel>& bid_depth,
                         const std::vector<MarketLevel>& ask_depth) {
        bid_depth_ = bid_depth;
        ask_depth_ = ask_depth;
        
        // Update order book with top of book
        clear_market_orders();
        if (!bid_depth.empty()) {
            order_book_.add_order(-1, bid_depth[0].price, 
                                 static_cast<int>(bid_depth[0].quantity), true);
        }
        if (!ask_depth.empty()) {
            order_book_.add_order(-2, ask_depth[0].price, 
                                 static_cast<int>(ask_depth[0].quantity), false);
        }
    }
    
    // Simulate realistic order execution
    DetailedTrade execute_order(int64_t order_id, double price, double quantity, 
                               bool is_buy, double aggressive_price) {
        DetailedTrade trade;
        trade.trade_id = order_id;
        trade.is_buy = is_buy;
        trade.quantity = quantity;
        
        // Simulate latency
        double latency = base_latency_us_ + (rand() / double(RAND_MAX)) * latency_stddev_us_;
        trade.latency = std::chrono::microseconds(static_cast<int64_t>(latency));
        
        // Calculate market impact
        double impact = quantity * market_impact_factor_;
        
        // Determine execution price with slippage
        if (is_buy) {
            trade.price = aggressive_price * (1.0 + impact);
            trade.slippage = trade.price - aggressive_price;
        } else {
            trade.price = aggressive_price * (1.0 - impact);
            trade.slippage = aggressive_price - trade.price;
        }
        
        // Calculate fees (taker fee for aggressive orders)
        trade.fee = trade.price * quantity * 0.0015; // 15 basis points taker fee
        
        return trade;
    }
    
    void clear_market_orders() {
        // Clear the order book
        order_book_ = OrderBook();
    }
};

class ProductionBacktestEngine {
public:
    struct Config {
        bool enable_market_maker = true;
        bool enable_sor = true;
        int num_exchanges = 1;
        double initial_base_inventory = 1.0;
        double initial_quote_inventory = 10000.0;
        
        // Realistic simulation parameters
        bool enable_market_impact = true;
        bool enable_latency_simulation = true;
        double base_latency_us = 100.0;
        double market_impact_factor = 0.0001;
        double adverse_selection_prob = 0.1;
    };
    
private:
    Config config_;
    std::unique_ptr<SmartOrderRouter> sor_;
    std::unique_ptr<MarketMaker> market_maker_;
    std::vector<ProductionMockExchange*> exchanges_;
    BacktestMetrics metrics_;
    
    int64_t next_order_id_ = 1000;
    int64_t next_trade_id_ = 1;
    
    // PnL tracking
    std::deque<double> pnl_history_;
    double last_mark_price_ = 0.0;
    
public:
    ProductionBacktestEngine(const Config& cfg) : config_(cfg) {
        // Initialize SOR
        sor_ = std::make_unique<SmartOrderRouter>();
        
        // Initialize exchanges
        for (int i = 0; i < config_.num_exchanges; ++i) {
            std::string exchange_name = "Exchange_" + std::to_string(i + 1);
            auto exchange = std::make_unique<ProductionMockExchange>(
                static_cast<ExchangeID>(i + 1), 
                exchange_name,
                config_.base_latency_us,
                config_.market_impact_factor
            );
            
            exchanges_.push_back(exchange.get());
            
            FeeSchedule fees{0.0010, 0.0015};  // 10bps maker, 15bps taker
            sor_->add_exchange(std::move(exchange), fees);
        }
        
        // Initialize market maker with realistic parameters
        if (config_.enable_market_maker) {
            MarketMakerParameters params;
            params.base_spread_bps = 5.0;     // 5 bps spread (tight for liquid markets)
            params.min_spread_bps = 2.0;      // 2 bps minimum
            params.max_spread_bps = 20.0;     // 20 bps maximum
            params.base_quote_size = 0.1;     // 0.1 BTC base size
            params.min_quote_size = 0.01;     // 0.01 BTC minimum
            params.max_quote_size = 1.0;      // 1.0 BTC maximum
            params.max_base_inventory = 10.0;
            params.max_quote_inventory = 500000.0;
            params.target_base_inventory = config_.initial_base_inventory;
            params.inventory_skew_factor = 0.2;
            params.volatility_adjustment = 1.5;
            
            market_maker_ = std::make_unique<MarketMaker>(sor_.get(), params);
            market_maker_->initialize(config_.initial_base_inventory, 
                                    config_.initial_quote_inventory);
        }
    }
    
    void process_market_update(const std::string& line) {
        // Parse enhanced CSV format
        std::istringstream iss(line);
        std::string timestamp, symbol;
        double bid, ask, bid_size, ask_size, last_price, volume;
        
        std::getline(iss, timestamp, ',');
        std::getline(iss, symbol, ',');
        
        std::string bid_str, ask_str, bid_size_str, ask_size_str, last_price_str, volume_str;
        std::getline(iss, bid_str, ',');
        std::getline(iss, ask_str, ',');
        std::getline(iss, bid_size_str, ',');
        std::getline(iss, ask_size_str, ',');
        std::getline(iss, last_price_str, ',');
        std::getline(iss, volume_str, ',');
        
        try {
            bid = std::stod(bid_str);
            ask = std::stod(ask_str);
            bid_size = std::stod(bid_size_str);
            ask_size = std::stod(ask_size_str);
            last_price = std::stod(last_price_str);
            volume = std::stod(volume_str);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << line << " - " << e.what() << std::endl;
            return;
        }
        
        last_mark_price_ = (bid + ask) / 2.0;
        
        // Create realistic market depth
        std::vector<ProductionMockExchange::MarketLevel> bid_depth, ask_depth;
        
        // Generate 5 levels of depth with decreasing liquidity
        for (int i = 0; i < 5; ++i) {
            double level_decay = std::pow(0.7, i);  // Each level has 70% of previous level's size
            
            bid_depth.push_back({
                bid - i * 0.50,  // 50 cent intervals
                bid_size * level_decay,
                static_cast<int>(5 + i * 2)  // More orders at deeper levels
            });
            
            ask_depth.push_back({
                ask + i * 0.50,
                ask_size * level_decay,
                static_cast<int>(5 + i * 2)
            });
        }
        
        // Update all exchanges with market depth
        for (auto* exchange : exchanges_) {
            exchange->set_market_depth(bid_depth, ask_depth);
        }
        
        // Run market maker strategy
        if (market_maker_) {
            auto quotes = market_maker_->update_quotes();
            
            // Process buy quote
            if (quotes.buy_quote.quantity > 0) {
                process_quote(timestamp, symbol, quotes.buy_quote, bid, ask, true);
            }
            
            // Process sell quote
            if (quotes.sell_quote.quantity > 0) {
                process_quote(timestamp, symbol, quotes.sell_quote, bid, ask, false);
            }
        }
        
        // Update metrics
        update_metrics(timestamp);
        
        // Output state
        output_state(timestamp);
    }
    
private:
    void process_quote(const std::string& timestamp, const std::string& symbol,
                      const Quote& quote, double bid, double ask, bool is_buy) {
        // Determine if quote would cross the spread
        bool would_execute = is_buy ? (quote.price >= ask) : (quote.price <= bid);
        
        if (would_execute) {
            // Simulate execution
            double exec_price = is_buy ? ask : bid;
            
            // Get the exchange for realistic simulation
            auto* exchange = exchanges_[0];  // Simplified - use first exchange
            
            // Convert quantity to double
            double exec_quantity = quote.quantity / 100.0;  // Assuming quote.quantity is in cents
            
            // Execute with realistic simulation
            auto trade = exchange->execute_order(
                next_order_id_++, 
                quote.price, 
                exec_quantity,
                is_buy,
                exec_price
            );
            
            // Fill additional trade details
            trade.timestamp = timestamp;
            trade.symbol = symbol;
            trade.buy_order_id = is_buy ? next_order_id_ - 1 : -1;
            trade.sell_order_id = is_buy ? -1 : next_order_id_ - 1;
            
            // Notify market maker of fill
            market_maker_->on_quote_filled(quote, trade.price, 
                                         static_cast<int>(trade.quantity * 100));
            
            // Update metrics
            metrics_.trades.push_back(trade);
            metrics_.total_trades++;
            metrics_.total_volume += trade.quantity;
            metrics_.total_fees += trade.fee;
            metrics_.avg_slippage = (metrics_.avg_slippage * (metrics_.total_trades - 1) + 
                                   trade.slippage) / metrics_.total_trades;
            
            // Output trade
            output_trade(trade);
        }
    }
    
    void update_metrics(const std::string& timestamp) {
        if (!market_maker_) return;
        
        auto position = market_maker_->get_inventory_position();
        
        // Calculate current P&L
        double current_value = position.base_inventory * last_mark_price_ + 
                              position.quote_inventory;
        double initial_value = config_.initial_base_inventory * last_mark_price_ + 
                              config_.initial_quote_inventory;
        
        metrics_.total_pnl = current_value - initial_value;
        metrics_.pnl_curve.push_back(metrics_.total_pnl);
        
        // Update max drawdown
        if (!metrics_.pnl_curve.empty()) {
            double peak = *std::max_element(metrics_.pnl_curve.begin(), 
                                          metrics_.pnl_curve.end());
            double current_dd = (peak - metrics_.total_pnl) / peak;
            metrics_.max_drawdown = std::max(metrics_.max_drawdown, current_dd);
        }
        
        // Calculate Sharpe ratio (simplified - annualized)
        if (metrics_.pnl_curve.size() > 2) {
            std::vector<double> returns;
            for (size_t i = 1; i < metrics_.pnl_curve.size(); ++i) {
                returns.push_back(metrics_.pnl_curve[i] - metrics_.pnl_curve[i-1]);
            }
            
            double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / 
                               returns.size();
            double variance = 0.0;
            for (double r : returns) {
                variance += std::pow(r - mean_return, 2);
            }
            variance /= returns.size();
            
            double daily_sharpe = mean_return / std::sqrt(variance);
            metrics_.sharpe_ratio = daily_sharpe * std::sqrt(252);  // Annualized
        }
    }
    
    void output_trade(const DetailedTrade& trade) {
        std::cout << trade.timestamp << ","
                  << trade.symbol << ","
                  << trade.trade_id << ","
                  << (trade.is_buy ? "BUY" : "SELL") << ","
                  << std::fixed << std::setprecision(2) << trade.price << ","
                  << std::fixed << std::setprecision(6) << trade.quantity << ","
                  << trade.buy_order_id << ","
                  << trade.sell_order_id << ","
                  << std::fixed << std::setprecision(4) << trade.fee << ","
                  << std::fixed << std::setprecision(4) << trade.slippage << ","
                  << trade.latency.count() << std::endl;
    }
    
    void output_state(const std::string& timestamp) {
        if (!market_maker_) return;
        
        auto position = market_maker_->get_inventory_position();
        
        std::cerr << timestamp << ",MM_STATE,"
                  << std::fixed << std::setprecision(8) << position.base_inventory << ","
                  << std::fixed << std::setprecision(2) << position.quote_inventory << ","
                  << std::fixed << std::setprecision(2) << metrics_.total_pnl << ","
                  << std::fixed << std::setprecision(2) << metrics_.realized_pnl << ","
                  << std::fixed << std::setprecision(2) << metrics_.unrealized_pnl << ","
                  << std::fixed << std::setprecision(4) << metrics_.sharpe_ratio << ","
                  << std::fixed << std::setprecision(4) << metrics_.max_drawdown << std::endl;
    }
    
public:
    void print_final_metrics() {
        std::cerr << "\n=== Final Backtest Metrics ===" << std::endl;
        std::cerr << "Total P&L: $" << std::fixed << std::setprecision(2) 
                  << metrics_.total_pnl << std::endl;
        std::cerr << "Total Trades: " << metrics_.total_trades << std::endl;
        std::cerr << "Total Volume: " << std::fixed << std::setprecision(2) 
                  << metrics_.total_volume << " BTC" << std::endl;
        std::cerr << "Total Fees: $" << std::fixed << std::setprecision(2) 
                  << metrics_.total_fees << std::endl;
        std::cerr << "Average Slippage: $" << std::fixed << std::setprecision(4) 
                  << metrics_.avg_slippage << std::endl;
        std::cerr << "Sharpe Ratio: " << std::fixed << std::setprecision(4) 
                  << metrics_.sharpe_ratio << std::endl;
        std::cerr << "Max Drawdown: " << std::fixed << std::setprecision(2) 
                  << (metrics_.max_drawdown * 100) << "%" << std::endl;
    }
};

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " --backtest [options]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --no-mm              Disable market maker" << std::endl;
    std::cerr << "  --no-sor             Disable smart order router" << std::endl;
    std::cerr << "  --exchanges N        Number of exchanges (default: 1)" << std::endl;
    std::cerr << "  --latency US         Base latency in microseconds (default: 100)" << std::endl;
    std::cerr << "  --impact FACTOR      Market impact factor (default: 0.0001)" << std::endl;
    std::cerr << "  --no-impact          Disable market impact simulation" << std::endl;
    std::cerr << "  --no-latency         Disable latency simulation" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || std::strcmp(argv[1], "--backtest") != 0) {
        print_usage(argv[0]);
        return 1;
    }
    
    ProductionBacktestEngine::Config config;
    
    // Parse command line options
    for (int i = 2; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--no-mm") {
            config.enable_market_maker = false;
        } else if (arg == "--no-sor") {
            config.enable_sor = false;
        } else if (arg == "--exchanges" && i + 1 < argc) {
            config.num_exchanges = std::stoi(argv[++i]);
        } else if (arg == "--latency" && i + 1 < argc) {
            config.base_latency_us = std::stod(argv[++i]);
        } else if (arg == "--impact" && i + 1 < argc) {
            config.market_impact_factor = std::stod(argv[++i]);
        } else if (arg == "--no-impact") {
            config.enable_market_impact = false;
        } else if (arg == "--no-latency") {
            config.enable_latency_simulation = false;
        }
    }
    
    ProductionBacktestEngine engine(config);
    
    // Process market data from stdin
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line[0] != '#') {
            engine.process_market_update(line);
        }
    }
    
    // Print final metrics
    engine.print_final_metrics();
    
    return 0;
}