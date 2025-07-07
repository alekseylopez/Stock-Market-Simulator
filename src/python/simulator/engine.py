import threading
import time
from typing import Dict, List, Optional, Callable
import simulator_core as core
from .strategies.base import BaseStrategy
from .strategies.market_maker import MarketMakerStrategy

class SimulationEngine:
    """Main simulation engine that orchestrates all components"""
    
    def __init__(self):
        self.portfolio = None
        self.market_data_engine = None
        self.order_books = {}
        self.strategies = []
        self.symbols = []
        self.running = False
        self.trade_log = []
        self.callbacks = {
            'on_trade': [],
            'on_market_data': [],
            'on_order_rejection': []
        }
        self.market_makers = 0
        
    def add_symbol(self, symbol: str, initial_price: float):
        """Add a symbol to the simulation"""

        self.symbols.append(symbol)
        
        # add to market data engine
        if not self.market_data_engine:
            self.market_data_engine = core.MarketDataEngine()
        self.market_data_engine.add_symbol(symbol, initial_price)
        
        # create order book
        order_book = core.OrderBook(symbol)
        order_book.set_trade_callback(self._on_trade)
        order_book.set_rejection_callback(self._on_order_rejection)
        self.order_books[symbol] = order_book
        
    def add_participants(self, participants: Dict[str, float]):
        """Add participants with initial cash"""
        if not self.portfolio:
            self.portfolio = core.Portfolio(participants)
        else:
            for participant_id, cash in participants.items():
                self.portfolio.add_participant(participant_id, cash)
                
        # Set portfolio for all order books
        for order_book in self.order_books.values():
            order_book.set_portfolio(self.portfolio)
    
    def add_market_maker(self, initial_cash: float, initial_positions: dict):
        """Add a market maker to provide liquidity"""

        self.market_makers += 1
        
        id = "__market_maker_" + str(self.market_makers)

        self.add_participants({id: initial_cash})

        self.set_initial_positions(id, initial_positions)

        market_maker = MarketMakerStrategy(
            participant_id=id,
            symbols=self.symbols,
            spread_bps=30,
            quote_size=50,
            max_position=500,
            inventory_skew=0.3
        )

        self.add_strategy(market_maker)
    
    def set_initial_positions(self, participant_id: str, positions: dict):
        """
        Set initial positions for a participant
        positions: dict of {symbol: quantity}
        """

        if not self.portfolio:
            raise ValueError("Portfolio not initialized")
        
        for symbol, quantity in positions.items():
            current_price = self.market_data_engine.get_current_price(symbol) if self.market_data_engine else 0.0
            self.portfolio.set_initial_position(participant_id, symbol, quantity, current_price)
    
    def add_strategy(self, strategy: BaseStrategy):
        """Add a trading strategy"""

        self.strategies.append(strategy)
        
    def add_callback(self, event_type: str, callback: Callable):
        """Add event callback"""

        if event_type in self.callbacks:
            self.callbacks[event_type].append(callback)
    
    def start(self):
        """Start the simulation"""

        if not self.portfolio or not self.market_data_engine or not self.order_books:
            raise ValueError("Simulation not properly configured")
            
        self.running = True
        
        # initialize strategies
        for strategy in self.strategies:
            strategy.initialize(self.portfolio, self.order_books, self.market_data_engine)
        
        # set market data callback
        self.market_data_engine.set_callback(self._on_market_data)
        
        # start market data engine
        self.market_data_engine.start()
        
        print(f"Simulation started with {len(self.symbols)} symbols and {len(self.strategies)} strategies")
    
    def stop(self):
        """Stop the simulation"""

        self.running = False

        if self.market_data_engine:
            self.market_data_engine.stop()

        print("Simulation stopped")
    
    def run_for_duration(self, duration_seconds: float):
        """Run simulation for a specific duration"""

        self.start()

        time.sleep(duration_seconds)

        self.stop()
    
    def _on_market_data(self, market_data: core.MarketData):
        """Internal market data handler"""

        if not self.running:
            return
            
        # update order book market prices
        if market_data.symbol in self.order_books:
            self.order_books[market_data.symbol].update_market_price(market_data.price)
        
        # notify strategies
        for strategy in self.strategies:
            try:
                strategy.update_market_data_history(market_data)
                strategy.on_market_data(market_data)
            except Exception as e:
                print(f"Error in strategy {strategy.__class__.__name__}: {e}")
        
        # call external callbacks
        for callback in self.callbacks['on_market_data']:
            try:
                callback(market_data)
            except Exception as e:
                print(f"Error in market data callback: {e}")
    
    def _on_trade(self, trade: core.Trade):
        """Internal trade handler"""

        self.trade_log.append(trade)
        
        # notify strategies
        for strategy in self.strategies:
            try:
                strategy.on_trade(trade)
            except Exception as e:
                print(f"Error in strategy {strategy.__class__.__name__}: {e}")
        
        # call external callbacks
        for callback in self.callbacks['on_trade']:
            try:
                callback(trade)
            except Exception as e:
                print(f"Error in trade callback: {e}")
    
    def _on_order_rejection(self, order: core.Order, reason: str):
        """Internal order rejection handler"""

        # notify strategies
        for strategy in self.strategies:
            try:
                if strategy.participant_id == order.participant_id:
                    strategy.on_order_rejection(order, reason)
            except Exception as e:
                print(f"Error in strategy {strategy.__class__.__name__}: {e}")
        
        # call external callbacks
        for callback in self.callbacks['on_order_rejection']:
            try:
                callback(order, reason)
            except Exception as e:
                print(f"Error in order rejection callback: {e}")
    
    def get_portfolio_summary(self) -> Dict:
        """Get summary of all portfolios"""

        if not self.portfolio or not self.market_data_engine:
            return {}
        
        prices = self.market_data_engine.get_all_prices()
        summary = {}
        
        for strategy in self.strategies:
            participant_id = strategy.participant_id
            summary[participant_id] = {
                'cash': self.portfolio.get_cash(participant_id),
                'portfolio_value': self.portfolio.get_portfolio_value(participant_id, prices),
                'pnl': self.portfolio.get_pnl(participant_id, prices),
                'positions': {symbol: self.portfolio.get_position(participant_id, symbol) for symbol in self.symbols}
            }
        
        return summary
    
    def get_market_summary(self) -> Dict:
        """Get summary of market state"""

        if not self.market_data_engine:
            return {}
        
        prices = self.market_data_engine.get_all_prices()
        summary = {}
        
        for symbol in self.symbols:
            order_book = self.order_books[symbol]
            summary[symbol] = {
                'current_price': prices.get(symbol, 0.0),
                'bid': order_book.get_bid_price(),
                'ask': order_book.get_ask_price(),
                'mid': order_book.get_mid_price(),
                'spread': order_book.get_ask_price() - order_book.get_bid_price(),
                'book_depth': order_book.get_book_depth(5)
            }
        
        return summary
    
    def get_trade_history(self) -> List[core.Trade]:
        """Get all trades executed"""

        return self.trade_log.copy()