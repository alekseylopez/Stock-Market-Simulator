#ifndef SMS_ORDER_BOOK_HPP
#define SMS_ORDER_BOOK_HPP

#include "../types/common_types.hpp"
#include "portfolio.hpp"

#include <functional>
#include <map>
#include <queue>
#include <memory>
#include <mutex>
#include <shared_mutex>

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
    bool cancel_order(const OrderId& order_id);

    Price get_bid_price() const;
    Price get_ask_price() const;
    Price get_mid_price() const;

    void update_market_price(Price price);

    struct BookDepth
    {
        std::vector<std::pair<Price, Quantity>> bids;
        std::vector<std::pair<Price, Quantity>> asks;
    };

    BookDepth get_book_depth(size_t levels = 5) const;

private:
    Symbol symbol_;
    std::map<Price, std::queue<Order>> buy_orders_;
    std::map<Price, std::queue<Order>> sell_orders_;

    std::unordered_map<OrderId, Order> active_orders_;
    std::unordered_map<OrderId, std::pair<Price, OrderSide>> order_locations_;

    // portfolio
    std::shared_ptr<Portfolio> portfolio_;
    Price current_market_price_;

    mutable std::shared_mutex book_mutex_;
    mutable std::mutex callback_mutex_;
    mutable std::mutex market_price_mutex_;

    // callbacks
    TradeCallback trade_callback_;
    OrderRejectionCallback rejection_callback_;

    mutable std::mutex callback_queue_mutex_;
    std::vector<Trade> queued_trades_;
    std::vector<std::pair<Order, std::string>> queued_rejections_;

    // callback queuing
    void queue_trade_callback(const Trade& trade);
    void queue_rejection_callback(const Order& order, const std::string& reason);
    void process_queued_callbacks();

    // order processing (assumes book_mutex_ is held)
    bool execute_market_order_unsafe(const Order& order);
    bool execute_buy_market_order_unsafe(const Order& order);
    bool execute_sell_market_order_unsafe(const Order& order);
    void add_limit_order_unsafe(const Order& order);

    // trade execution
    void match_orders_unsafe();
    void execute_trade_unsafe(const Order& buyer_order, const Order& seller_order, Quantity quantity, Price price);

    // cancelling orders
    bool remove_order_from_queue(std::queue<Order>& orders, const OrderId& order_id);

    // portfolio validation
    bool validate_order(const Order& order) const;
    bool validate_buy_order(const Order& order) const;
    bool validate_sell_order(const Order& order) const;
    Price estimate_execution_price_unsafe(const Order& order) const;
};

}

#endif