mod order_book;

use order_book::OrderBook;

fn main() {
    println!("=== Order Book Test ===");
    
    let mut book = OrderBook::new();
    
    // Add buy orders
    book.add_order(1, 100.50, 10, true);
    book.add_order(2, 100.75, 5, true);
    book.add_order(3, 100.25, 15, true);
    book.add_order(4, 100.50, 20, true);  // Same price as order 1
    
    // Add sell orders
    book.add_order(5, 101.00, 10, false);
    book.add_order(6, 101.25, 15, false);
    book.add_order(7, 101.50, 5, false);
    book.add_order(8, 101.00, 10, false);  // Same price as order 5
    
    if let Some(best_bid) = book.get_best_bid() {
        println!("\nBest Bid: ${:.2} (Quantity: {})", 
                 best_bid, book.get_bid_quantity_at(best_bid));
    }
    
    if let Some(best_ask) = book.get_best_ask() {
        println!("Best Ask: ${:.2} (Quantity: {})", 
                 best_ask, book.get_ask_quantity_at(best_ask));
    }
    
    // Test order cancellation
    println!("\nCancelling order 2 (Buy @ $100.75)...");
    book.cancel_order(2);
    
    if let Some(best_bid) = book.get_best_bid() {
        println!("New Best Bid: ${:.2} (Quantity: {})", 
                 best_bid, book.get_bid_quantity_at(best_bid));
    }
    
    // Cancel one of the aggregated orders
    println!("\nCancelling order 1 (Buy @ $100.50)...");
    book.cancel_order(1);
    
    println!("Quantity at $100.50: {}", book.get_bid_quantity_at(100.50));
}