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
    
public:
    BinanceWebSocketClient(net::io_context& ioc)
        : ioc_(ioc)
        , ssl_ctx_(ssl::context::tlsv12_client)
        , resolver_(ioc)
        , ws_(ioc, ssl_ctx_)
        , host_("stream.binance.com")
        , port_("9443")
        , target_("/ws/btcusdt@depth") {
        
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
        while (true) {
            try {
                // Read a message into our buffer
                ws_.read(buffer_);
                
                // The make_printable() function helps print a ConstBufferSequence
                std::string message = beast::buffers_to_string(buffer_.data());
                
                // Parse JSON
                json data = json::parse(message);
                
                // Extract and print best bid and ask
                if (data.contains("b") && data["b"].is_array() && !data["b"].empty()) {
                    auto best_bid = data["b"][0];
                    if (best_bid.is_array() && best_bid.size() >= 2) {
                        std::cout << "Best Bid: $" << best_bid[0].get<std::string>() 
                                  << " (Qty: " << best_bid[1].get<std::string>() << ")";
                    }
                }
                
                if (data.contains("a") && data["a"].is_array() && !data["a"].empty()) {
                    auto best_ask = data["a"][0];
                    if (best_ask.is_array() && best_ask.size() >= 2) {
                        std::cout << " | Best Ask: $" << best_ask[0].get<std::string>()
                                  << " (Qty: " << best_ask[1].get<std::string>() << ")";
                    }
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