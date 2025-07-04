mod order_book;

use order_book::{OrderBook, Trade};

fn print_trades(trades: &Vec<Trade>) {
    if trades.is_empty() {
        println!("No trades generated.");
    } else {
        println!("Trades generated:");
        for trade in trades {
            println!(
                "  Trade #{}: {} @ ${:.2} (Buy Order: {}, Sell Order: {})",
                trade.trade_id,
                trade.quantity,
                trade.price,
                trade.buy_order_id,
                trade.sell_order_id
            );
        }
    }
}

fn main() {
    println!("=== Order Book & Matching Engine Test ===");

    let mut book = OrderBook::new();

    // Build initial order book
    println!("\n--- Building Initial Order Book ---");

    // Add buy orders (no matches expected)
    let trades = book.add_order(1, 100.50, 10, true);
    print_trades(&trades);
    let trades = book.add_order(2, 100.75, 5, true);
    print_trades(&trades);
    let trades = book.add_order(3, 100.25, 15, true);
    print_trades(&trades);

    // Add sell orders (no matches expected)
    let trades = book.add_order(4, 101.00, 10, false);
    print_trades(&trades);
    let trades = book.add_order(5, 101.25, 15, false);
    print_trades(&trades);

    if let Some(best_bid) = book.get_best_bid() {
        println!(
            "\nInitial Best Bid: ${:.2} (Quantity: {})",
            best_bid,
            book.get_bid_quantity_at(best_bid)
        );
    }

    if let Some(best_ask) = book.get_best_ask() {
        println!(
            "Initial Best Ask: ${:.2} (Quantity: {})",
            best_ask,
            book.get_ask_quantity_at(best_ask)
        );
    }

    // Test market-crossing orders
    println!("\n--- Testing Market-Crossing Orders ---");

    // Add aggressive buy order that crosses the spread
    println!("\nAdding Buy Order #6: 25 @ $101.10 (crosses spread)...");
    let trades = book.add_order(6, 101.10, 25, true);
    print_trades(&trades);

    if let Some(best_bid) = book.get_best_bid() {
        println!(
            "\nBest Bid after crossing: ${:.2} (Quantity: {})",
            best_bid,
            book.get_bid_quantity_at(best_bid)
        );
    }

    if let Some(best_ask) = book.get_best_ask() {
        println!(
            "Best Ask after crossing: ${:.2} (Quantity: {})",
            best_ask,
            book.get_ask_quantity_at(best_ask)
        );
    }

    // Add aggressive sell order that crosses the spread
    println!("\nAdding Sell Order #7: 30 @ $100.00 (crosses spread)...");
    let trades = book.add_order(7, 100.00, 30, false);
    print_trades(&trades);

    if let Some(best_bid) = book.get_best_bid() {
        println!(
            "\nFinal Best Bid: ${:.2} (Quantity: {})",
            best_bid,
            book.get_bid_quantity_at(best_bid)
        );
    }

    if let Some(best_ask) = book.get_best_ask() {
        println!(
            "Final Best Ask: ${:.2} (Quantity: {})",
            best_ask,
            book.get_ask_quantity_at(best_ask)
        );
    }
}
