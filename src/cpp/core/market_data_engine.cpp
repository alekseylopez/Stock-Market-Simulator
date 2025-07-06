#include "market_data_engine.hpp"

namespace simulator
{

MarketDataEngine::MarketDataEngine():
    running_(false), rng_(std::random_device{}()) {}

void MarketDataEngine::add_symbol(const Symbol& symbol, Price initial_price)
{
    prices_[symbol] = initial_price;
    volatilities_[symbol] = 0.2;
}

void MarketDataEngine::set_callback(DataCallBack callback)
{
    callback_ = callback;
}

void MarketDataEngine::start()
{
    running_ = true;

    thread_ = std::thread(&MarketDataEngine::generate_data, this);
}

void MarketDataEngine::stop()
{
    running_ = false;

    if(thread_.joinable())
        thread_.join();
}

Price MarketDataEngine::get_current_price(const Symbol& symbol) const
{
    const auto& it = prices_.find(symbol);

    return it == prices_.end() ? 0.0 : it->second;
}

void MarketDataEngine::generate_data()
{
    const auto dt = 1.0 / (252 * 24 * 60 * 60); // 1 second in years

    std::normal_distribution<double> normal(0.0, 1.0);
    
    while(running_)
    {
        for(auto& [symbol, price] : prices_)
        {
            // simple geometric Brownian Motion
            double volatility = volatilities_[symbol];
            double drift = 0.0; // risk-free rate
            double shock = normal(rng_);
            
            // dS = S * (drift * dt + volatility * sqrt(dt) * shock)
            double change = price * (drift * dt + volatility * std::sqrt(dt) * shock);
            price = std::max(0.01, price + change); // clamp to prevent negative prices
            
            if(callback_)
            {
                auto timestamp = std::chrono::duration_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch());
                callback_(MarketData(symbol, price, 1000, timestamp));
            }
        }
        
        std::this_thread::sleep_for(Timestamp(100)); // 10 Hz
    }
}

}