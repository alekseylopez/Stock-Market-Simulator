"""
Test different trading strategies
"""

import time
import threading
from typing import Dict, Any, List
import json

# import the simulator
from simulator import SimulationEngine
import simulator_core as core





print(core)

engine = core.MarketDataEngine()
engine.add_symbol("AAPL", 100.0)

print(engine.get_current_price("AAPL")) # prints 100.0

#
# WIP
#