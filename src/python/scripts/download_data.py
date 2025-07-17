#!/usr/bin/env python3
"""
Download historical market data using yfinance and save as Parquet files.

This script demonstrates professional data engineering practices:
- Efficient data storage using Parquet format
- Proper error handling and logging
- Support for multiple symbols and timeframes
"""

import argparse
import sys
from pathlib import Path
from datetime import datetime, timedelta
from typing import List, Optional

import pandas as pd
import yfinance as yf


def download_symbol_data(
    symbol: str,
    start_date: str,
    end_date: str,
    interval: str = "1d"
) -> Optional[pd.DataFrame]:
    """
    Download historical data for a single symbol.
    
    Args:
        symbol: Ticker symbol (e.g., 'SPY', 'AAPL')
        start_date: Start date in YYYY-MM-DD format
        end_date: End date in YYYY-MM-DD format
        interval: Data interval (1d, 1h, 5m, etc.)
        
    Returns:
        DataFrame with OHLCV data or None if download fails
    """
    try:
        print(f"Downloading {symbol} data from {start_date} to {end_date}...")
        
        # Create ticker object
        ticker = yf.Ticker(symbol)
        
        # Download data
        data = ticker.history(
            start=start_date,
            end=end_date,
            interval=interval,
            auto_adjust=True,  # Adjust for splits and dividends
            prepost=False,     # Exclude pre/post market data
            actions=True       # Include dividends and splits
        )
        
        if data.empty:
            print(f"Warning: No data received for {symbol}")
            return None
        
        # Clean column names
        data.columns = [col.lower().replace(' ', '_') for col in data.columns]
        
        # Add symbol column
        data['symbol'] = symbol
        
        # Ensure we have the expected columns
        expected_cols = ['open', 'high', 'low', 'close', 'volume']
        missing_cols = set(expected_cols) - set(data.columns)
        if missing_cols:
            print(f"Warning: Missing columns for {symbol}: {missing_cols}")
            return None
        
        print(f"Successfully downloaded {len(data)} rows for {symbol}")
        return data
        
    except Exception as e:
        print(f"Error downloading {symbol}: {e}")
        return None


def save_to_parquet(
    data: pd.DataFrame,
    output_dir: Path,
    symbol: str,
    compression: str = 'snappy'
) -> bool:
    """
    Save DataFrame to Parquet format.
    
    Args:
        data: DataFrame to save
        output_dir: Directory to save files
        symbol: Ticker symbol for filename
        compression: Parquet compression type
        
    Returns:
        True if successful, False otherwise
    """
    try:
        # Create output directory if it doesn't exist
        output_dir.mkdir(parents=True, exist_ok=True)
        
        # Generate filename with timestamp
        timestamp = datetime.now().strftime('%Y%m%d')
        filename = f"{symbol.lower()}_{timestamp}.parquet"
        filepath = output_dir / filename
        
        # Save to Parquet
        data.to_parquet(
            filepath,
            engine='pyarrow',
            compression=compression,
            index=True  # Preserve datetime index
        )
        
        # Verify file was created
        file_size = filepath.stat().st_size / (1024 * 1024)  # MB
        print(f"Saved {symbol} data to {filepath} ({file_size:.2f} MB)")
        
        return True
        
    except Exception as e:
        print(f"Error saving {symbol} data: {e}")
        return False


def main():
    """Main execution function."""
    parser = argparse.ArgumentParser(
        description='Download market data and save as Parquet files'
    )
    parser.add_argument(
        'symbols',
        nargs='+',
        help='Ticker symbols to download (e.g., SPY AAPL MSFT)'
    )
    parser.add_argument(
        '--start-date',
        default=None,
        help='Start date (YYYY-MM-DD). Default: 5 years ago'
    )
    parser.add_argument(
        '--end-date',
        default=None,
        help='End date (YYYY-MM-DD). Default: today'
    )
    parser.add_argument(
        '--interval',
        default='1d',
        choices=['1m', '5m', '15m', '30m', '1h', '1d', '1wk', '1mo'],
        help='Data interval. Default: 1d (daily)'
    )
    parser.add_argument(
        '--output-dir',
        type=Path,
        default=Path('data/market_data'),
        help='Output directory for Parquet files'
    )
    
    args = parser.parse_args()
    
    # Set default dates if not provided
    if args.end_date is None:
        end_date = datetime.now().strftime('%Y-%m-%d')
    else:
        end_date = args.end_date
    
    if args.start_date is None:
        start_date = (datetime.now() - timedelta(days=5*365)).strftime('%Y-%m-%d')
    else:
        start_date = args.start_date
    
    print(f"\n=== Market Data Download ===")
    print(f"Symbols: {', '.join(args.symbols)}")
    print(f"Period: {start_date} to {end_date}")
    print(f"Interval: {args.interval}")
    print(f"Output: {args.output_dir}\n")
    
    # Download data for each symbol
    success_count = 0
    for symbol in args.symbols:
        data = download_symbol_data(
            symbol=symbol,
            start_date=start_date,
            end_date=end_date,
            interval=args.interval
        )
        
        if data is not None:
            if save_to_parquet(data, args.output_dir, symbol):
                success_count += 1
        
        print()  # Blank line between symbols
    
    # Summary
    print(f"\n=== Download Summary ===")
    print(f"Successfully downloaded: {success_count}/{len(args.symbols)} symbols")
    
    if success_count < len(args.symbols):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())