#include "portfolio.hpp"

#include <stdexcept>

namespace simulator
{

Portfolio::Portfolio(const std::unordered_map<ParticipantId, double>& initial_cash_by_participant)
{
    for(const auto& [participant_id, cash] : initial_cash_by_participant)
        participants_[participant_id] = ParticipantData(cash);
}

void Portfolio::add_participant(const ParticipantId& participant_id, double initial_cash)
{
    std::lock_guard<std::mutex> lock(portfolio_mutex_);
    participants_[participant_id] = ParticipantData(initial_cash);
}

void Portfolio::set_initial_position(const ParticipantId& participant_id, const Symbol& symbol, int quantity, double cost_basis)
{
    std::lock_guard<std::mutex> lock(portfolio_mutex_);

    ParticipantData& participant = participants_[participant_id];
        
    // set position
    participant.positions[symbol] = quantity;
    
    // adjust cash for cost basis
    if(cost_basis > 0.0)
    {
        double total_cost = quantity * cost_basis;
        participant.cash -= total_cost;
    }
}

bool Portfolio::can_buy(const ParticipantId& participant_id, const Symbol& symbol, Quantity qty, Price price)
{
    std::lock_guard<std::mutex> lock(portfolio_mutex_);
    
    try
    {
        const auto& participant = get_participant_data(participant_id);
        return qty * price <= participant.cash;
    } catch(const std::exception&)
    {
        return false;
    }
}

bool Portfolio::can_sell(const ParticipantId& participant_id, const Symbol& symbol, Quantity qty)
{
    std::lock_guard<std::mutex> lock(portfolio_mutex_);
    
    try
    {
        const auto& participant = get_participant_data(participant_id);
        const auto& it = participant.positions.find(symbol);
        return it != participant.positions.end() && it->second >= qty;
    } catch(const std::exception&)
    {
        return false;
    }
}

void Portfolio::execute_trade(const ParticipantId& participant_id, const Trade& trade, OrderSide side)
{
    std::lock_guard<std::mutex> lock(portfolio_mutex_);

    auto& participant = get_participant_data(participant_id);

    int multiplier = side == OrderSide::BUY ? 1 : -1;
    
    // update position
    participant.positions[trade.symbol] += multiplier * trade.quantity;

    // update cash
    participant.cash -= multiplier * trade.quantity * trade.price;
}

double Portfolio::get_pnl(const ParticipantId& participant_id, const std::unordered_map<Symbol, Price>& prices) const
{
    std::lock_guard<std::mutex> lock(portfolio_mutex_);
    
    const auto& participant = get_participant_data(participant_id);

    double position_value = 0.0;

    for(const auto& [symbol, quantity] : participant.positions)
    {
        const auto& it = prices.find(symbol);

        if(it != prices.end())
            position_value += it->second * quantity;
    }

    return position_value + participant.cash - participant.initial_cash;
}

double Portfolio::get_portfolio_value(const ParticipantId& participant_id, const std::unordered_map<Symbol, Price>& prices) const
{
    std::lock_guard<std::mutex> lock(portfolio_mutex_);
    
    const auto& participant = get_participant_data(participant_id);

    double position_value = 0.0;

    for(const auto& [symbol, quantity] : participant.positions)
    {
        const auto& it = prices.find(symbol);

        if(it != prices.end())
            position_value += it->second * quantity;
    }

    return participant.cash + position_value;
}

double Portfolio::get_cash(const ParticipantId& participant_id) const
{
    std::lock_guard<std::mutex> lock(portfolio_mutex_);

    return get_participant_data(participant_id).cash;
}

Quantity Portfolio::get_position(const ParticipantId& participant_id, const Symbol& symbol) const
{
    std::lock_guard<std::mutex> lock(portfolio_mutex_);
    
    const auto& participant = get_participant_data(participant_id);
    const auto& it = participant.positions.find(symbol);

    return it != participant.positions.end() ? it->second : 0;
}

double Portfolio::get_buying_power(const ParticipantId& participant_id) const
{
    std::lock_guard<std::mutex> lock(portfolio_mutex_);

    return get_participant_data(participant_id).cash;
}

double Portfolio::get_total_exposure(const ParticipantId& participant_id, const std::unordered_map<Symbol, Price>& prices) const
{
    std::lock_guard<std::mutex> lock(portfolio_mutex_);
    
    const auto& participant = get_participant_data(participant_id);
    double total_exposure = 0.0;

    for(const auto& [symbol, quantity] : participant.positions)
    {
        const auto& price_it = prices.find(symbol);

        if (price_it != prices.end())
            total_exposure += std::abs(quantity) * price_it->second;
    }

    return total_exposure;
}

Portfolio::ParticipantData& Portfolio::get_participant_data(const ParticipantId& participant_id)
{
    const auto& it = participants_.find(participant_id);

    if(it == participants_.end())
        throw std::runtime_error("Participant not found: " + participant_id);

    return it->second;
}

const Portfolio::ParticipantData& Portfolio::get_participant_data(const ParticipantId& participant_id) const
{
    const auto& it = participants_.find(participant_id);

    if(it == participants_.end())
        throw std::runtime_error("Participant not found: " + participant_id);

    return it->second;
}

}