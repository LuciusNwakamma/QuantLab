#pragma once
#include "common/types.hpp"
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>

namespace quantlab::risk_engine {

struct Position {
    Symbol   symbol;
    Quantity quantity{0.0};   // positive = long, negative = short
    Price    avg_cost{0.0};
    Price    current_price{0.0};

    [[nodiscard]] double market_value()     const { return quantity * current_price; }
    [[nodiscard]] double unrealized_pnl()  const { return (current_price - avg_cost) * quantity; }
    [[nodiscard]] double gross_exposure()  const;
};

struct RiskLimits {
    double max_position_notional{1'000'000.0};
    double max_portfolio_notional{10'000'000.0};
    double max_drawdown_pct{0.10};
    double max_daily_loss{50'000.0};
    double max_single_loss{10'000.0};
    double max_leverage{3.0};
    double var_limit_95{100'000.0};
};

struct RiskMetrics {
    double portfolio_var_95{0.0};
    double portfolio_cvar_95{0.0};
    double current_drawdown{0.0};
    double daily_pnl{0.0};
    double gross_exposure{0.0};
    double net_exposure{0.0};
    double leverage{0.0};
    double portfolio_beta{0.0};
    double portfolio_volatility{0.0};
};

enum class RiskViolation {
    None,
    PositionLimitBreached,
    DrawdownLimitBreached,
    DailyLossLimitBreached,
    LeverageBreached,
    VaRBreached,
    SingleLossBreached
};

using RiskAlertCallback = std::function<void(RiskViolation, const std::string&)>;

class RiskEngine {
public:
    explicit RiskEngine(RiskLimits limits = {});

    // Order pre-trade check
    [[nodiscard]] std::pair<bool, RiskViolation>
        check_order(const Symbol& symbol, Side side, Quantity qty, Price price) const;

    // Position updates
    void update_position(const Symbol& symbol, Side side, Quantity qty, Price fill_price);
    void update_market_prices(const std::unordered_map<Symbol, Price>& prices);

    // Metrics
    [[nodiscard]] RiskMetrics compute_metrics() const;
    [[nodiscard]] double      kelly_fraction(double win_rate, double avg_win, double avg_loss) const;
    [[nodiscard]] double      volatility_target_size(double target_vol, double instrument_vol,
                                                     double capital) const;
    // VaR & CVaR (historical simulation)
    [[nodiscard]] double      historical_var(const std::vector<double>& returns,
                                             double confidence = 0.95) const;
    [[nodiscard]] double      historical_cvar(const std::vector<double>& returns,
                                              double confidence = 0.95) const;

    void set_alert_callback(RiskAlertCallback cb);
    void reset_daily_pnl();

    [[nodiscard]] const std::unordered_map<Symbol, Position>& positions() const;

private:
    RiskLimits                             limits_;
    std::unordered_map<Symbol, Position>   positions_;
    double                                 high_watermark_{0.0};
    double                                 daily_pnl_{0.0};
    RiskAlertCallback                      alert_cb_;
};

} // namespace quantlab::risk_engine
