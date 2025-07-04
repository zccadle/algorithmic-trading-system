#include <iostream>
#include <iomanip>
#include "fix_parser.h"
#include "order_book.h"

void print_parsed_message(const FIXMessage& msg) {
    std::cout << "Message Type: ";
    switch (msg.msg_type) {
        case FIXMsgType::NewOrderSingle:
            std::cout << "NewOrderSingle (35=D)";
            break;
        case FIXMsgType::OrderCancelRequest:
            std::cout << "OrderCancelRequest (35=F)";
            break;
        default:
            std::cout << "Unknown";
    }
    std::cout << std::endl;
    
    std::cout << "\nParsed Fields:" << std::endl;
    std::cout << std::setw(15) << "Tag" << std::setw(25) << "Value" << std::setw(30) << "Description" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    
    // Print fields in a logical order
    std::vector<std::pair<int, std::string>> tag_descriptions = {
        {FIXTags::BeginString, "BeginString"},
        {FIXTags::BodyLength, "BodyLength"},
        {FIXTags::MsgType, "MsgType"},
        {FIXTags::SenderCompID, "SenderCompID"},
        {FIXTags::TargetCompID, "TargetCompID"},
        {FIXTags::SendingTime, "SendingTime"},
        {FIXTags::ClOrdID, "ClOrdID"},
        {FIXTags::OrigClOrdID, "OrigClOrdID"},
        {FIXTags::Symbol, "Symbol"},
        {FIXTags::Side, "Side"},
        {FIXTags::OrderQty, "OrderQty"},
        {FIXTags::Price, "Price"},
        {FIXTags::OrdType, "OrdType"},
        {FIXTags::TimeInForce, "TimeInForce"},
        {FIXTags::TransactTime, "TransactTime"},
        {FIXTags::CheckSum, "CheckSum"}
    };
    
    for (const auto& [tag, description] : tag_descriptions) {
        auto value = msg.get_field(tag);
        if (value.has_value()) {
            std::cout << std::setw(15) << tag 
                      << std::setw(25) << value.value() 
                      << std::setw(30) << description << std::endl;
        }
    }
}

int main() {
    std::cout << "=== FIX Parser Test ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    
    // Test 1: Create and parse a NewOrderSingle message
    std::cout << "\n1. Testing NewOrderSingle Message" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    std::string new_order = FIXParser::create_new_order_single(
        "ORD123456",    // ClOrdID
        "BTCUSD",       // Symbol
        FIXSide::Buy,   // Side
        100,            // Quantity
        45000.50,       // Price
        FIXOrdType::Limit
    );
    
    std::cout << "Raw FIX Message (with \\x01 shown as |):" << std::endl;
    for (char c : new_order) {
        if (c == '\x01') std::cout << '|';
        else std::cout << c;
    }
    std::cout << "\n" << std::endl;
    
    FIXMessage parsed_order = FIXParser::parse(new_order);
    print_parsed_message(parsed_order);
    
    // Test 2: Create and parse an OrderCancelRequest message
    std::cout << "\n\n2. Testing OrderCancelRequest Message" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    std::string cancel_request = FIXParser::create_order_cancel_request(
        "CANCEL789",    // ClOrdID
        "ORD123456",    // OrigClOrdID
        "BTCUSD",       // Symbol
        FIXSide::Buy,   // Side
        100             // Quantity
    );
    
    std::cout << "Raw FIX Message (with \\x01 shown as |):" << std::endl;
    for (char c : cancel_request) {
        if (c == '\x01') std::cout << '|';
        else std::cout << c;
    }
    std::cout << "\n" << std::endl;
    
    FIXMessage parsed_cancel = FIXParser::parse(cancel_request);
    print_parsed_message(parsed_cancel);
    
    // Test 3: Integration with OrderBook
    std::cout << "\n\n3. Integration with OrderBook" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    OrderBook book;
    
    // Use parsed NewOrderSingle to add order to book
    if (parsed_order.msg_type == FIXMsgType::NewOrderSingle) {
        auto symbol = parsed_order.get_field(FIXTags::Symbol);
        auto price = parsed_order.get_price();
        auto quantity = parsed_order.get_quantity();
        bool is_buy = parsed_order.is_buy_side();
        
        if (symbol.has_value() && price.has_value() && quantity.has_value()) {
            std::cout << "Adding order to book: " 
                      << symbol.value() << " "
                      << (is_buy ? "BUY" : "SELL") << " "
                      << quantity.value() << " @ $" << price.value() << std::endl;
            
            // Add to order book
            auto trades = book.add_order(1, price.value(), quantity.value(), is_buy);
            
            if (trades.empty()) {
                std::cout << "Order added to book successfully (no trades generated)" << std::endl;
            } else {
                std::cout << "Generated " << trades.size() << " trade(s)" << std::endl;
            }
            
            // Display book state
            double best_bid = book.get_best_bid();
            double best_ask = book.get_best_ask();
            
            std::cout << "\nOrder Book State:" << std::endl;
            if (best_bid > -std::numeric_limits<double>::infinity()) {
                std::cout << "Best Bid: $" << best_bid 
                          << " (Qty: " << book.get_bid_quantity_at(best_bid) << ")" << std::endl;
            }
            if (best_ask < std::numeric_limits<double>::infinity()) {
                std::cout << "Best Ask: $" << best_ask 
                          << " (Qty: " << book.get_ask_quantity_at(best_ask) << ")" << std::endl;
            }
        }
    }
    
    // Test 4: Parse a manually constructed FIX message
    std::cout << "\n\n4. Parsing Manually Constructed FIX Message" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    std::string manual_fix = "8=FIX.4.4\x01"
                            "9=150\x01"
                            "35=D\x01"
                            "49=CLIENT\x01"
                            "56=EXCHANGE\x01"
                            "52=20240101-12:00:00\x01"
                            "11=MANUAL001\x01"
                            "55=AAPL\x01"
                            "54=2\x01"  // Sell
                            "38=50\x01"
                            "40=2\x01"  // Limit
                            "44=175.25\x01"
                            "59=0\x01"
                            "60=20240101-12:00:00\x01"
                            "10=123\x01";
    
    FIXMessage manual_parsed = FIXParser::parse(manual_fix);
    print_parsed_message(manual_parsed);
    
    return 0;
}