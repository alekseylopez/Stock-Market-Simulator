#include "order_book.hpp"

#include <algorithm>

namespace simulator
{

OrderBook::OrderBook(const Symbol& symbol):
    symbol_(symbol) {}

void OrderBook::set_trade_callback(TradeCallback callback)
{
    std::lock_guard<std::mutex> lock(callback_mutex_);

    trade_callback_ = callback;
}

void OrderBook::set_rejection_callback(OrderRejectionCallback callback)
{
    std::lock_guard<std::mutex> lock(callback_mutex_);

    rejection_callback_ = callback;
}

void OrderBook::set_portfolio(std::shared_ptr<Portfolio> portfolio)
{
    std::unique_lock<std::shared_mutex> lock(book_mutex_);

    portfolio_ = portfolio;
}

bool OrderBook::add_order(const Order& order)
{
    if(!validate_order(order))
    {
        std::lock_guard<std::mutex> callback_lock(callback_mutex_);

        if(rejection_callback_)
            rejection_callback_(order, "Insufficient funds or position for participant: " + order.participant_id);

        return false;
    }

    std::unique_lock<std::shared_mutex> lock(book_mutex_);

    if(order.type == OrderType::MARKET)
        return execute_market_order_unsafe(order);
    else
        add_limit_order_unsafe(order);
    
    return true;
}

bool OrderBook::cancel_order(const OrderId& order_id)
{
    std::unique_lock<std::shared_mutex> lock(book_mutex_);

    auto active_it = active_orders_.find(order_id);
    if(active_it == active_orders_.end())
        return false; // order not found or already filled/cancelled
    
    auto location_it = order_locations_.find(order_id);
    if(location_it == order_locations_.end())
    {
        // clean up inconsistency
        active_orders_.erase(active_it);

        return false;
    }
    
    Price price = location_it->second.first;
    OrderSide side = location_it->second.second;
    
    bool removed = false;

    if(side == OrderSide::BUY)
    {
        removed = remove_order_from_queue(buy_orders_[price], order_id);

        if(buy_orders_[price].empty())
            buy_orders_.erase(price);
    } else
    {
        removed = remove_order_from_queue(sell_orders_[price], order_id);

        if(sell_orders_[price].empty())
            sell_orders_.erase(price);
    }
    
    if(removed)
    {
        active_orders_.erase(active_it);
        order_locations_.erase(location_it);

        return true;
    }
    
    return false;
}

Price OrderBook::get_bid_price() const
{
    std::shared_lock<std::shared_mutex> lock(book_mutex_);

    return buy_orders_.empty() ? 0.0 : buy_orders_.rbegin()->first;
}

Price OrderBook::get_ask_price() const
{
    std::shared_lock<std::shared_mutex> lock(book_mutex_);

    return sell_orders_.empty() ? 0.0 : sell_orders_.begin()->first;
}

Price OrderBook::get_mid_price() const
{
    std::shared_lock<std::shared_mutex> lock(book_mutex_);

    Price bid = get_bid_price();
    Price ask = get_ask_price();

    return (bid > 0 && ask > 0) ? (bid + ask) / 2.0 : 0.0;
}

void OrderBook::update_market_price(Price price)
{
    std::lock_guard<std::mutex> lock(market_price_mutex_);

    current_market_price_ = price;
}

OrderBook::BookDepth OrderBook::get_book_depth(size_t levels) const
{
    std::shared_lock<std::shared_mutex> lock(book_mutex_);

    BookDepth depth;

    // bid side
    auto bid_it = buy_orders_.rbegin();
    for(size_t i = 0; i < levels && bid_it != buy_orders_.rend(); ++i, ++bid_it)
    {
        Quantity total_qty = 0;

        std::queue<Order> temp_queue = bid_it->second;

        while(!temp_queue.empty())
        {
            total_qty += temp_queue.front().quantity;
            temp_queue.pop();
        }

        depth.bids.emplace_back(bid_it->first, total_qty);
    }
    
    // ask side
    auto ask_it = sell_orders_.begin();
    for(size_t i = 0; i < levels && ask_it != sell_orders_.end(); ++i, ++ask_it)
    {
        Quantity total_qty = 0;

        std::queue<Order> temp_queue = ask_it->second;

        while(!temp_queue.empty())
        {
            total_qty += temp_queue.front().quantity;
            temp_queue.pop();
        }

        depth.asks.emplace_back(ask_it->first, total_qty);
    }

    return depth;
}

bool OrderBook::execute_market_order_unsafe(const Order& order)
{
    if(order.side == OrderSide::BUY)
        return execute_buy_market_order_unsafe(order);
    else
        return execute_sell_market_order_unsafe(order);
}

bool OrderBook::execute_buy_market_order_unsafe(const Order& order)
{
    if(sell_orders_.empty())
    {
        std::lock_guard<std::mutex> callback_lock(callback_mutex_);

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
        execute_trade_unsafe(modified_order, sell_order, trade_quantity, price);

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

bool OrderBook::execute_sell_market_order_unsafe(const Order& order)
{
    if(buy_orders_.empty())
    {
        std::lock_guard<std::mutex> callback_lock(callback_mutex_);

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
        execute_trade_unsafe(buy_order, modified_order, trade_quantity, price);

        remaining -= trade_quantity;
        buy_order.quantity -= trade_quantity;

        if(buy_order.quantity == 0)
        {
            orders.pop();

            if(orders.empty())
                buy_orders_.erase(std::prev(buy_orders_.end()));
        }
    }

    return true;
}

void OrderBook::add_limit_order_unsafe(const Order& order)
{
    active_orders_[order.id] = order;
    order_locations_[order.id] = { order.price, order.side };

    if(order.side == OrderSide::BUY)
        buy_orders_[order.price].push(order);
    else
        sell_orders_[order.price].push(order);
    
    match_orders_unsafe();
}

void OrderBook::match_orders_unsafe()
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
            Price trade_price = best_ask; // standard behavior

            // create trade
            execute_trade_unsafe(buy_order, sell_order, trade_quantity, trade_price);

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

void OrderBook::execute_trade_unsafe(const Order& buyer_order, const Order& seller_order, Quantity quantity, Price price)
{
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

    Trade trade(buyer_order.id, seller_order.id, symbol_, quantity, price, timestamp);

    if(portfolio_)
    {
        portfolio_->execute_trade(buyer_order.participant_id, trade, OrderSide::BUY);
        portfolio_->execute_trade(seller_order.participant_id, trade, OrderSide::SELL);
    }

    if(buyer_order.quantity == 0)
    {
        active_orders_.erase(buyer_order.id);
        order_locations_.erase(buyer_order.id);
    } else
    {
        // update remaining quantity in tracking
        active_orders_[buyer_order.id].quantity = buyer_order.quantity;
    }
    
    if(seller_order.quantity == 0)
    {
        active_orders_.erase(seller_order.id);
        order_locations_.erase(seller_order.id);
    } else
    {
        // update remaining quantity in tracking
        active_orders_[seller_order.id].quantity = seller_order.quantity;
    }

    std::lock_guard<std::mutex> callback_lock(callback_mutex_);

    if(trade_callback_)
        trade_callback_(trade);
}

bool OrderBook::remove_order_from_queue(std::queue<Order>& orders, const OrderId& order_id)
{
    std::queue<Order> temp_queue;
    bool found = false;
    
    while(!orders.empty())
    {
        Order order = orders.front();
        orders.pop();
        
        if(order.id == order_id)
            found = true;
        else
            temp_queue.push(order);
    }
    
    // queue without the cancelled order
    orders = std::move(temp_queue);

    return found;
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
    Price execution_price;

    {
        std::shared_lock<std::shared_mutex> lock(book_mutex_);

        execution_price = estimate_execution_price_unsafe(order);
    }

    if(execution_price == 0.0)
        return order.type == OrderType::LIMIT;
    
    Price price_to_check = (order.type == OrderType::MARKET) ? execution_price : order.price;
    
    return portfolio_->can_buy(order.participant_id, order.symbol, order.quantity, price_to_check);
}

bool OrderBook::validate_sell_order(const Order& order) const
{
    return portfolio_->can_sell(order.participant_id, order.symbol, order.quantity);
}

Price OrderBook::estimate_execution_price_unsafe(const Order& order) const
{
    if(order.side == OrderSide::BUY)
    {
        if(!sell_orders_.empty())
            return sell_orders_.begin()->first;
    } else
    {
        if(!buy_orders_.empty())
            return buy_orders_.rbegin()->first;
    }
    
    std::lock_guard<std::mutex> lock(market_price_mutex_);

    return current_market_price_;
}

}