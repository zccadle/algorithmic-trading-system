"""Simple moving average crossover strategy implementation."""

from typing import List

import numpy as np
import pandas as pd

from trading_framework.core.strategy import Strategy


class SimpleMACrossStrategy(Strategy):
    """
    Simple moving average crossover strategy.
    
    Generates buy signals when short MA crosses above long MA,
    and sell signals when short MA crosses below long MA.
    """
    
    def __init__(self, short_window: int = 20, long_window: int = 50) -> None:
        """
        Initialize strategy with moving average windows.
        
        Args:
            short_window: Period for short-term moving average
            long_window: Period for long-term moving average
        """
        super().__init__("MA_Cross")
        
        if short_window >= long_window:
            raise ValueError("Short window must be less than long window")
        
        self.short_window = short_window
        self.long_window = long_window
        self._parameters = {
            'short_window': short_window,
            'long_window': long_window
        }
    
    def calculate_signals(self, market_data: pd.DataFrame) -> pd.DataFrame:
        """
        Generate trading signals based on moving average crossover.
        
        Args:
            market_data: DataFrame with OHLCV data
            
        Returns:
            DataFrame with signals and position changes
        """
        self.validate_data(market_data)
        
        df = market_data.copy()
        
        # Calculate moving averages
        df['MA_short'] = df['close'].rolling(
            window=self.short_window, 
            min_periods=1
        ).mean()
        
        df['MA_long'] = df['close'].rolling(
            window=self.long_window,
            min_periods=1
        ).mean()
        
        # Initialize signal column
        df['signal'] = 0
        
        # Generate signals: 1 when short MA > long MA, -1 otherwise
        # Only generate signals after we have enough data for long MA
        valid_idx = df.index[self.long_window-1:]
        
        df.loc[valid_idx, 'signal'] = np.where(
            df.loc[valid_idx, 'MA_short'] > df.loc[valid_idx, 'MA_long'],
            1,
            -1
        )
        
        # Calculate position changes (diff of signals)
        df['position'] = df['signal'].diff().fillna(0)
        
        # Create output DataFrame with required columns
        output = pd.DataFrame({
            'timestamp': df.index,
            'signal': df['signal'],
            'position': df['position'],
            'MA_short': df['MA_short'],
            'MA_long': df['MA_long'],
            'close': df['close']
        })
        
        return output
    
    def get_required_columns(self) -> List[str]:
        """Return list of required columns for this strategy."""
        return ['close']
    
    def get_signal_description(self, row: pd.Series) -> str:
        """
        Get human-readable description of signal.
        
        Args:
            row: Row from signal DataFrame
            
        Returns:
            Description of the signal
        """
        if row['position'] > 0:
            return f"BUY: MA{self.short_window} crossed above MA{self.long_window}"
        elif row['position'] < 0:
            return f"SELL: MA{self.short_window} crossed below MA{self.long_window}"
        else:
            if row['signal'] > 0:
                return f"HOLD LONG: MA{self.short_window} > MA{self.long_window}"
            elif row['signal'] < 0:
                return f"HOLD SHORT: MA{self.short_window} < MA{self.long_window}"
            else:
                return "NO POSITION"