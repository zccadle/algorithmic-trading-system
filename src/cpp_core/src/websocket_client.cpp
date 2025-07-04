#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <iomanip>
#include "order_book.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;
using json = nlohmann::json;

class BinanceWebSocketClient {
private:
    net::io_context& ioc_;
    ssl::context ssl_ctx_;
    tcp::resolver resolver_;
    websocket::stream<beast::ssl_stream<tcp::socket>> ws_;
    std::string host_;
    std::string port_;
    std::string target_;
    beast::flat_buffer buffer_;
    OrderBook order_book_;
    int next_order_id_;
    int update_count_;
    std::unordered_map<std::string, std::vector<int>> buy_orders_;  // price -> order_ids
    std::unordered_map<std::string, std::vector<int>> sell_orders_; // price -> order_ids
    
public:
    BinanceWebSocketClient(net::io_context& ioc)
        : ioc_(ioc)
        , ssl_ctx_(ssl::context::tlsv12_client)
        , resolver_(ioc)
        , ws_(ioc, ssl_ctx_)
        , host_("stream.binance.com")
        , port_("9443")
        , target_("/ws/btcusdt@depth")
        , next_order_id_(1)
        , update_count_(0) {
        
        // Configure SSL
        ssl_ctx_.set_default_verify_paths();
        ssl_ctx_.set_verify_mode(ssl::verify_peer);
    }
    
    void run() {
        // Look up the domain name
        auto const results = resolver_.resolve(host_, port_);
        
        // Make the connection on the IP address we get from a lookup
        auto ep = net::connect(get_lowest_layer(ws_), results);
        
        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str())) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category()),
                "Failed to set SNI Hostname");
        }
        
        // Perform the SSL handshake
        ws_.next_layer().handshake(ssl::stream_base::client);
        
        // Set a decorator to change the User-Agent of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-client-coro");
            }));
        
        // Perform the websocket handshake
        ws_.handshake(host_, target_);
        
        std::cout << "Connected to Binance WebSocket stream: " << target_ << std::endl;
        std::cout << "Listening for BTC/USDT depth updates...\n" << std::endl;
        
        // Start reading messages
        read_loop();
    }
    
    void read_loop() {
        std::cout << std::fixed << std::setprecision(2);
        
        while (true) {
            try {
                // Read a message into our buffer
                ws_.read(buffer_);
                
                // The make_printable() function helps print a ConstBufferSequence
                std::string message = beast::buffers_to_string(buffer_.data());
                
                // Parse JSON
                json data = json::parse(message);
                
                update_count_++;
                std::cout << "=== Update #" << update_count_ << " ===" << std::endl;
                
                // Process bids (buy orders)
                if (data.contains("b") && data["b"].is_array()) {
                    auto bids = data["b"];
                    std::cout << "Processing " << bids.size() << " bid levels..." << std::endl;
                    
                    for (const auto& bid : bids) {
                        if (bid.is_array() && bid.size() >= 2) {
                            std::string price_str = bid[0].get<std::string>();
                            std::string qty_str = bid[1].get<std::string>();
                            
                            double price = std::stod(price_str);
                            double quantity = std::stod(qty_str);
                            
                            if (quantity > 0.0 && price > 0.0) {
                                // Cancel existing orders at this price level
                                if (buy_orders_.find(price_str) != buy_orders_.end()) {
                                    for (int order_id : buy_orders_[price_str]) {
                                        order_book_.cancel_order(order_id);
                                    }
                                    buy_orders_[price_str].clear();
                                }
                                
                                // Add new order
                                auto trades = order_book_.add_order(next_order_id_, price, 
                                                                  static_cast<int>(quantity), true);
                                
                                // Track the order
                                buy_orders_[price_str].push_back(next_order_id_);
                                
                                if (!trades.empty()) {
                                    std::cout << "  Generated " << trades.size() 
                                              << " trade(s) from bid @ $" << price << std::endl;
                                }
                                
                                next_order_id_++;
                            }
                        }
                    }
                }
                
                // Process asks (sell orders)
                if (data.contains("a") && data["a"].is_array()) {
                    auto asks = data["a"];
                    std::cout << "Processing " << asks.size() << " ask levels..." << std::endl;
                    
                    for (const auto& ask : asks) {
                        if (ask.is_array() && ask.size() >= 2) {
                            std::string price_str = ask[0].get<std::string>();
                            std::string qty_str = ask[1].get<std::string>();
                            
                            double price = std::stod(price_str);
                            double quantity = std::stod(qty_str);
                            
                            if (quantity > 0.0 && price > 0.0) {
                                // Cancel existing orders at this price level
                                if (sell_orders_.find(price_str) != sell_orders_.end()) {
                                    for (int order_id : sell_orders_[price_str]) {
                                        order_book_.cancel_order(order_id);
                                    }
                                    sell_orders_[price_str].clear();
                                }
                                
                                // Add new order
                                auto trades = order_book_.add_order(next_order_id_, price, 
                                                                  static_cast<int>(quantity), false);
                                
                                // Track the order
                                sell_orders_[price_str].push_back(next_order_id_);
                                
                                if (!trades.empty()) {
                                    std::cout << "  Generated " << trades.size() 
                                              << " trade(s) from ask @ $" << price << std::endl;
                                }
                                
                                next_order_id_++;
                            }
                        }
                    }
                }
                
                // Display current order book state
                std::cout << "\nLocal Order Book State:" << std::endl;
                double best_bid = order_book_.get_best_bid();
                double best_ask = order_book_.get_best_ask();
                
                if (best_bid > -std::numeric_limits<double>::infinity()) {
                    int bid_qty = order_book_.get_bid_quantity_at(best_bid);
                    std::cout << "  Best Bid: $" << best_bid << " (Qty: " << bid_qty << ")";
                } else {
                    std::cout << "  Best Bid: None";
                }
                
                if (best_ask < std::numeric_limits<double>::infinity()) {
                    int ask_qty = order_book_.get_ask_quantity_at(best_ask);
                    std::cout << " | Best Ask: $" << best_ask << " (Qty: " << ask_qty << ")" << std::endl;
                } else {
                    std::cout << " | Best Ask: None" << std::endl;
                }
                
                if (best_bid > -std::numeric_limits<double>::infinity() && 
                    best_ask < std::numeric_limits<double>::infinity()) {
                    std::cout << "  Spread: $" << (best_ask - best_bid) << std::endl;
                } else {
                    std::cout << "  Spread: N/A" << std::endl;
                }
                
                std::cout << std::endl;
                
                // Clear the buffer
                buffer_.consume(buffer_.size());
                
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                break;
            }
        }
    }
    
    void close() {
        // Close the WebSocket connection
        ws_.close(websocket::close_code::normal);
    }
};

int main(int argc, char** argv) {
    try {
        // The io_context is required for all I/O
        net::io_context ioc;
        
        // Create and run the WebSocket client
        BinanceWebSocketClient client(ioc);
        
        // Run the client in a separate thread so we can handle shutdown
        std::thread client_thread([&client]() {
            try {
                client.run();
            } catch (const std::exception& e) {
                std::cerr << "Client error: " << e.what() << std::endl;
            }
        });
        
        // Wait for the client thread to finish
        client_thread.join();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}