import simulator_core as core
from .base import BaseStrategy

class MarketMakerStrategy(BaseStrategy):
    """
    Simple market maker strategy that provides liquidity by continuously
    quoting bid and ask prices around the current market price
    """
    
    def __init__(self, participant_id: str, symbols: list, spread_bps: int = 50, quote_size: int = 100, max_position: int = 1000, inventory_skew: float = 0.5):
        super().__init__(participant_id, symbols)
        self.spread_bps = spread_bps  # spread in basis points (50 bps = 0.5%)
        self.quote_size = quote_size  # size of each quote
        self.max_position = max_position  # maximum position per symbol
        self.inventory_skew = inventory_skew  # how much to skew quotes based on inventory
        
        # track active quotes
        self.active_quotes = {}  # {symbol: {'bid': order, 'ask': order}}
        self.last_market_price = {}  # track last known market price
        
        # initialize structures for each symbol
        for symbol in symbols:
            self.active_quotes[symbol] = {'bid': None, 'ask': None}
            self.last_market_price[symbol] = 0.0
    
    def on_market_data(self, market_data: core.MarketData):
        """Process new market data and update quotes"""

        symbol = market_data.symbol
        current_price = market_data.price
        
        # update last known price
        self.last_market_price[symbol] = current_price
        
        # get current position
        current_position = self.get_position(symbol)
        
        # check if we need to update quotes
        if self._should_update_quotes(symbol, current_price):
            self._update_quotes(symbol, current_price, current_position)
    
    def _should_update_quotes(self, symbol: str, current_price: float) -> bool:
        """Determine if quotes need to be updated"""

        # always update if no active quotes
        if not self.active_quotes[symbol]['bid'] or not self.active_quotes[symbol]['ask']:
            return True
        
        # update if price has moved significantly
        last_price = self.last_market_price.get(symbol, 0.0)
        if abs(current_price - last_price) / last_price > 0.001:  # 0.1% move
            return True
        
        return False
    
    def _update_quotes(self, symbol: str, current_price: float, current_position: int):
        """Update bid and ask quotes"""

        # cancel existing quotes first
        self._cancel_quotes(symbol)
        
        # calculate base spread
        half_spread = current_price * (self.spread_bps / 10000.0) / 2.0
        
        # apply inventory skew
        inventory_ratio = current_position / self.max_position
        skew_adjustment = inventory_ratio * self.inventory_skew * half_spread
        
        # calculate bid and ask prices
        bid_price = current_price - half_spread - skew_adjustment
        ask_price = current_price + half_spread - skew_adjustment
        
        # ensure prices are positive
        bid_price = max(bid_price, 0.01)
        ask_price = max(ask_price, bid_price + 0.01)
        
        # check position limits before quoting
        can_buy = abs(current_position + self.quote_size) <= self.max_position
        can_sell = abs(current_position - self.quote_size) <= self.max_position
        
        # submit bid order
        if can_buy and current_position < self.max_position:
            bid_order = self.submit_order(
                symbol, 
                core.OrderSide.BUY, 
                self.quote_size, 
                core.OrderType.LIMIT, 
                bid_price
            )
            if bid_order:
                self.active_quotes[symbol]['bid'] = bid_order
                print(f"[{self.participant_id}] BID: {self.quote_size} {symbol} @ ${bid_price:.2f}")
        
        # submit ask order
        if can_sell and current_position > -self.max_position:
            ask_order = self.submit_order(
                symbol, 
                core.OrderSide.SELL, 
                self.quote_size, 
                core.OrderType.LIMIT, 
                ask_price
            )
            if ask_order:
                self.active_quotes[symbol]['ask'] = ask_order
                print(f"[{self.participant_id}] ASK: {self.quote_size} {symbol} @ ${ask_price:.2f}")
    
    def _cancel_quotes(self, symbol: str):
        """Cancel existing quotes for a symbol"""

        if symbol not in self.order_books:
            return
        
        order_book = self.order_books[symbol]
        
        # cancel bid
        if self.active_quotes[symbol]['bid']:
            order_book.cancel_order(self.active_quotes[symbol]['bid'].id)
            self.active_quotes[symbol]['bid'] = None
        
        # cancel ask
        if self.active_quotes[symbol]['ask']:
            order_book.cancel_order(self.active_quotes[symbol]['ask'].id)
            self.active_quotes[symbol]['ask'] = None
    
    def on_trade(self, trade: core.Trade):
        """Handle trade execution"""

        if trade.buyer_id == self.participant_id or trade.seller_id == self.participant_id:
            self.trade_history.append(trade)
            
            # determine our role in the trade
            if trade.buyer_id == self.participant_id:
                role = "BOUGHT"
                # clear the bid quote
                if trade.symbol in self.active_quotes:
                    self.active_quotes[trade.symbol]['bid'] = None
            else:
                role = "SOLD"
                # clear the ask quote
                if trade.symbol in self.active_quotes:
                    self.active_quotes[trade.symbol]['ask'] = None
            
            print(f"[{self.participant_id}] {role}: {trade.quantity} {trade.symbol} @ ${trade.price:.2f}")
            
            # update quotes after trade
            current_price = self.last_market_price.get(trade.symbol, trade.price)
            current_position = self.get_position(trade.symbol)
            self._update_quotes(trade.symbol, current_price, current_position)
    
    def on_order_rejection(self, order: core.Order, reason: str):
        """Handle order rejection"""

        print(f"[{self.participant_id}] Order rejected: {reason}")
        
        # clean up active quotes if the rejected order was one of them
        symbol = order.symbol
        if symbol in self.active_quotes:
            if self.active_quotes[symbol]['bid'] and self.active_quotes[symbol]['bid'].id == order.id:
                self.active_quotes[symbol]['bid'] = None
            elif self.active_quotes[symbol]['ask'] and self.active_quotes[symbol]['ask'].id == order.id:
                self.active_quotes[symbol]['ask'] = None
        
        # remove from active orders
        if order.id in self.active_orders:
            del self.active_orders[order.id]
    
    def get_market_making_stats(self) -> dict:
        """Get market making performance statistics"""
        
        stats = {}
        
        for symbol in self.symbols:
            position = self.get_position(symbol)
            trades = [t for t in self.trade_history if t.symbol == symbol]
            
            buy_trades = [t for t in trades if t.buyer_id == self.participant_id]
            sell_trades = [t for t in trades if t.seller_id == self.participant_id]
            
            avg_buy_price = sum(t.price for t in buy_trades) / len(buy_trades) if buy_trades else 0
            avg_sell_price = sum(t.price for t in sell_trades) / len(sell_trades) if sell_trades else 0
            
            stats[symbol] = {
                'position': position,
                'total_trades': len(trades),
                'buy_trades': len(buy_trades),
                'sell_trades': len(sell_trades),
                'avg_buy_price': avg_buy_price,
                'avg_sell_price': avg_sell_price,
                'spread_captured': avg_sell_price - avg_buy_price if avg_buy_price > 0 and avg_sell_price > 0 else 0
            }
        
        return stats