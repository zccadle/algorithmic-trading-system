"""Enhanced data loader with support for multiple formats."""

import pandas as pd
import numpy as np
from datetime import datetime, timedelta
from pathlib import Path
from typing import Optional, Union


class DataLoader:
    """Load market data from various sources."""
    
    def __init__(self, data_dir: Optional[Path] = None):
        """
        Initialize data loader.
        
        Args:
            data_dir: Directory containing market data files
        """
        if data_dir is None:
            # Default to data/market_data relative to project root
            # data_loader.py is in src/python/trading_framework/core/
            # Go up to src/python/, then up 2 more to trading_system/
            self.data_dir = Path(__file__).parent.parent.parent.parent.parent / "data" / "market_data"
        else:
            self.data_dir = Path(data_dir)
    
    def load_parquet(self, symbol: str) -> pd.DataFrame:
        """
        Load data from Parquet file.
        
        Args:
            symbol: Ticker symbol
            
        Returns:
            DataFrame with OHLCV data
        """
        # Look for most recent file for this symbol
        pattern = f"{symbol.lower()}_*.parquet"
        files = list(self.data_dir.glob(pattern))
        
        if not files:
            raise FileNotFoundError(f"No Parquet files found for {symbol} in {self.data_dir}")
        
        # Use most recent file
        latest_file = max(files, key=lambda f: f.stat().st_mtime)
        
        print(f"Loading data from {latest_file}")
        df = pd.read_parquet(latest_file)
        
        # Ensure proper column names
        if 'close' not in df.columns and 'Close' in df.columns:
            df.columns = [col.lower() for col in df.columns]
        
        return df
    
    def load_csv(self, filepath: Union[str, Path]) -> pd.DataFrame:
        """
        Load data from CSV file.
        
        Args:
            filepath: Path to CSV file
            
        Returns:
            DataFrame with OHLCV data
        """
        df = pd.read_csv(filepath, parse_dates=True, index_col=0)
        
        # Standardize column names
        column_mapping = {
            'Date': 'date',
            'Open': 'open',
            'High': 'high',
            'Low': 'low',
            'Close': 'close',
            'Volume': 'volume'
        }
        df.rename(columns=column_mapping, inplace=True)
        
        return df
    
    def load_sample_data(self, days: int = 252) -> pd.DataFrame:
        """
        Generate sample market data for testing.
        
        Args:
            days: Number of days of data to generate
            
        Returns:
            DataFrame with OHLCV data
        """
        # Generate dates
        end_date = datetime.now()
        start_date = end_date - timedelta(days=days)
        dates = pd.date_range(start=start_date, end=end_date, freq='D')
        
        # Generate synthetic price data with trend and noise
        np.random.seed(42)
        
        # Base price with upward trend
        base_price = 50000
        trend = np.linspace(0, 5000, len(dates))
        
        # Add random walk
        returns = np.random.normal(0, 0.02, len(dates))
        price_series = base_price + trend
        
        for i in range(1, len(price_series)):
            price_series[i] = price_series[i-1] * (1 + returns[i])
        
        # Generate OHLCV data
        data = {
            'open': price_series * (1 + np.random.uniform(-0.001, 0.001, len(dates))),
            'high': price_series * (1 + np.random.uniform(0, 0.01, len(dates))),
            'low': price_series * (1 - np.random.uniform(0, 0.01, len(dates))),
            'close': price_series,
            'volume': np.random.uniform(1000, 5000, len(dates))
        }
        
        df = pd.DataFrame(data, index=dates)
        
        # Ensure high >= close >= low
        df['high'] = df[['open', 'high', 'close']].max(axis=1)
        df['low'] = df[['open', 'low', 'close']].min(axis=1)
        
        return df
    
    def load(self, symbol_or_path: Optional[str] = None, **kwargs) -> pd.DataFrame:
        """
        Smart loader that detects data source.
        
        Args:
            symbol_or_path: Symbol name, file path, or None for sample data
            **kwargs: Additional arguments for specific loaders
            
        Returns:
            DataFrame with OHLCV data
        """
        if symbol_or_path is None:
            # Generate sample data
            return self.load_sample_data(**kwargs)
        
        if '/' in str(symbol_or_path) or '\\' in str(symbol_or_path):
            # It's a file path
            filepath = Path(symbol_or_path)
            if filepath.suffix == '.parquet':
                return pd.read_parquet(filepath)
            elif filepath.suffix == '.csv':
                return self.load_csv(filepath)
            else:
                raise ValueError(f"Unsupported file type: {filepath.suffix}")
        else:
            # It's a symbol - try to load from Parquet
            try:
                return self.load_parquet(symbol_or_path)
            except FileNotFoundError:
                print(f"No data found for {symbol_or_path}, generating sample data")
                return self.load_sample_data(**kwargs)