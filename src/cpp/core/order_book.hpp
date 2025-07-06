#ifndef SMS_ORDER_BOOK_HPP
#define SMS_ORDER_BOOK_HPP

#include "../types/common_types.hpp"
#include "portfolio.hpp"

#include <functional>
#include <map>
#include <queue>
#include <memory>

namespace simulator
{

class OrderBook
{
public:
    using TradeCallback = std::function<void(const Trade&)>;
    using OrderRejectionCallback = std::function<void(const Order&, const std::string&)>;

    OrderBook(const Symbol& symbol);

    void set_trade_callback(TradeCallback callback);
    void set_rejection_callback(OrderRejectionCallback callback);
    void set_portfolio(std::shared_ptr<Portfolio> portfolio);

    bool add_order(const Order& order);

    Price get_bid_price() const;
    Price get_ask_price() const;
    Price get_mid_price() const;

    void update_market_price(Price price);

private:
    Symbol symbol_;
    std::map<Price, std::queue<Order>> buy_orders_;
    std::map<Price, std::queue<Order>> sell_orders_;

    // portfolio
    std::shared_ptr<Portfolio> portfolio_;
    Price current_market_price_;

    // callbacks
    TradeCallback trade_callback_;
    OrderRejectionCallback rejection_callback_;

    // order processing
    bool execute_market_order(const Order& order);
    bool execute_buy_market_order(const Order& order);
    bool execute_sell_market_order(const Order& order);
    void add_limit_order(const Order& order);

    // trade execution with portfolio updates
    void match_orders();
    void execute_trade(const Order& buyer_order, const Order& seller_order, Quantity quantity, Price price);

    // portfolio validation
    bool validate_order(const Order& order) const;
    bool validate_buy_order(const Order& order) const;
    bool validate_sell_order(const Order& order) const;
    Price estimate_execution_price(const Order& order) const;
};

}

#endif