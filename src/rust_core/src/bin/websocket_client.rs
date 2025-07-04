use futures_util::{SinkExt, StreamExt};
use rust_core::order_book::OrderBook;
use serde::Deserialize;
use std::collections::HashMap;
use tokio_tungstenite::{connect_async, tungstenite::protocol::Message};

#[derive(Debug, Deserialize)]
struct DepthUpdate {
    #[serde(rename = "e")]
    #[allow(dead_code)]
    event_type: String,
    #[serde(rename = "E")]
    #[allow(dead_code)]
    event_time: u64,
    #[serde(rename = "s")]
    #[allow(dead_code)]
    symbol: String,
    #[serde(rename = "b")]
    bids: Vec<Vec<String>>,
    #[serde(rename = "a")]
    asks: Vec<Vec<String>>,
}

async fn handle_binance_stream() -> Result<(), Box<dyn std::error::Error>> {
    // Binance WebSocket endpoint for BTC/USDT depth updates
    let url = "wss://stream.binance.com:9443/ws/btcusdt@depth";

    println!("Connecting to Binance WebSocket stream: {url}");

    // Connect to the WebSocket
    let (ws_stream, _) = connect_async(url).await?;
    println!("Connected to Binance WebSocket stream: /ws/btcusdt@depth");
    println!("Listening for BTC/USDT depth updates...\n");

    let (mut write, mut read) = ws_stream.split();

    // Create OrderBook instance
    let mut order_book = OrderBook::new();
    let mut order_id: u32 = 1;
    let mut update_count = 0;

    // Track orders at each price level for cancellation
    let mut buy_orders: HashMap<String, Vec<u32>> = HashMap::new();
    let mut sell_orders: HashMap<String, Vec<u32>> = HashMap::new();

    // Process incoming messages
    while let Some(message) = read.next().await {
        match message {
            Ok(Message::Text(text)) => {
                // Parse the JSON message
                match serde_json::from_str::<DepthUpdate>(&text) {
                    Ok(depth) => {
                        update_count += 1;
                        println!("=== Update #{update_count} ===");

                        // Process bids (buy orders)
                        println!("Processing {} bid levels...", depth.bids.len());
                        for bid in &depth.bids {
                            if bid.len() >= 2 {
                                let price = bid[0].parse::<f64>().unwrap_or(0.0);
                                let quantity = bid[1].parse::<f64>().unwrap_or(0.0);

                                if quantity > 0.0 && price > 0.0 {
                                    // Cancel existing orders at this price level
                                    let price_key = bid[0].clone();
                                    if let Some(existing_orders) = buy_orders.get(&price_key) {
                                        for &oid in existing_orders {
                                            order_book.cancel_order(oid);
                                        }
                                    }

                                    // Add new order
                                    let trades = order_book.add_order(
                                        order_id,
                                        price,
                                        quantity as u32,
                                        true,
                                    );

                                    // Track the order
                                    buy_orders.entry(price_key).or_default().clear();
                                    buy_orders.get_mut(&bid[0]).unwrap().push(order_id);

                                    if !trades.is_empty() {
                                        println!(
                                            "  Generated {} trade(s) from bid @ ${price}",
                                            trades.len()
                                        );
                                    }

                                    order_id += 1;
                                }
                            }
                        }

                        // Process asks (sell orders)
                        println!("Processing {} ask levels...", depth.asks.len());
                        for ask in &depth.asks {
                            if ask.len() >= 2 {
                                let price = ask[0].parse::<f64>().unwrap_or(0.0);
                                let quantity = ask[1].parse::<f64>().unwrap_or(0.0);

                                if quantity > 0.0 && price > 0.0 {
                                    // Cancel existing orders at this price level
                                    let price_key = ask[0].clone();
                                    if let Some(existing_orders) = sell_orders.get(&price_key) {
                                        for &oid in existing_orders {
                                            order_book.cancel_order(oid);
                                        }
                                    }

                                    // Add new order
                                    let trades = order_book.add_order(
                                        order_id,
                                        price,
                                        quantity as u32,
                                        false,
                                    );

                                    // Track the order
                                    sell_orders.entry(price_key).or_default().clear();
                                    sell_orders.get_mut(&ask[0]).unwrap().push(order_id);

                                    if !trades.is_empty() {
                                        println!(
                                            "  Generated {} trade(s) from ask @ ${price}",
                                            trades.len()
                                        );
                                    }

                                    order_id += 1;
                                }
                            }
                        }

                        // Display current order book state
                        println!("\nLocal Order Book State:");
                        if let Some(best_bid) = order_book.get_best_bid() {
                            let bid_qty = order_book.get_bid_quantity_at(best_bid);
                            print!("  Best Bid: ${best_bid:.2} (Qty: {bid_qty})");
                        } else {
                            print!("  Best Bid: None");
                        }

                        if let Some(best_ask) = order_book.get_best_ask() {
                            let ask_qty = order_book.get_ask_quantity_at(best_ask);
                            println!(" | Best Ask: ${best_ask:.2} (Qty: {ask_qty})");
                        } else {
                            println!(" | Best Ask: None");
                        }

                        if let (Some(bid), Some(ask)) =
                            (order_book.get_best_bid(), order_book.get_best_ask())
                        {
                            println!("  Spread: ${:.2}\n", ask - bid);
                        } else {
                            println!("  Spread: N/A\n");
                        }
                    }
                    Err(e) => {
                        eprintln!("Failed to parse depth update: {e}");
                    }
                }
            }
            Ok(Message::Ping(ping)) => {
                // Respond to ping with pong to keep connection alive
                write.send(Message::Pong(ping)).await?;
            }
            Ok(Message::Close(_)) => {
                println!("WebSocket connection closed");
                break;
            }
            Err(e) => {
                eprintln!("WebSocket error: {e}");
                break;
            }
            _ => {}
        }
    }

    Ok(())
}

#[tokio::main]
async fn main() {
    match handle_binance_stream().await {
        Ok(_) => println!("WebSocket client terminated successfully"),
        Err(e) => eprintln!("WebSocket client error: {e}"),
    }
}
