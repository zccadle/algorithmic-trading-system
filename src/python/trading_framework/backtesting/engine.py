"""Signal-based backtesting engine that properly uses C++/Rust execution engines."""

import io
import subprocess
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Union

import pandas as pd

from trading_framework.core.strategy import Strategy


class SignalBacktestEngine:
    """
    Orchestrates backtesting by sending signals to C++/Rust engines for execution.
    
    This implementation follows the correct architecture:
    1. Python generates signals
    2. C++/Rust simulates execution and outputs trades
    3. Python analyzes the resulting trades
    """
    
    def __init__(
        self, 
        engine_path: str, 
        engine_type: str = "cpp",
        verbose: bool = True
    ) -> None:
        """
        Initialize signal-based backtest engine.
        
        Args:
            engine_path: Path to compiled signal_backtest_engine executable
            engine_type: Type of engine ('cpp' or 'rust')
            verbose: Whether to print progress messages
        """
        self.engine_path = Path(engine_path)
        if not self.engine_path.exists():
            raise FileNotFoundError(f"Engine executable not found: {engine_path}")
        
        self.engine_type = engine_type
        self.verbose = verbose
        self._validate_engine()
    
    def _validate_engine(self) -> None:
        """Validate that the engine is executable."""
        if not self.engine_path.is_file():
            raise ValueError(f"Engine path is not a file: {self.engine_path}")
        
        # Check if executable (Unix-like systems)
        if hasattr(self.engine_path, 'stat'):
            import stat
            if not self.engine_path.stat().st_mode & stat.S_IXUSR:
                raise ValueError(f"Engine is not executable: {self.engine_path}")
    
    def run(
        self,
        market_data: pd.DataFrame,
        strategy: Strategy,
        initial_capital: float = 100000.0,
        position_size: float = 0.1
    ) -> Dict[str, pd.DataFrame]:
        """
        Run backtest using the signal-based execution engine.
        
        This method:
        1. Generates signals using the strategy
        2. Sends market data + signals to C++ engine
        3. Receives executed trades from the engine
        4. Returns trades for analysis
        
        Args:
            market_data: Historical market data
            strategy: Strategy instance to test
            initial_capital: Starting capital
            position_size: Fraction of capital to use per trade
            
        Returns:
            Dictionary containing:
                - trades: DataFrame of executed trades from engine
                - signals: DataFrame of generated signals
                - states: Portfolio states from engine
        """
        if self.verbose:
            print(f"Generating signals using {strategy.name} strategy...")
        
        # Generate signals
        signals = strategy.calculate_signals(market_data)
        
        if self.verbose:
            print(f"Preparing data for {self.engine_type} signal engine...")
        
        # Prepare input data with signals
        engine_input = self._prepare_engine_input_with_signals(market_data, signals)
        
        if self.verbose:
            print(f"Running signal backtest engine: {self.engine_path}")
        
        # Run engine subprocess
        trades_output, states_output = self._run_engine_subprocess(
            engine_input, initial_capital, position_size
        )
        
        if self.verbose:
            print("Parsing engine output...")
        
        # Parse outputs
        trades = self._parse_signal_trades(trades_output)
        states = self._parse_portfolio_states(states_output)
        
        # The key difference: we don't calculate portfolio metrics here
        # That's done by the PerformanceAnalyzer using the actual trades
        
        return {
            'trades': trades,
            'signals': signals,
            'states': states
        }
    
    def _prepare_engine_input_with_signals(
        self,
        market_data: pd.DataFrame,
        signals: pd.DataFrame
    ) -> str:
        """
        Prepare CSV input for the signal-based engine.
        
        The engine expects CSV with columns:
        timestamp,symbol,bid,ask,bid_size,ask_size,last_price,volume,signal_position
        """
        # Start with market data
        df = market_data.copy()
        
        # Ensure we have required columns
        if 'timestamp' not in df.columns:
            df['timestamp'] = df.index
        
        # Create synthetic bid/ask from close price
        if 'bid' not in df.columns:
            spread = 0.0001  # 1 basis point spread
            df['bid'] = df['close'] * (1 - spread)
            df['ask'] = df['close'] * (1 + spread)
        
        # Add default sizes
        if 'bid_size' not in df.columns:
            df['bid_size'] = 100.0
            df['ask_size'] = 100.0
        
        # Add symbol
        if 'symbol' not in df.columns:
            df['symbol'] = 'BTC-USD'
        
        # Add last_price if not present
        if 'last_price' not in df.columns:
            df['last_price'] = df['close']
        
        # Merge with signals to get position
        df = df.merge(
            signals[['position']], 
            left_index=True, 
            right_index=True, 
            how='left'
        )
        df['position'] = df['position'].fillna(0)
        
        # Rename position to signal_position for clarity
        df['signal_position'] = df['position']
        
        # Select and order columns for engine
        engine_columns = [
            'timestamp', 'symbol', 'bid', 'ask', 
            'bid_size', 'ask_size', 'last_price', 'volume', 'signal_position'
        ]
        
        # Convert timestamp to Unix timestamp if needed
        if pd.api.types.is_datetime64_any_dtype(df['timestamp']):
            df['timestamp'] = df['timestamp'].astype('int64') // 10**9
        
        return df[engine_columns].to_csv(index=False)
    
    def _run_engine_subprocess(
        self, 
        input_data: str,
        initial_capital: float,
        position_size: float
    ) -> Tuple[str, str]:
        """
        Run the signal backtest engine as a subprocess.
        
        Args:
            input_data: CSV data with market data and signals
            initial_capital: Initial capital for the engine
            position_size: Position size fraction
            
        Returns:
            Tuple of (trades_output, states_output)
        """
        # Prepare command with parameters
        cmd = [
            str(self.engine_path),
            "--capital", str(initial_capital),
            "--size", str(position_size)
        ]
        
        # Run subprocess
        try:
            process = subprocess.Popen(
                cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            stdout, stderr = process.communicate(input=input_data)
            
            if process.returncode != 0:
                raise RuntimeError(
                    f"Signal backtest engine failed with code {process.returncode}:\n{stderr}"
                )
            
            # Trades go to stdout, states to stderr
            return stdout, stderr
            
        except Exception as e:
            raise RuntimeError(f"Failed to run signal backtest engine: {e}")
    
    def _parse_signal_trades(self, output: str) -> pd.DataFrame:
        """Parse trade output from signal-based engine."""
        trade_lines = []
        
        for line in output.strip().split('\n'):
            if line.startswith('TRADE,'):
                trade_lines.append(line[6:])  # Remove 'TRADE,' prefix
        
        if not trade_lines:
            # Return empty DataFrame with expected columns
            return pd.DataFrame(columns=[
                'timestamp', 'symbol', 'trade_id', 'side', 'price', 
                'quantity', 'fee', 'slippage', 'signal_type'
            ])
        
        # Parse CSV
        trades_csv = '\n'.join(trade_lines)
        trades = pd.read_csv(
            io.StringIO(trades_csv),
            names=['timestamp', 'symbol', 'trade_id', 'side', 'price',
                   'quantity', 'fee', 'slippage', 'signal_type']
        )
        
        # Convert timestamp to datetime (assuming Unix timestamp in seconds)
        trades['timestamp'] = pd.to_datetime(trades['timestamp'], unit='s')
        
        return trades
    
    def _parse_portfolio_states(self, output: str) -> pd.DataFrame:
        """Parse portfolio state output from engine."""
        state_lines = []
        
        for line in output.strip().split('\n'):
            if line.startswith('STATE,'):
                state_lines.append(line[6:])  # Remove 'STATE,' prefix
        
        if not state_lines:
            return pd.DataFrame()
        
        # Parse CSV
        states_csv = '\n'.join(state_lines)
        states = pd.read_csv(
            io.StringIO(states_csv),
            names=['timestamp', 'cash', 'position', 'holdings_value', 
                   'total_value', 'last_price']
        )
        
        # Convert timestamp to datetime (assuming Unix timestamp in seconds)
        states['timestamp'] = pd.to_datetime(states['timestamp'], unit='s')
        
        return states