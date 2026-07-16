#pragma once
#include "common/types.hpp"
#include <vector>
#include <random>
#include <functional>

namespace quantlab::monte_carlo {

struct MCConfig {
    int    simulations{10'000};
    int    horizon_days{252};
    double dt{1.0 / 252.0};
    unsigned int seed{42};
};

struct MCResult {
    std::vector<double> final_values;
    double              mean;
    double              std_dev;
    double              var_95;
    double              cvar_95;
    double              var_99;
    double              cvar_99;
    double              prob_loss;
    std::vector<std::vector<double>> paths; // subset for visualization
};

class MonteCarloSimulator {
public:
    explicit MonteCarloSimulator(MCConfig cfg = {});

    // GBM price paths
    MCResult simulate_gbm(double initial_price,
                          double mu,
                          double sigma) const;

    // Portfolio simulation with correlated assets
    MCResult simulate_portfolio(const std::vector<double>& initial_prices,
                                const std::vector<double>& drifts,
                                const std::vector<double>& vols,
                                const std::vector<std::vector<double>>& correlation_matrix,
                                const std::vector<double>& weights) const;

    // Bootstrap returns simulation
    MCResult bootstrap_returns(const std::vector<double>& historical_returns,
                               double initial_capital = 1'000'000.0) const;

    // Cholesky decomposition for correlated draws
    static std::vector<std::vector<double>>
        cholesky(const std::vector<std::vector<double>>& cov_matrix);

private:
    MCConfig     cfg_;
    mutable std::mt19937 rng_;
};

} // namespace quantlab::monte_carlo
