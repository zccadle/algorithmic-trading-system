use csv::Reader;
use rust_core::order_book::{OrderBook, Trade};
use std::env;
use std::error::Error;
use std::fs::File;
use std::time::Instant;

#[derive(Debug, serde::Deserialize)]
struct MarketOrder {
    is_buy: u8,
    price: f64,
    quantity: u32,
}

fn print_trades(trades: &Vec<Trade>) {
    for trade in trades {
        println!(
            "  Trade #{}: {} @ ${:.2} (Buy Order: {}, Sell Order: {})",
            trade.trade_id, trade.quantity, trade.price, trade.buy_order_id, trade.sell_order_id
        );
    }
}

fn main() -> Result<(), Box<dyn Error>> {
    println!("=== Order Book Replay Tool ===");

    // Determine the CSV file path
    let args: Vec<String> = env::args().collect();
    let csv_path = if args.len() > 1 {
        args[1].clone()
    } else {
        "../../market_data.csv".to_string()
    };

    // Read market data from CSV
    println!("\nReading market data from: {csv_path}");
    let file = File::open(&csv_path)?;
    let mut reader = Reader::from_reader(file);

    let mut orders: Vec<MarketOrder> = Vec::new();
    for result in reader.deserialize() {
        let order: MarketOrder = result?;
        orders.push(order);
    }

    println!("Loaded {} orders from file.", orders.len());

    // Create order book and replay orders
    let mut book = OrderBook::new();
    let mut total_trades = 0;
    let mut order_id = 1;

    println!("\n--- Replaying Market Data ---");

    let start_time = Instant::now();

    for order in &orders {
        let is_buy = order.is_buy == 1;
        println!(
            "\nOrder #{}: {} {} @ ${:.2}",
            order_id,
            if is_buy { "BUY" } else { "SELL" },
            order.quantity,
            order.price
        );

        let trades = book.add_order(order_id, order.price, order.quantity, is_buy);
        order_id += 1;

        if !trades.is_empty() {
            println!("Generated {} trade(s):", trades.len());
            print_trades(&trades);
            total_trades += trades.len();
        } else {
            println!("Order added to book (no trades).");
        }

        // Print current book state
        print!("Book State - Best Bid: ");
        if let Some(best_bid) = book.get_best_bid() {
            print!(
                "${:.2} (Qty: {})",
                best_bid,
                book.get_bid_quantity_at(best_bid)
            );
        } else {
            print!("None");
        }

        print!(", Best Ask: ");
        if let Some(best_ask) = book.get_best_ask() {
            println!(
                "${:.2} (Qty: {})",
                best_ask,
                book.get_ask_quantity_at(best_ask)
            );
        } else {
            println!("None");
        }
    }

    let duration = start_time.elapsed();

    // Print summary
    println!("\n=== Replay Summary ===");
    println!("Total orders processed: {}", orders.len());
    println!("Total trades generated: {total_trades}");
    println!("Processing time: {} microseconds", duration.as_micros());
    println!(
        "Average time per order: {:.2} microseconds",
        duration.as_micros() as f64 / orders.len() as f64
    );

    Ok(())
}
