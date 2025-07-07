from abc import ABC, abstractmethod
from typing import Dict, List, Optional, Any
import simulator_core as core

class BaseStrategy(ABC):
    """Base class for all trading strategies"""
    
    def __init__(self, participant_id: str, symbols: List[str]):
        self.participant_id = participant_id
        self.symbols = symbols
        self.portfolio = None
        self.order_books = {}
        self.market_data_engine = None
        self.active_orders = {}
        self.trade_history = []
        self.market_data_history = {}
        
    def initialize(self, portfolio, order_books, market_data_engine):
        """Initialize strategy with simulation components"""
        
        self.portfolio = portfolio
        self.order_books = order_books
        self.market_data_engine = market_data_engine
        
        # initialize market data history for each symbol
        for symbol in self.symbols:
            self.market_data_history[symbol] = []
    
    @abstractmethod
    def on_market_data(self, market_data: core.MarketData):
        """Called when new market data arrives"""
        pass
    
    @abstractmethod
    def on_trade(self, trade: core.Trade):
        """Called when a trade occurs"""
        pass
    
    @abstractmethod
    def on_order_rejection(self, order: core.Order, reason: str):
        """Called when an order is rejected"""
        pass
    
    def submit_order(self, symbol: str, side: core.OrderSide, quantity: int, order_type: core.OrderType = core.OrderType.MARKET, price: float = 0.0) -> Optional[core.Order]:
        """Submit an order to the market"""

        if symbol not in self.order_books:
            print(f"No order book for symbol: {symbol}")
            return None
            
        order = core.Order(
            participant_id=self.participant_id,
            symbol=symbol,
            side=side,
            quantity=quantity,
            type=order_type,
            price=price
        )
        
        success = self.order_books[symbol].add_order(order)
        if success:
            self.active_orders[order.id] = order
            return order
        
        return None
    
    def get_position(self, symbol: str) -> int:
        """Get current position for a symbol"""

        if self.portfolio:
            return self.portfolio.get_position(self.participant_id, symbol)
        
        return 0
    
    def get_cash(self) -> float:
        """Get current cash balance"""

        if self.portfolio:
            return self.portfolio.get_cash(self.participant_id)
        
        return 0.0
    
    def get_portfolio_value(self) -> float:
        """Get total portfolio value"""

        if self.portfolio and self.market_data_engine:
            prices = self.market_data_engine.get_all_prices()

            return self.portfolio.get_portfolio_value(self.participant_id, prices)
        
        return 0.0
    
    def get_pnl(self) -> float:
        """Get profit and loss"""

        if self.portfolio and self.market_data_engine:
            prices = self.market_data_engine.get_all_prices()

            return self.portfolio.get_pnl(self.participant_id, prices)
        
        return 0.0
    
    def get_market_data_history(self, symbol: str, lookback: int = 100) -> List[core.MarketData]:
        """Get recent market data for a symbol"""

        if symbol in self.market_data_history:
            return self.market_data_history[symbol][-lookback:]
        
        return []
    
    def update_market_data_history(self, market_data: core.MarketData):
        """Update market data history"""

        symbol = market_data.symbol

        if symbol in self.market_data_history:
            self.market_data_history[symbol].append(market_data)
            
            # keep only last 1000 data points
            if len(self.market_data_history[symbol]) > 1000:
                self.market_data_history[symbol] = self.market_data_history[symbol][-1000:]