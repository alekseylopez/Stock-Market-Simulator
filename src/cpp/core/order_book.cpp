#include "order_book.hpp"

namespace simulator
{

OrderBook::OrderBook(const Symbol& symbol):
    symbol_(symbol) {}

void OrderBook::set_trade_callback(TradeCallback callback)
{
    trade_callback_ = callback;
}

void OrderBook::add_order(const Order& order)
{
    if(order.type == OrderType::MARKET)
        execute_market_order(order);
    else
        add_limit_order(order);
}

Price OrderBook::get_bid_price() const
{
    return buy_orders_.empty() ? 0.0 : buy_orders_.rbegin()->first;
}

Price OrderBook::get_ask_price() const
{
    return sell_orders_.empty() ? 0.0 : sell_orders_.begin()->first;
}

Price OrderBook::get_mid_price() const
{
    Price bid = get_bid_price();
    Price ask = get_ask_price();

    return (bid > 0 && ask > 0) ? (bid + ask) / 2.0 : 0.0;
}

void OrderBook::execute_market_order(const Order& order)
{
    if(order.side == OrderSide::BUY)
        execute_buy_market_order(order);
    else
        execute_sell_market_order(order);
}

void OrderBook::execute_buy_market_order(const Order& order)
{
    Quantity remaining = order.quantity;

    while(remaining > 0 && !sell_orders_.empty())
    {
        auto& [price, orders] = *sell_orders_.begin();
        auto& sell_order = orders.front();

        Quantity trade_quantity = std::min(remaining, sell_order.quantity);

        // create trade
        if(trade_callback_)
        {
            auto timestamp = std::chrono::duration_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch());

            trade_callback_(Trade(order.id, sell_order.id, symbol_, trade_quantity, price, timestamp));
        }

        remaining -= trade_quantity;
        sell_order.quantity -= trade_quantity;

        if(sell_order.quantity == 0)
        {
            orders.pop();

            if(orders.empty())
                sell_orders_.erase(sell_orders_.begin());
        }
    }
}

void OrderBook::execute_sell_market_order(const Order& order)
{
    Quantity remaining = order.quantity;

    while(remaining > 0 && !buy_orders_.empty())
    {
        auto& [price, orders] = *buy_orders_.rbegin();
        auto& buy_order = orders.front();

        Quantity trade_quantity = std::min(remaining, buy_order.quantity);

        // create trade
        if(trade_callback_)
        {
            auto timestamp = std::chrono::duration_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch());

            trade_callback_(Trade(order.id, buy_order.id, symbol_, trade_quantity, price, timestamp));
        }

        remaining -= trade_quantity;
        buy_order.quantity -= trade_quantity;

        if(buy_order.quantity == 0)
        {
            orders.pop();

            if(orders.empty())
                buy_orders_.erase(buy_orders_.begin());
        }
    }
}

void OrderBook::add_limit_order(const Order& order)
{
    if(order.side == OrderSide::BUY)
        buy_orders_[order.price].push(order);
    else
        sell_orders_[order.price].push(order);
    
    match_orders();
}

void OrderBook::match_orders()
{
    while(!buy_orders_.empty() && !sell_orders_.empty())
    {
        Price best_bid = buy_orders_.rbegin()->first;
        Price best_ask = sell_orders_.begin()->first;

        if(best_bid >= best_ask)
        {
            auto& buy_order = buy_orders_.rbegin()->second.front();
            auto& sell_order = sell_orders_.begin()->second.front();

            Quantity trade_quantity = std::min(buy_order.quantity, sell_order.quantity);
            Price trade_price = best_ask; // improvement for buyer

            // create trade
            if(trade_callback_)
            {
                auto timestamp = std::chrono::duration_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch());

                trade_callback_(Trade(buy_order.id, sell_order.id, symbol_, trade_quantity, trade_price, timestamp));
            }

            buy_order.quantity -= trade_quantity;
            sell_order.quantity -= trade_quantity;

            // remove filled orders
            if(buy_order.quantity == 0)
            {
                buy_orders_.rbegin()->second.pop();

                if(buy_orders_.rbegin()->second.empty())
                    buy_orders_.erase(std::prev(buy_orders_.end()));
            }

            if(sell_order.quantity == 0)
            {
                sell_orders_.begin()->second.pop();

                if(sell_orders_.begin()->second.empty())
                    sell_orders_.erase(sell_orders_.begin());
            }
        } else
        {
            break;
        }
    }
}

}