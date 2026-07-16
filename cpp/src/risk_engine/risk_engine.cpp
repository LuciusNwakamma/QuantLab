#include "risk_engine/risk_engine.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace quantlab::risk_engine {

double Position::gross_exposure() const {
    return std::abs(quantity * current_price);
}

RiskEngine::RiskEngine(RiskLimits limits) : limits_(std::move(limits)) {}

std::pair<bool, RiskViolation>
RiskEngine::check_order(const Symbol& symbol, Side side, Quantity qty, Price price) const {
    double notional = qty * price;

    // Position size check
    auto it = positions_.find(symbol);
    double existing_notional = it != positions_.end() ? it->second.gross_exposure() : 0.0;
    if (existing_notional + notional > limits_.max_position_notional) {
        return {false, RiskViolation::PositionLimitBreached};
    }

    // Daily loss check
    if (daily_pnl_ < -limits_.max_daily_loss) {
        return {false, RiskViolation::DailyLossLimitBreached};
    }

    // Drawdown check
    double portfolio_value = std::accumulate(positions_.begin(), positions_.end(), 0.0,
        [](double acc, const auto& kv){ return acc + kv.second.market_value(); });
    if (high_watermark_ > 0.0) {
        double drawdown = (high_watermark_ - portfolio_value) / high_watermark_;
        if (drawdown > limits_.max_drawdown_pct) {
            return {false, RiskViolation::DrawdownLimitBreached};
        }
    }

    return {true, RiskViolation::None};
}

void RiskEngine::update_position(const Symbol& symbol, Side side,
                                 Quantity qty, Price fill_price) {
    auto& pos = positions_[symbol];
    pos.symbol = symbol;

    double signed_qty = (side == Side::Buy) ? qty : -qty;
    double prev_cost  = pos.avg_cost * std::abs(pos.quantity);

    pos.quantity += signed_qty;
    if (std::abs(pos.quantity) < 1e-9) {
        pos.avg_cost = 0.0;
    } else if (signed_qty > 0) {
        pos.avg_cost = (prev_cost + qty * fill_price) / std::abs(pos.quantity);
    }

    double realized = (side == Side::Sell)
        ? (fill_price - pos.avg_cost) * qty
        : 0.0;
    daily_pnl_ += realized;

    double portfolio_value = std::accumulate(positions_.begin(), positions_.end(), 0.0,
        [](double acc, const auto& kv){ return acc + kv.second.market_value(); });
    high_watermark_ = std::max(high_watermark_, portfolio_value);
}

void RiskEngine::update_market_prices(const std::unordered_map<Symbol, Price>& prices) {
    for (auto& [sym, pos] : positions_) {
        auto it = prices.find(sym);
        if (it != prices.end()) pos.current_price = it->second;
    }
}

RiskMetrics RiskEngine::compute_metrics() const {
    RiskMetrics m;
    for (auto& [sym, pos] : positions_) {
        m.gross_exposure += pos.gross_exposure();
        m.net_exposure   += pos.market_value();
    }
    if (m.gross_exposure > 0) {
        // Approximation; full implementation uses portfolio value
        m.leverage = m.gross_exposure / std::max(m.net_exposure, 1.0);
    }
    m.daily_pnl = daily_pnl_;
    return m;
}

double RiskEngine::kelly_fraction(double win_rate, double avg_win, double avg_loss) const {
    if (avg_loss < 1e-12 || avg_win < 1e-12) return 0.0;
    double b = avg_win / avg_loss;
    return win_rate - (1.0 - win_rate) / b;
}

double RiskEngine::volatility_target_size(double target_vol, double instrument_vol,
                                          double capital) const {
    if (instrument_vol < 1e-12) return 0.0;
    return (target_vol / instrument_vol) * capital;
}

double RiskEngine::historical_var(const std::vector<double>& returns,
                                  double confidence) const {
    if (returns.empty()) return 0.0;
    std::vector<double> sorted = returns;
    std::sort(sorted.begin(), sorted.end());
    std::size_t idx = static_cast<std::size_t>((1.0 - confidence) * sorted.size());
    return -sorted[std::min(idx, sorted.size() - 1)];
}

double RiskEngine::historical_cvar(const std::vector<double>& returns,
                                   double confidence) const {
    if (returns.empty()) return 0.0;
    std::vector<double> sorted = returns;
    std::sort(sorted.begin(), sorted.end());
    std::size_t cutoff = static_cast<std::size_t>((1.0 - confidence) * sorted.size());
    if (cutoff == 0) return -sorted[0];
    double sum = std::accumulate(sorted.begin(), sorted.begin() + cutoff, 0.0);
    return -sum / static_cast<double>(cutoff);
}

void RiskEngine::set_alert_callback(RiskAlertCallback cb) {
    alert_cb_ = std::move(cb);
}

void RiskEngine::reset_daily_pnl() { daily_pnl_ = 0.0; }

const std::unordered_map<Symbol, Position>& RiskEngine::positions() const {
    return positions_;
}

} // namespace quantlab::risk_engine
