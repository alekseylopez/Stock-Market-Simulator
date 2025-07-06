#include "order_book.hpp"

#include <algorithm>

namespace simulator
{

OrderBook::OrderBook(const Symbol& symbol):
    symbol_(symbol) {}

void OrderBook::set_trade_callback(TradeCallback callback)
{
    trade_callback_ = callback;
}

void OrderBook::set_rejection_callback(OrderRejectionCallback callback)
{
    rejection_callback_ = callback;
}

void OrderBook::set_portfolio(std::shared_ptr<Portfolio> portfolio)
{
    portfolio_ = portfolio;
}

bool OrderBook::add_order(const Order& order)
{
    if(!validate_order(order))
    {
        if(rejection_callback_)
            rejection_callback_(order, "Insufficient funds or position for participant: " + order.participant_id);

        return false;
    }

    if(order.type == OrderType::MARKET)
        return execute_market_order(order);
    else
        add_limit_order(order);
    
    return true;
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

void OrderBook::update_market_price(Price price)
{
    current_market_price_ = price;
}

bool OrderBook::execute_market_order(const Order& order)
{
    if(order.side == OrderSide::BUY)
        return execute_buy_market_order(order);
    else
        return execute_sell_market_order(order);
}

bool OrderBook::execute_buy_market_order(const Order& order)
{
    if(sell_orders_.empty())
    {
        if(rejection_callback_)
            rejection_callback_(order, "No liquidity available");
        
        return false;
    }

    Quantity remaining = order.quantity;
    Order modified_order = order;

    while(remaining > 0 && !sell_orders_.empty())
    {
        auto& [price, orders] = *sell_orders_.begin();
        auto& sell_order = orders.front();

        Quantity trade_quantity = std::min(remaining, sell_order.quantity);

        // create trade
        modified_order.quantity = trade_quantity;
        execute_trade(modified_order, sell_order, trade_quantity, price);

        remaining -= trade_quantity;
        sell_order.quantity -= trade_quantity;

        if(sell_order.quantity == 0)
        {
            orders.pop();

            if(orders.empty())
                sell_orders_.erase(sell_orders_.begin());
        }
    }

    return true;
}

bool OrderBook::execute_sell_market_order(const Order& order)
{
    if(buy_orders_.empty())
    {
        if(rejection_callback_)
            rejection_callback_(order, "No liquidity available");
        
        return false;
    }

    Quantity remaining = order.quantity;
    Order modified_order = order;

    while(remaining > 0 && !buy_orders_.empty())
    {
        auto& [price, orders] = *buy_orders_.rbegin();
        auto& buy_order = orders.front();

        Quantity trade_quantity = std::min(remaining, buy_order.quantity);

        // create trade
        modified_order.quantity = trade_quantity;
        execute_trade(buy_order, modified_order, trade_quantity, price);

        remaining -= trade_quantity;
        buy_order.quantity -= trade_quantity;

        if(buy_order.quantity == 0)
        {
            orders.pop();

            if(orders.empty())
                buy_orders_.erase(buy_orders_.begin());
        }
    }

    return true;
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
            execute_trade(buy_order, sell_order, trade_quantity, trade_price);

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

void OrderBook::execute_trade(const Order& buyer_order, const Order& seller_order, Quantity quantity, Price price)
{
    auto timestamp = std::chrono::duration_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch());

    Trade trade(buyer_order.id, seller_order.id, symbol_, quantity, price, timestamp);

    if(portfolio_)
    {
        portfolio_->execute_trade(buyer_order.participant_id, trade, OrderSide::BUY);
        portfolio_->execute_trade(seller_order.participant_id, trade, OrderSide::SELL);
    }

    if(trade_callback_)
        trade_callback_(trade);
}

bool OrderBook::validate_order(const Order& order) const
{
    if(!portfolio_)
        return true;
    
    if(order.side == OrderSide::BUY)
        return validate_buy_order(order);
    else
        return validate_sell_order(order);
}

bool OrderBook::validate_buy_order(const Order& order) const
{
    Price execution_price = estimate_execution_price(order);

    if(execution_price == 0.0)
        return order.type == OrderType::LIMIT;
    
    Price price_to_check = (order.type == OrderType::MARKET) ? execution_price : order.price;
    
    return portfolio_->can_buy(order.participant_id, order.symbol, order.quantity, price_to_check);
}

bool OrderBook::validate_sell_order(const Order& order) const
{
    return portfolio_->can_sell(order.participant_id, order.symbol, order.quantity);
}

Price OrderBook::estimate_execution_price(const Order& order) const
{
    if(order.side == OrderSide::BUY)
        return sell_orders_.empty() ? current_market_price_ : sell_orders_.begin()->first;
    else
        return buy_orders_.empty() ? current_market_price_ : buy_orders_.rbegin()->first;
}

}