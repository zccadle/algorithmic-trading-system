use futures_util::{SinkExt, StreamExt};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use tokio::net::TcpStream;
use tokio_tungstenite::{connect_async, tungstenite::protocol::Message, MaybeTlsStream, WebSocketStream};

#[derive(Debug, Deserialize)]
struct DepthUpdate {
    #[serde(rename = "e")]
    event_type: String,
    #[serde(rename = "E")]
    event_time: u64,
    #[serde(rename = "s")]
    symbol: String,
    #[serde(rename = "b")]
    bids: Vec<Vec<String>>,
    #[serde(rename = "a")]
    asks: Vec<Vec<String>>,
}

type WSStream = WebSocketStream<MaybeTlsStream<TcpStream>>;

async fn handle_binance_stream() -> Result<(), Box<dyn std::error::Error>> {
    // Binance WebSocket endpoint for BTC/USDT depth updates
    let url = "wss://stream.binance.com:9443/ws/btcusdt@depth";
    
    println!("Connecting to Binance WebSocket stream: {}", url);
    
    // Connect to the WebSocket
    let (ws_stream, _) = connect_async(url).await?;
    println!("Connected to Binance WebSocket stream: /ws/btcusdt@depth");
    println!("Listening for BTC/USDT depth updates...\n");
    
    let (mut write, mut read) = ws_stream.split();
    
    // Process incoming messages
    while let Some(message) = read.next().await {
        match message {
            Ok(Message::Text(text)) => {
                // Parse the JSON message
                match serde_json::from_str::<DepthUpdate>(&text) {
                    Ok(depth) => {
                        // Extract and print best bid
                        if let Some(best_bid) = depth.bids.first() {
                            if best_bid.len() >= 2 {
                                print!("Best Bid: ${} (Qty: {})", &best_bid[0], &best_bid[1]);
                            }
                        }
                        
                        // Extract and print best ask
                        if let Some(best_ask) = depth.asks.first() {
                            if best_ask.len() >= 2 {
                                print!(" | Best Ask: ${} (Qty: {})", &best_ask[0], &best_ask[1]);
                            }
                        }
                        
                        println!();
                    }
                    Err(e) => {
                        eprintln!("Failed to parse depth update: {}", e);
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
                eprintln!("WebSocket error: {}", e);
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
        Err(e) => eprintln!("WebSocket client error: {}", e),
    }
}