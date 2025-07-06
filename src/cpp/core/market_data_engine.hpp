#ifndef SMS_MARKET_DATA_ENGINE_HPP
#define SMS_MARKET_DATA_ENGINE_HPP

#include "../types/common_types.hpp"

#include <functional>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <random>

namespace simulator
{

class MarketDataEngine
{
public:
    using DataCallBack = std::function<void(const MarketData&)>;

    MarketDataEngine();

    void add_symbol(const Symbol& symbol, Price initial_price);
    void set_callback(DataCallBack callback);

    void start();
    void stop();

    Price get_current_price(const Symbol& symbol) const;

private:
    std::unordered_map<Symbol, Price> prices_;
    std::unordered_map<Symbol, double> volatilities_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::mt19937 rng_;
    DataCallBack callback_;

    void generate_data();
};

}

#endif