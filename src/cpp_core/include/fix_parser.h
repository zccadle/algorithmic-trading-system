#ifndef FIX_PARSER_H
#define FIX_PARSER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

enum class FIXMsgType {
    NewOrderSingle,      // 35=D
    OrderCancelRequest,  // 35=F
    Unknown
};

// FIX 4.4 Tag definitions
namespace FIXTags {
    constexpr int BeginString = 8;
    constexpr int BodyLength = 9;
    constexpr int MsgType = 35;
    constexpr int SenderCompID = 49;
    constexpr int TargetCompID = 56;
    constexpr int SendingTime = 52;
    constexpr int ClOrdID = 11;
    constexpr int OrigClOrdID = 41;
    constexpr int Symbol = 55;
    constexpr int Side = 54;
    constexpr int OrderQty = 38;
    constexpr int Price = 44;
    constexpr int OrdType = 40;
    constexpr int TimeInForce = 59;
    constexpr int TransactTime = 60;
    constexpr int CheckSum = 10;
}

// Side values
namespace FIXSide {
    constexpr char Buy = '1';
    constexpr char Sell = '2';
}

// Order Type values
namespace FIXOrdType {
    constexpr char Market = '1';
    constexpr char Limit = '2';
    constexpr char Stop = '3';
}

struct FIXMessage {
    FIXMsgType msg_type;
    std::unordered_map<int, std::string> fields;
    
    // Helper methods to extract common fields
    std::optional<std::string> get_field(int tag) const;
    std::optional<double> get_price() const;
    std::optional<int> get_quantity() const;
    bool is_buy_side() const;
};

class FIXParser {
private:
    static constexpr char SOH = '\x01';  // FIX delimiter
    
    // Calculate FIX checksum
    static std::string calculate_checksum(const std::string& message);
    
    // Validate checksum
    static bool validate_checksum(const std::string& message, const std::string& checksum);
    
    // Extract message type from parsed fields
    static FIXMsgType get_message_type(const std::unordered_map<int, std::string>& fields);
    
public:
    // Parse a raw FIX message
    static FIXMessage parse(const std::string& raw_message);
    
    // Create a NewOrderSingle message (for testing)
    static std::string create_new_order_single(
        const std::string& cl_ord_id,
        const std::string& symbol,
        char side,
        int quantity,
        double price,
        char ord_type = FIXOrdType::Limit
    );
    
    // Create an OrderCancelRequest message (for testing)
    static std::string create_order_cancel_request(
        const std::string& cl_ord_id,
        const std::string& orig_cl_ord_id,
        const std::string& symbol,
        char side,
        int quantity
    );
};

#endif // FIX_PARSER_H