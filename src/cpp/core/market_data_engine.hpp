#ifndef SMS_MARKET_DATA_ENGINE_HPP
#define SMS_MARKET_DATA_ENGINE_HPP

#include "../types/common_types.hpp"

#include <functional>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <random>
#include <mutex>
#include <shared_mutex>

namespace simulator
{

class MarketDataEngine
{
public:
    using DataCallBack = std::function<void(const MarketData&)>;

    MarketDataEngine();
    ~MarketDataEngine();

    void add_symbol(const Symbol& symbol, Price initial_price);
    void set_callback(DataCallBack callback);

    void start();
    void stop();

    Price get_current_price(const Symbol& symbol) const;

    std::unordered_map<Symbol, Price> get_all_prices() const;

private:
    std::unordered_map<Symbol, Price> prices_;
    std::unordered_map<Symbol, double> volatilities_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::mt19937 rng_;
    DataCallBack callback_;

    mutable std::shared_mutex prices_mutex_;
    mutable std::mutex callback_mutex_;
    mutable std::mutex rng_mutex_;

    void generate_data();
};

}

#endif