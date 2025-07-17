"""Mean Reversion strategy based on RSI (Relative Strength Index)."""

from typing import Optional

import numpy as np
import pandas as pd

from trading_framework.core.strategy import Strategy


class MeanReversionRSIStrategy(Strategy):
    """
    Mean reversion strategy using RSI indicator.
    
    Generates buy signals when RSI is oversold (< lower_threshold)
    and sell signals when RSI is overbought (> upper_threshold).
    """
    
    def __init__(
        self,
        rsi_period: int = 14,
        lower_threshold: float = 35.0,
        upper_threshold: float = 65.0,
        lookback_days: Optional[int] = None
    ) -> None:
        """
        Initialize RSI mean reversion strategy.
        
        Args:
            rsi_period: Period for RSI calculation (default 14)
            lower_threshold: Buy when RSI below this level (default 35)
            upper_threshold: Sell when RSI above this level (default 65)
            lookback_days: Optional limit on historical data to use
        """
        super().__init__("Mean_Reversion_RSI")
        
        if not 0 < lower_threshold < upper_threshold < 100:
            raise ValueError("Thresholds must satisfy: 0 < lower < upper < 100")
        
        self.rsi_period = rsi_period
        self.lower_threshold = lower_threshold
        self.upper_threshold = upper_threshold
        self.lookback_days = lookback_days
    
    def calculate_rsi(self, prices: pd.Series) -> pd.Series:
        """
        Calculate RSI (Relative Strength Index).
        
        Args:
            prices: Series of prices (typically close prices)
            
        Returns:
            Series of RSI values
        """
        # Calculate price changes
        delta = prices.diff()
        
        # Separate gains and losses
        gains = delta.where(delta > 0, 0)
        losses = -delta.where(delta < 0, 0)
        
        # Calculate average gains and losses
        avg_gains = gains.rolling(window=self.rsi_period, min_periods=self.rsi_period).mean()
        avg_losses = losses.rolling(window=self.rsi_period, min_periods=self.rsi_period).mean()
        
        # Calculate RS (Relative Strength)
        rs = avg_gains / avg_losses
        
        # Calculate RSI
        rsi = 100 - (100 / (1 + rs))
        
        # Handle division by zero (when avg_losses = 0)
        rsi = rsi.fillna(100)  # If no losses, RSI = 100 (extremely overbought)
        
        return rsi
    
    def calculate_signals(self, data: pd.DataFrame) -> pd.DataFrame:
        """
        Calculate trading signals based on RSI levels.
        
        Args:
            data: DataFrame with OHLCV data
            
        Returns:
            DataFrame with position signals
        """
        # Ensure we have required data
        if 'close' not in data.columns:
            raise ValueError("Data must contain 'close' column")
        
        # Limit data if specified
        if self.lookback_days is not None:
            data = data.tail(self.lookback_days)
        
        # Initialize signals DataFrame
        signals = pd.DataFrame(index=data.index)
        signals['price'] = data['close']
        
        # Calculate RSI
        signals['rsi'] = self.calculate_rsi(data['close'])
        
        # Initialize position to 0
        signals['position'] = 0
        
        # Generate entry/exit signals
        # Buy signal: RSI crosses below lower threshold (oversold)
        signals.loc[signals['rsi'] < self.lower_threshold, 'signal'] = 1
        
        # Sell signal: RSI crosses above upper threshold (overbought)
        signals.loc[signals['rsi'] > self.upper_threshold, 'signal'] = -1
        
        # Fill NaN signals with 0
        signals['signal'] = signals['signal'].fillna(0)
        
        # Convert signals to positions
        # This is a simple implementation - in practice you might want more sophisticated logic
        position = 0
        positions = []
        
        for idx, row in signals.iterrows():
            if row['signal'] == 1 and position == 0:
                # Enter long position
                position = 1
            elif row['signal'] == -1 and position == 1:
                # Exit long position
                position = 0
            
            positions.append(position)
        
        signals['position'] = positions
        
        # Add signal strength (how far RSI is from neutral 50)
        signals['signal_strength'] = abs(signals['rsi'] - 50) / 50
        
        return signals
    
    def get_required_columns(self) -> list[str]:
        """Return list of required data columns."""
        return ['close']
    
    def __str__(self) -> str:
        """String representation of the strategy."""
        return (
            f"{self.name}(period={self.rsi_period}, "
            f"buy<{self.lower_threshold}, sell>{self.upper_threshold})"
        )