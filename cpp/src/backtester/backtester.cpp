#include "backtester/backtester.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <stdexcept>

namespace quantlab::backtester {

Backtester::Backtester(BacktestConfig config) : config_(std::move(config)) {}

void Backtester::add_data(const Symbol& symbol, std::vector<Bar> bars) {
    std::sort(bars.begin(), bars.end(),
              [](const Bar& a, const Bar& b){ return a.timestamp < b.timestamp; });
    data_[symbol] = std::move(bars);
}

void Backtester::set_strategy(StrategyFn strategy) {
    strategy_ = std::move(strategy);
}

BacktestResult Backtester::run() {
    if (!strategy_) throw std::runtime_error("No strategy set");
    if (data_.empty()) throw std::runtime_error("No data loaded");

    std::vector<double> equity_curve;
    std::vector<BacktestTrade> trades;
    std::unordered_map<Symbol, double> current_weights;
    std::unordered_map<Symbol, double> positions; // quantity held

    double capital = config_.initial_capital;
    equity_curve.push_back(capital);

    // Build a sorted timeline of bar indices per symbol
    std::size_t max_bars = 0;
    for (auto& [sym, bars] : data_) max_bars = std::max(max_bars, bars.size());

    for (std::size_t i = 1; i < max_bars; ++i) {
        // Gather current bar slice
        std::vector<Bar> history_slice;
        std::unordered_map<Symbol, Price> prices;
        for (auto& [sym, bars] : data_) {
            if (i < bars.size()) {
                prices[sym] = bars[i].close;
                for (std::size_t j = 0; j <= i; ++j) history_slice.push_back(bars[j]);
            }
        }

        auto signals = strategy_(history_slice, current_weights);

        // Execute signals
        for (auto& sig : signals) {
            auto pit = prices.find(sig.symbol);
            if (pit == prices.end()) continue;
            Price px = pit->second;

            double target_value = capital * sig.target_weight;
            double current_qty  = positions.count(sig.symbol) ? positions[sig.symbol] : 0.0;
            double current_value = current_qty * px;
            double delta_value  = target_value - current_value;
            if (std::abs(delta_value) < 1.0) continue;

            Side   side = delta_value > 0 ? Side::Buy : Side::Sell;
            Quantity qty = std::abs(delta_value) / px;

            BacktestTrade trade;
            double fill_px = apply_costs(side, px, qty, qty * 252, trade);
            trade.timestamp = data_[sig.symbol][i].timestamp;
            trade.symbol    = sig.symbol;
            trade.side      = side;
            trade.quantity  = qty;

            positions[sig.symbol] = current_qty + (side == Side::Buy ? qty : -qty);
            capital -= (side == Side::Buy ? 1 : -1) * (fill_px * qty)
                       + trade.commission + trade.slippage;
            trades.push_back(trade);
        }

        // Mark-to-market
        double portfolio_val = capital;
        for (auto& [sym, qty] : positions) {
            auto pit = prices.find(sym);
            if (pit != prices.end()) portfolio_val += qty * pit->second;
        }
        equity_curve.push_back(portfolio_val);
    }

    return compute_metrics(equity_curve, trades);
}

double Backtester::apply_costs(Side side, Price price, Quantity qty,
                               double /*adv*/, BacktestTrade& trade) {
    double slip = price * (config_.slippage_bps / 10000.0);
    Price fill_px = (side == Side::Buy) ? price + slip : price - slip;

    double comm = std::max(config_.commission_min,
                           qty * config_.commission_per_share);
    trade.fill_price = fill_px;
    trade.commission = comm;
    trade.slippage   = std::abs(fill_px - price) * qty;
    return fill_px;
}

BacktestResult Backtester::compute_metrics(const std::vector<double>& equity,
                                           const std::vector<BacktestTrade>& trades) const {
    BacktestResult r;
    r.equity_curve = equity;
    r.trades       = trades;
    if (equity.size() < 2) return r;

    // Daily returns
    for (std::size_t i = 1; i < equity.size(); ++i)
        r.daily_returns.push_back((equity[i] - equity[i-1]) / equity[i-1]);

    r.total_return       = (equity.back() - equity.front()) / equity.front();
    double n_years       = static_cast<double>(r.daily_returns.size()) / 252.0;
    r.annualized_return  = std::pow(1.0 + r.total_return, 1.0 / std::max(n_years, 1e-6)) - 1.0;

    double mean_ret = std::accumulate(r.daily_returns.begin(), r.daily_returns.end(), 0.0)
                      / r.daily_returns.size();
    double variance = 0.0;
    for (double d : r.daily_returns) variance += (d - mean_ret) * (d - mean_ret);
    variance /= std::max<std::size_t>(r.daily_returns.size() - 1, 1);
    double vol = std::sqrt(variance) * std::sqrt(252.0);

    r.sharpe_ratio = vol > 1e-12 ? (r.annualized_return - 0.045) / vol : 0.0;

    // Max drawdown
    double peak = equity.front();
    for (double v : equity) {
        peak = std::max(peak, v);
        r.max_drawdown = std::max(r.max_drawdown, (peak - v) / peak);
    }

    r.calmar_ratio = r.max_drawdown > 1e-12 ? r.annualized_return / r.max_drawdown : 0.0;
    r.total_trades = static_cast<int>(trades.size());

    double total_comm = 0.0, total_slip = 0.0;
    for (auto& t : trades) { total_comm += t.commission; total_slip += t.slippage; }
    r.total_commission = total_comm;
    r.total_slippage   = total_slip;

    return r;
}

std::vector<BacktestResult> Backtester::walk_forward(int train_periods, int test_periods) {
    std::vector<BacktestResult> results;
    for (auto& [sym, bars] : data_) {
        int n = static_cast<int>(bars.size());
        for (int start = 0; start + train_periods + test_periods <= n;
             start += test_periods) {
            Backtester bt(config_);
            bt.set_strategy(strategy_);
            bt.add_data(sym, {bars.begin() + start,
                              bars.begin() + start + train_periods + test_periods});
            results.push_back(bt.run());
        }
    }
    return results;
}

std::vector<BacktestResult> Backtester::monte_carlo(int simulations, unsigned int seed) {
    std::vector<BacktestResult> results;
    std::mt19937 rng(seed);
    if (data_.empty() || strategy_ == nullptr) return results;

    auto& [ref_sym, ref_bars] = *data_.begin();
    std::size_t n = ref_bars.size();

    for (int s = 0; s < simulations; ++s) {
        // Shuffle bar ordering to simulate path uncertainty
        std::vector<std::size_t> idx(n);
        std::iota(idx.begin(), idx.end(), 0);
        std::shuffle(idx.begin(), idx.end(), rng);

        Backtester bt(config_);
        bt.set_strategy(strategy_);
        for (auto& [sym, bars] : data_) {
            std::vector<Bar> shuffled;
            shuffled.reserve(n);
            for (auto i : idx) if (i < bars.size()) shuffled.push_back(bars[i]);
            bt.add_data(sym, shuffled);
        }
        results.push_back(bt.run());
    }
    return results;
}

} // namespace quantlab::backtester
