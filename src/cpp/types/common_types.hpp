#ifndef SMS_COMMON_TYPES_HPP
#define SMS_COMMON_TYPES_HPP

#include <string>
#include <chrono>
#include <atomic>

namespace simulator
{

using Price = double;
using Quantity = int;
using OrderId = std::string;
using Symbol = std::string;
using Timestamp = std::chrono::milliseconds;
using ParticipantId = std::string;

enum class OrderType
{
    MARKET,
    LIMIT
};

enum class OrderSide
{
    BUY,
    SELL
};

struct MarketData
{
    Symbol symbol;
    Price price;
    Quantity volume;
    Timestamp timestamp;
    Price bid;
    Price ask;

    MarketData(const Symbol& sym, Price p, Quantity v, Timestamp ts):
        symbol(sym), price(p), volume(v), timestamp(ts), bid(p * 0.999), ask(p * 1.001) {}
};

class OrderIdGenerator
{
public:
    static OrderId generate()
    {
        static std::atomic<int> counter{ 0 };

        return "ORDER_" + std::to_string(++counter);
    }
};

struct Order
{
    OrderId id;
    ParticipantId participant_id;
    Symbol symbol;
    OrderType type;
    OrderSide side;
    Quantity quantity;
    Price price;
    Timestamp timestamp;

    Order() {}

    Order(const ParticipantId& p_id, const Symbol& sym, OrderSide s, Quantity qty, OrderType t = OrderType::MARKET, Price p = 0.0):
        participant_id(p_id), symbol(sym), type(t), side(s), quantity(qty), price(p)
    {
        id = OrderIdGenerator::generate();
        
        timestamp = std::chrono::duration_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch());
    }
};

struct Trade
{
    OrderId buy_order_id;
    OrderId sell_order_id;
    Symbol symbol;
    Quantity quantity;
    Price price;
    Timestamp timestamp;
    ParticipantId buyer_id;
    ParticipantId seller_id;

    Trade(const OrderId& buy_id, const OrderId& sell_id, const Symbol& sym, Quantity qty, Price p, Timestamp ts, ParticipantId buyer, ParticipantId seller):
        buy_order_id(buy_id), sell_order_id(sell_id), symbol(sym), quantity(qty), price(p), timestamp(ts), buyer_id(buyer), seller_id(seller) {}
    
    double notional_value() const
    {
        return quantity * price;
    }
};

}

#endif