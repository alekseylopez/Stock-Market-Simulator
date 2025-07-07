#include "../types/common_types.hpp"
#include "../core/market_data_engine.hpp"
#include "../core/order_book.hpp"
#include "../core/portfolio.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/operators.h>

namespace py = pybind11;
using namespace simulator;

PYBIND11_MODULE(simulator_core, m)
{
    m.doc() = "Stock Market Simulator Core Engine";
    
    // basic types and enums
    py::enum_<OrderType>(m, "OrderType")
        .value("MARKET", OrderType::MARKET)
        .value("LIMIT", OrderType::LIMIT);
    
    py::enum_<OrderSide>(m, "OrderSide")
        .value("BUY", OrderSide::BUY)
        .value("SELL", OrderSide::SELL);
    
    // MarketData struct
    py::class_<MarketData>(m, "MarketData")
        .def(py::init<const Symbol&, Price, Quantity, Timestamp>())
        .def_readwrite("symbol", &MarketData::symbol)
        .def_readwrite("price", &MarketData::price)
        .def_readwrite("volume", &MarketData::volume)
        .def_readwrite("timestamp", &MarketData::timestamp)
        .def_readwrite("bid", &MarketData::bid)
        .def_readwrite("ask", &MarketData::ask)
        .def("__repr__", [](const MarketData& md)
        {
            return "MarketData(symbol='" + md.symbol + "', price=" +
                    std::to_string(md.price) + ", volume=" + std::to_string(md.volume) + ")";
        });
    
    // Order struct
    py::class_<Order>(m, "Order")
        .def(py::init<const ParticipantId&, const Symbol&, OrderSide, Quantity, OrderType, Price>(),
             py::arg("participant_id"), py::arg("symbol"), py::arg("side"), 
             py::arg("quantity"), py::arg("type") = OrderType::MARKET, py::arg("price") = 0.0)
        .def_readwrite("id", &Order::id)
        .def_readwrite("participant_id", &Order::participant_id)
        .def_readwrite("symbol", &Order::symbol)
        .def_readwrite("type", &Order::type)
        .def_readwrite("side", &Order::side)
        .def_readwrite("quantity", &Order::quantity)
        .def_readwrite("price", &Order::price)
        .def_readwrite("timestamp", &Order::timestamp)
        .def("__repr__", [](const Order& o)
        {
            return "Order(id='" + o.id + "', participant='" + o.participant_id + 
                    "', symbol='" + o.symbol + "', side=" + 
                    (o.side == OrderSide::BUY ? "BUY" : "SELL") + 
                    ", quantity=" + std::to_string(o.quantity) + 
                    ", price=" + std::to_string(o.price) + ")";
        });
    
    // Trade struct
    py::class_<Trade>(m, "Trade")
        .def(py::init<const OrderId&, const OrderId&, const Symbol&, Quantity, Price, Timestamp>())
        .def_readwrite("buy_order_id", &Trade::buy_order_id)
        .def_readwrite("sell_order_id", &Trade::sell_order_id)
        .def_readwrite("symbol", &Trade::symbol)
        .def_readwrite("quantity", &Trade::quantity)
        .def_readwrite("price", &Trade::price)
        .def_readwrite("timestamp", &Trade::timestamp)
        .def("notional_value", &Trade::notional_value)
        .def("__repr__", [](const Trade& t)
        {
            return "Trade(symbol='" + t.symbol + "', quantity=" +
                    std::to_string(t.quantity) + ", price=" + std::to_string(t.price) +
                    ", notional=" + std::to_string(t.notional_value()) + ")";
        });
    
    // Portfolio class
    py::class_<Portfolio, std::shared_ptr<Portfolio>>(m, "Portfolio")
        .def(py::init<const std::unordered_map<ParticipantId, double>&>())
        .def("add_participant", &Portfolio::add_participant)
        .def("can_buy", &Portfolio::can_buy)
        .def("can_sell", &Portfolio::can_sell)
        .def("execute_trade", &Portfolio::execute_trade)
        .def("get_pnl", &Portfolio::get_pnl)
        .def("get_portfolio_value", &Portfolio::get_portfolio_value)
        .def("get_cash", &Portfolio::get_cash)
        .def("get_position", &Portfolio::get_position)
        .def("get_buying_power", &Portfolio::get_buying_power)
        .def("get_total_exposure", &Portfolio::get_total_exposure);
    
    // OrderBook::BookDepth struct
    py::class_<OrderBook::BookDepth>(m, "BookDepth")
        .def(py::init<>())
        .def_readwrite("bids", &OrderBook::BookDepth::bids)
        .def_readwrite("asks", &OrderBook::BookDepth::asks)
        .def("__repr__", [](const OrderBook::BookDepth& bd)
        {
            return "BookDepth(bids=" + std::to_string(bd.bids.size()) +
                    " levels, asks=" + std::to_string(bd.asks.size()) + " levels)";
        });
    
    // OrderBook class
    py::class_<OrderBook>(m, "OrderBook")
        .def(py::init<const Symbol&>())
        .def("set_trade_callback", &OrderBook::set_trade_callback)
        .def("set_rejection_callback", &OrderBook::set_rejection_callback)
        .def("set_portfolio", &OrderBook::set_portfolio)
        .def("add_order", &OrderBook::add_order)
        .def("cancel_order", &OrderBook::cancel_order)
        .def("get_bid_price", &OrderBook::get_bid_price)
        .def("get_ask_price", &OrderBook::get_ask_price)
        .def("get_mid_price", &OrderBook::get_mid_price)
        .def("update_market_price", &OrderBook::update_market_price)
        .def("get_book_depth", &OrderBook::get_book_depth, py::arg("levels") = 5);
    
    // MarketDataEngine class
    py::class_<MarketDataEngine>(m, "MarketDataEngine")
        .def(py::init<>())
        .def("add_symbol", &MarketDataEngine::add_symbol)
        .def("set_callback", &MarketDataEngine::set_callback)
        .def("start", &MarketDataEngine::start)
        .def("stop", &MarketDataEngine::stop)
        .def("get_current_price", &MarketDataEngine::get_current_price)
        .def("get_all_prices", &MarketDataEngine::get_all_prices);
    
    // Utility functions
    m.def("generate_order_id", &OrderIdGenerator::generate, "Generate a unique order ID");
    
    // Version info
    m.attr("__version__") = "0.1.0";
}