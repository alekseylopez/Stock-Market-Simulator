import simulator_core as core
from .base import BaseStrategy

class MomentumStrategy(BaseStrategy):
    """
    Simple momentum strategy that buys when price is trending up 
    and sells when price is trending down
    """
    
    def __init__(self, participant_id: str, symbols: list, lookback_period: int = 10, momentum_threshold: float = 0.02, position_size: int = 100):
        super().__init__(participant_id, symbols)
        self.lookback_period = lookback_period
        self.momentum_threshold = momentum_threshold  # 2% threshold
        self.position_size = position_size
        self.last_signal = {}  # track last signal to avoid repeated orders
        
        # initialize last signal for each symbol
        for symbol in symbols:
            self.last_signal[symbol] = None
    
    def on_market_data(self, market_data: core.MarketData):
        """Process new market data and make trading decisions"""

        symbol = market_data.symbol
        
        # get recent price history
        history = self.get_market_data_history(symbol, self.lookback_period)
        
        # need enough data points to calculate momentum
        if len(history) < self.lookback_period:
            return
            
        # calculate momentum
        current_price = market_data.price
        old_price = history[0].price
        momentum = (current_price - old_price) / old_price
        
        # get current position
        current_position = self.get_position(symbol)
        
        # generate trading signals
        signal = None
        if momentum > self.momentum_threshold:
            signal = "BUY"
        elif momentum < -self.momentum_threshold:
            signal = "SELL"
        
        # execute trades based on signal
        if signal and signal != self.last_signal[symbol]:
            self._execute_signal(symbol, signal, current_position)
            self.last_signal[symbol] = signal
    
    def _execute_signal(self, symbol: str, signal: str, current_position: int):
        """Execute trading signal"""
        
        if signal == "BUY" and current_position <= 0:
            # buy if we don't have a long position
            quantity = self.position_size
            if current_position < 0:
                quantity += abs(current_position)  # cover short position first
            
            order = self.submit_order(symbol, core.OrderSide.BUY, quantity, core.OrderType.MARKET)
            if order:
                print(f"[{self.participant_id}] BUY {quantity} shares of {symbol}")
                
        elif signal == "SELL" and current_position >= 0:
            # sell if we don't have a short position
            quantity = self.position_size
            if current_position > 0:
                quantity += current_position  # sell existing long position first
            
            order = self.submit_order(symbol, core.OrderSide.SELL, quantity, core.OrderType.MARKET)
            if order:
                print(f"[{self.participant_id}] SELL {quantity} shares of {symbol}")
    
    def on_trade(self, trade: core.Trade):
        """Handle trade execution"""

        if trade.buyer_id == self.participant_id or trade.seller_id == self.participant_id:
            self.trade_history.append(trade)
            participant_role = "BUYER" if trade.buyer_id == self.participant_id else "SELLER"
            print(f"[{self.participant_id}] Trade executed as {participant_role}: {trade.quantity} shares of {trade.symbol} at ${trade.price:.2f}")
    
    def on_order_rejection(self, order: core.Order, reason: str):
        """Handle order rejection"""
        
        print(f"[{self.participant_id}] Order rejected: {reason}")
        # Remove from active orders if it was tracked
        if order.id in self.active_orders:
            del self.active_orders[order.id]