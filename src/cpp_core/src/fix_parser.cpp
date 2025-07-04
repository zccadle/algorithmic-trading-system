#include "fix_parser.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

// FIXMessage implementation
std::optional<std::string> FIXMessage::get_field(int tag) const {
    auto it = fields.find(tag);
    if (it != fields.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<double> FIXMessage::get_price() const {
    auto price_str = get_field(FIXTags::Price);
    if (price_str.has_value()) {
        try {
            return std::stod(price_str.value());
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<int> FIXMessage::get_quantity() const {
    auto qty_str = get_field(FIXTags::OrderQty);
    if (qty_str.has_value()) {
        try {
            return std::stoi(qty_str.value());
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

bool FIXMessage::is_buy_side() const {
    auto side = get_field(FIXTags::Side);
    return side.has_value() && side.value() == std::string(1, FIXSide::Buy);
}

// FIXParser implementation
FIXMessage FIXParser::parse(const std::string& raw_message) {
    FIXMessage result;
    result.msg_type = FIXMsgType::Unknown;
    
    // Split message by SOH delimiter
    std::string field;
    std::istringstream stream(raw_message);
    
    while (std::getline(stream, field, SOH)) {
        if (field.empty()) continue;
        
        // Find the '=' separator
        size_t equals_pos = field.find('=');
        if (equals_pos == std::string::npos) continue;
        
        // Extract tag and value
        try {
            int tag = std::stoi(field.substr(0, equals_pos));
            std::string value = field.substr(equals_pos + 1);
            result.fields[tag] = value;
        } catch (...) {
            // Skip malformed fields
            continue;
        }
    }
    
    // Determine message type
    result.msg_type = get_message_type(result.fields);
    
    return result;
}

FIXMsgType FIXParser::get_message_type(const std::unordered_map<int, std::string>& fields) {
    auto it = fields.find(FIXTags::MsgType);
    if (it != fields.end()) {
        if (it->second == "D") return FIXMsgType::NewOrderSingle;
        if (it->second == "F") return FIXMsgType::OrderCancelRequest;
    }
    return FIXMsgType::Unknown;
}

std::string FIXParser::calculate_checksum(const std::string& message) {
    int sum = 0;
    for (char c : message) {
        sum += static_cast<unsigned char>(c);
    }
    
    // FIX checksum is the sum modulo 256, formatted as 3-digit string
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(3) << (sum % 256);
    return oss.str();
}

bool FIXParser::validate_checksum(const std::string& message, const std::string& checksum) {
    return calculate_checksum(message) == checksum;
}

std::string FIXParser::create_new_order_single(
    const std::string& cl_ord_id,
    const std::string& symbol,
    char side,
    int quantity,
    double price,
    char ord_type) {
    
    std::ostringstream msg;
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::gmtime(&time_t);
    
    std::ostringstream timestamp;
    timestamp << std::put_time(tm, "%Y%m%d-%H:%M:%S");
    
    // Build message body (without header and trailer)
    std::ostringstream body;
    body << FIXTags::MsgType << "=D" << SOH;
    body << FIXTags::SenderCompID << "=CLIENT" << SOH;
    body << FIXTags::TargetCompID << "=EXCHANGE" << SOH;
    body << FIXTags::SendingTime << "=" << timestamp.str() << SOH;
    body << FIXTags::ClOrdID << "=" << cl_ord_id << SOH;
    body << FIXTags::Symbol << "=" << symbol << SOH;
    body << FIXTags::Side << "=" << side << SOH;
    body << FIXTags::OrderQty << "=" << quantity << SOH;
    body << FIXTags::OrdType << "=" << ord_type << SOH;
    if (ord_type == FIXOrdType::Limit) {
        body << FIXTags::Price << "=" << std::fixed << std::setprecision(2) << price << SOH;
    }
    body << FIXTags::TimeInForce << "=0" << SOH;  // Day order
    body << FIXTags::TransactTime << "=" << timestamp.str() << SOH;
    
    std::string body_str = body.str();
    
    // Build complete message
    msg << FIXTags::BeginString << "=FIX.4.4" << SOH;
    msg << FIXTags::BodyLength << "=" << body_str.length() << SOH;
    msg << body_str;
    
    // Calculate and append checksum
    std::string msg_for_checksum = msg.str();
    std::string checksum = calculate_checksum(msg_for_checksum);
    msg << FIXTags::CheckSum << "=" << checksum << SOH;
    
    return msg.str();
}

std::string FIXParser::create_order_cancel_request(
    const std::string& cl_ord_id,
    const std::string& orig_cl_ord_id,
    const std::string& symbol,
    char side,
    int quantity) {
    
    std::ostringstream msg;
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::gmtime(&time_t);
    
    std::ostringstream timestamp;
    timestamp << std::put_time(tm, "%Y%m%d-%H:%M:%S");
    
    // Build message body
    std::ostringstream body;
    body << FIXTags::MsgType << "=F" << SOH;
    body << FIXTags::SenderCompID << "=CLIENT" << SOH;
    body << FIXTags::TargetCompID << "=EXCHANGE" << SOH;
    body << FIXTags::SendingTime << "=" << timestamp.str() << SOH;
    body << FIXTags::ClOrdID << "=" << cl_ord_id << SOH;
    body << FIXTags::OrigClOrdID << "=" << orig_cl_ord_id << SOH;
    body << FIXTags::Symbol << "=" << symbol << SOH;
    body << FIXTags::Side << "=" << side << SOH;
    body << FIXTags::OrderQty << "=" << quantity << SOH;
    body << FIXTags::TransactTime << "=" << timestamp.str() << SOH;
    
    std::string body_str = body.str();
    
    // Build complete message
    msg << FIXTags::BeginString << "=FIX.4.4" << SOH;
    msg << FIXTags::BodyLength << "=" << body_str.length() << SOH;
    msg << body_str;
    
    // Calculate and append checksum
    std::string msg_for_checksum = msg.str();
    std::string checksum = calculate_checksum(msg_for_checksum);
    msg << FIXTags::CheckSum << "=" << checksum << SOH;
    
    return msg.str();
}