#ifndef SMS_PORTFOLIO_HPP
#define SMS_PORTFOLIO_HPP

#include "../types/common_types.hpp"

#include <unordered_map>
#include <mutex>

namespace simulator
{

class Portfolio
{
public:
    Portfolio(const std::unordered_map<ParticipantId, double>& initial_cash_by_participant);

    void add_participant(const ParticipantId& participant_id, double initial_cash);

    bool can_buy(const ParticipantId& participant_id, const Symbol& symbol, Quantity qty, Price price);
    bool can_sell(const ParticipantId& participant_id, const Symbol& symbol, Quantity qty);

    void execute_trade(const ParticipantId& participant_id, const Trade& trade, OrderSide side);

    double get_pnl(const ParticipantId& participant_id, const std::unordered_map<Symbol, Price>& prices) const;
    double get_portfolio_value(const ParticipantId& participant_id, const std::unordered_map<Symbol, Price>& prices) const;

    double get_cash(const ParticipantId& participant_id) const;
    Quantity get_position(const ParticipantId& participant_id, const Symbol& symbol) const;

    double get_buying_power(const ParticipantId& participant_id) const;
    double get_total_exposure(const ParticipantId& participant_id, const std::unordered_map<Symbol, Price>& prices) const;

private:
    struct ParticipantData
    {
        std::unordered_map<Symbol, Quantity> positions;
        double cash;
        double initial_cash;

        ParticipantData(double initial):
            cash(initial), initial_cash(initial) {}
    };
    
    std::unordered_map<ParticipantId, ParticipantData> participants_;
    mutable std::mutex portfolio_mutex_;

    ParticipantData& get_participant_data(const ParticipantId& participant_id);
    const ParticipantData& get_participant_data(const ParticipantId& participant_id) const;
};

}

#endif