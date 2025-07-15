"""
Test script for the momentum strategy
"""

import time
import sys
import os

# add the project root to the Python path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from simulator import SimulationEngine
from simulator.strategies.momentum import MomentumStrategy

def print_portfolio_summary(engine):
    """Print portfolio summary"""

    print("\n" + "=" * 60)
    print("PORTFOLIO SUMMARY")
    print("=" * 60)
    
    summary = engine.get_portfolio_summary()
    for participant_id, data in summary.items():
        print(f"\nParticipant: {participant_id}")
        print(f"  Cash: ${data['cash']:.2f}")
        print(f"  Portfolio Value: ${data['portfolio_value']:.2f}")
        print(f"  P&L: ${data['pnl']:.2f}")
        print(f"  Positions: {data['positions']}")

def print_market_summary(engine):
    """Print market summary"""

    print("\n" + "=" * 60)
    print("MARKET SUMMARY")
    print("=" * 60)
    
    summary = engine.get_market_summary()
    for symbol, data in summary.items():
        print(f"\n{symbol}:")
        print(f"  Current Price: ${data['current_price']:.2f}")
        print(f"  Bid: ${data['bid']:.2f}")
        print(f"  Ask: ${data['ask']:.2f}")
        print(f"  Mid: ${data['mid']:.2f}")
        print(f"  Spread: ${data['spread']:.2f}")

def print_trade_history(engine):
    """Print trade history"""

    print("\n" + "=" * 60)
    print("TRADE HISTORY")
    print("=" * 60)
    
    trades = engine.get_trade_history()
    if not trades:
        print("No trades executed")
        return
    
    for trade in trades:
        print(f"  {trade.timestamp}: {trade.buyer_id} bought {trade.quantity} {trade.symbol} "
              f"from {trade.seller_id} at ${trade.price:.2f}")

def print_performance_metrics(engine, participants):
    """Calculate performance metrics"""

    print("\n" + "=" * 60)
    print("PERFORMANCE METRICS")
    print("=" * 60)
    
    summary = engine.get_portfolio_summary()
    for participant_id, data in summary.items():
        if not participant_id[0:14] == "__market_maker":
            initial_value = participants[participant_id]
            final_value = data['portfolio_value']
            returns = (final_value - initial_value) / initial_value * 100
            print(f"\n{participant_id}:")
            print(f"  Initial Value: ${initial_value:.2f}")
            print(f"  Final Value: ${final_value:.2f}")
            print(f"  Returns: {returns:.2f}%")
            print(f"  Total Trades: {len([t for t in engine.get_trade_history() if t.buy_order_id == participant_id or t.sell_order_id == participant_id])}")

def print_market_maker_statistics(engine):
    """Print market maker statistics"""

    print("\n" + "="*60)
    print("MARKET MAKER STATISTICS")
    print("=" * 60)
    
    # get market maker strategy
    market_maker_strategies = {}
    for strategy in engine.strategies:
        if strategy.participant_id[0:14] == "__market_maker":
            market_maker_strategies[strategy.participant_id] = strategy
    
    for id, market_maker_strategy in market_maker_strategies.items():
        mm_stats = market_maker_strategy.get_market_making_stats()
        print("\nStats for " + id + ":")
        for symbol, stats in mm_stats.items():
            print(f"{symbol}:")
            print(f"  Position: {stats['position']}")
            print(f"  Total Trades: {stats['total_trades']}")
            print(f"  Buy Trades: {stats['buy_trades']}")
            print(f"  Sell Trades: {stats['sell_trades']}")
            print(f"  Avg Buy Price: ${stats['avg_buy_price']:.2f}")
            print(f"  Avg Sell Price: ${stats['avg_sell_price']:.2f}")
            print(f"  Spread Captured: ${stats['spread_captured']:.2f}")

def main():
    print("Starting Momentum Strategy Simulation")
    print("=" * 60)
    
    # create simulation engine
    engine = SimulationEngine()
    
    # add symbols to trade
    symbols = ["AAPL", "GOOGL", "MSFT"]
    initial_prices = {"AAPL": 150.0, "GOOGL": 2500.0, "MSFT": 300.0}
    
    for symbol in symbols:
        engine.add_symbol(symbol, initial_prices[symbol])
        print(f"Added {symbol} with initial price ${initial_prices[symbol]:.2f}")
    
    # add participants
    participants = {
        "momentum_trader_1": 100000.0,  # $100k starting cash
        "momentum_trader_2": 100000.0,  # $100k starting cash
    }
    
    engine.add_participants(participants)
    print(f"\nAdded {len(participants)} participants")
    
    # create and add momentum strategies
    strategy1 = MomentumStrategy(
        participant_id="momentum_trader_1",
        symbols=symbols,
        lookback_period=15,
        momentum_threshold=0.00015,  # .015% threshold
        position_size=10
    )
    
    strategy2 = MomentumStrategy(
        participant_id="momentum_trader_2", 
        symbols=symbols,
        lookback_period=10,
        momentum_threshold=0.00005,  # .005% threshold
        position_size=15
    )
    
    engine.add_strategy(strategy1)
    engine.add_strategy(strategy2)
    
    print(f"Added {len(engine.strategies)} trading strategies")

    print(f"\nSetting up initial inventory for market makers...")
    engine.add_market_maker(1000000.0, {
        "AAPL": 300,
        "GOOGL": 50,
        "MSFT": 150
    })
    engine.add_market_maker(1000000.0, {
        "AAPL": 100,
        "GOOGL": 250,
        "MSFT": 50
    })
    
    # add event callbacks for monitoring
    def on_trade_callback(trade):
        print(f"\n  TRADE: {trade.quantity} {trade.symbol} at ${trade.price:.2f}")
    
    def on_market_data_callback(market_data):
        # only print every 10th update to avoid spam
        if hasattr(on_market_data_callback, 'counter'):
            on_market_data_callback.counter += 1
        else:
            on_market_data_callback.counter = 1
            
        if on_market_data_callback.counter % 10 == 0:
            print(f"  {market_data.symbol}: ${market_data.price:.2f}")
    
    engine.add_callback('on_trade', on_trade_callback)
    engine.add_callback('on_market_data', on_market_data_callback)
    
    # start simulation
    print("\nStarting simulation...")
    engine.start()
    
    # let simulation run for 10 seconds
    simulation_duration = 10.0
    print(f"Running simulation for {simulation_duration} seconds...")
    
    try:
        time.sleep(simulation_duration)
    except KeyboardInterrupt:
        print("\nSimulation interrupted by user")
    
    # stop simulation
    print("\nStopping simulation...")
    engine.stop()
    
    # print final results
    print_portfolio_summary(engine)
    print_market_summary(engine)
    print_trade_history(engine)

    # participant statistics
    print_performance_metrics(engine, participants)
    print_market_maker_statistics(engine)

if __name__ == "__main__":
    main()