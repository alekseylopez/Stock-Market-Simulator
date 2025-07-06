#ifndef SMS_ORDER_BOOK_HPP
#define SMS_ORDER_BOOK_HPP

#include "../types/common_types.hpp"

#include <functional>
#include <map>
#include <queue>

namespace simulator
{

class OrderBook
{
public:
    using TradeCallback = std::function<void(const Trade&)>;

    OrderBook(const Symbol& symbol);

    void set_trade_callback(TradeCallback callback);

    void add_order(const Order& order);

    Price get_bid_price() const;
    Price get_ask_price() const;
    Price get_mid_price() const;

private:
    Symbol symbol_;
    std::map<Price, std::queue<Order>> buy_orders_;
    std::map<Price, std::queue<Order>> sell_orders_;
    TradeCallback trade_callback_;

    void execute_market_order(const Order& order);

    void execute_buy_market_order(const Order& order);
    void execute_sell_market_order(const Order& order);

    void add_limit_order(const Order& order);

    void match_orders();
};

}

#endif