#include "market_data_engine.hpp"

namespace simulator
{

MarketDataEngine::MarketDataEngine():
    running_(false), rng_(std::random_device{}()) {}

MarketDataEngine::~MarketDataEngine()
{
    stop();
}

void MarketDataEngine::add_symbol(const Symbol& symbol, Price initial_price)
{
    std::unique_lock<std::shared_mutex> lock(prices_mutex_);

    prices_[symbol] = initial_price;
    volatilities_[symbol] = 0.2;
}

void MarketDataEngine::set_callback(DataCallBack callback)
{
    std::lock_guard<std::mutex> lock(callback_mutex_);

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
    std::shared_lock<std::shared_mutex> lock(prices_mutex_);

    const auto& it = prices_.find(symbol);

    return it == prices_.end() ? 0.0 : it->second;
}

std::unordered_map<Symbol, Price> MarketDataEngine::get_all_prices() const
{
    std::shared_lock<std::shared_mutex> lock(prices_mutex_);

    return prices_;
}

void MarketDataEngine::generate_data()
{
    const auto dt = 1.0 / (252 * 6.5 * 60 * 60); // 1 second in trading years

    std::normal_distribution<double> normal(0.0, 1.0);
    
    while(running_)
    {
        std::vector<std::pair<Symbol, Price>> updates;

        // generate all updates first (minimize lock time)
        {
            std::shared_lock<std::shared_mutex> read_lock(prices_mutex_);
            std::lock_guard<std::mutex> rng_lock(rng_mutex_);
            
            std::normal_distribution<double> normal(0.0, 1.0);
            
            for(const auto& [symbol, price] : prices_)
            {
                double volatility = volatilities_.at(symbol);
                double drift = 0.0;
                double shock = normal(rng_);
                
                double change = price * (drift * dt + volatility * std::sqrt(dt) * shock);
                Price new_price = std::max(0.01, price + change);
                
                updates.emplace_back(symbol, new_price);
            }
        }

        // apply updates and send callbacks
        {
            std::unique_lock<std::shared_mutex> write_lock(prices_mutex_);
            
            for(const auto& [symbol, new_price] : updates)
                prices_[symbol] = new_price;
        }

        // send callbacks outside of price lock to avoid deadlocks
        {
            std::lock_guard<std::mutex> callback_lock(callback_mutex_);
            if(callback_)
            {
                auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
                
                for(const auto& [symbol, price] : updates)
                    callback_(MarketData(symbol, price, 1000, timestamp));
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10 Hz
    }
}

}