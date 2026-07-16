#pragma once
#include "common/types.hpp"
#include <vector>
#include <string>

namespace quantlab::portfolio_optimizer {

enum class OptimizationMethod {
    MeanVariance,       // Markowitz
    BlackLitterman,
    RiskParity,
    HierarchicalRiskParity,
    MinimumVariance,
    MaximumSharpe,
    EqualRiskContribution
};

struct OptimizationInput {
    std::vector<Symbol>              symbols;
    std::vector<std::vector<double>> returns_matrix; // [n_periods][n_assets]
    std::vector<double>              expected_returns; // optional (for BL)
    std::vector<std::vector<double>> views_matrix;   // BL views P
    std::vector<double>              views_vector;   // BL views Q
    double                           risk_free_rate{0.045};
    double                           target_return{-1.0};  // -1 = no constraint
    double                           target_vol{-1.0};
};

struct OptimizationResult {
    std::vector<double> weights;
    double              expected_return;
    double              expected_volatility;
    double              sharpe_ratio;
    std::vector<double> risk_contributions; // per-asset
};

class PortfolioOptimizer {
public:
    OptimizationResult optimize(const OptimizationInput& input,
                                OptimizationMethod method = OptimizationMethod::MaximumSharpe);

    // Covariance estimation
    static std::vector<std::vector<double>>
        sample_covariance(const std::vector<std::vector<double>>& returns);

    static std::vector<std::vector<double>>
        ledoit_wolf_shrinkage(const std::vector<std::vector<double>>& returns);

private:
    OptimizationResult mean_variance(const OptimizationInput& input) const;
    OptimizationResult black_litterman(const OptimizationInput& input) const;
    OptimizationResult risk_parity(const OptimizationInput& input) const;
    OptimizationResult hierarchical_risk_parity(const OptimizationInput& input) const;
    OptimizationResult min_variance(const OptimizationInput& input) const;
    OptimizationResult max_sharpe(const OptimizationInput& input) const;
    OptimizationResult equal_risk_contribution(const OptimizationInput& input) const;
};

} // namespace quantlab::portfolio_optimizer
