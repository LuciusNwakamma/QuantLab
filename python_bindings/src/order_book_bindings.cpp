#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <chrono>
#include <string>
#include "order_book/order_book.hpp"
#include "risk_engine/risk_engine.hpp"
#include "backtester/backtester.hpp"

namespace py = pybind11;
using namespace quantlab;
using namespace quantlab::order_book;
using namespace quantlab::risk_engine;
using namespace quantlab::backtester;

namespace {

Side parse_side(const std::string& side) {
    if (side == "buy" || side == "BUY") return Side::Buy;
    if (side == "sell" || side == "SELL") return Side::Sell;
    throw std::invalid_argument("side must be 'buy' or 'sell'");
}

OrderType parse_order_type(const std::string& order_type) {
    if (order_type == "market") return OrderType::Market;
    if (order_type == "limit") return OrderType::Limit;
    if (order_type == "stop") return OrderType::Stop;
    if (order_type == "stop_limit") return OrderType::StopLimit;
    throw std::invalid_argument("order_type must be one of: market, limit, stop, stop_limit");
}

TimeInForce parse_tif(const std::string& tif) {
    if (tif == "GTC") return TimeInForce::GTC;
    if (tif == "DAY") return TimeInForce::DAY;
    if (tif == "IOC") return TimeInForce::IOC;
    if (tif == "FOK") return TimeInForce::FOK;
    throw std::invalid_argument("tif must be one of: GTC, DAY, IOC, FOK");
}

Order make_order(
    std::uint64_t id,
    const std::string& symbol,
    const std::string& side,
    const std::string& order_type,
    double price,
    double quantity,
    const std::string& tif
) {
    Order o{};
    o.id = id;
    o.symbol = symbol;
    o.side = parse_side(side);
    o.type = parse_order_type(order_type);
    o.price = price;
    o.quantity = quantity;
    o.filled = 0.0;
    o.tif = parse_tif(tif);
    o.timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now()
    );
    return o;
}

py::dict price_level_to_dict(const PriceLevel& lvl) {
    py::dict d;
    d["price"] = lvl.price;
    d["total_quantity"] = lvl.total_quantity;
    d["num_orders"] = lvl.orders.size();
    return d;
}

Bar make_bar(
    const std::string& symbol,
    const Timestamp& timestamp,
    double open,
    double high,
    double low,
    double close,
    double volume,
    double vwap
) {
    Bar b{};
    b.timestamp = timestamp;
    b.symbol = symbol;
    b.open = open;
    b.high = high;
    b.low = low;
    b.close = close;
    b.volume = volume;
    b.vwap = vwap;
    return b;
}

} // namespace

PYBIND11_MODULE(quantlab_cpp, m) {
    m.doc() = "QuantLab C++ bindings (OrderBook + RiskEngine + Backtester)";

    py::class_<OrderBook>(m, "OrderBook")
        .def(py::init<Symbol>(), py::arg("symbol"))
        .def("add_order", [](OrderBook& self,
                              std::uint64_t id,
                              const std::string& symbol,
                              const std::string& side,
                              const std::string& order_type,
                              double price,
                              double quantity,
                              const std::string& tif) {
            self.add_order(make_order(id, symbol, side, order_type, price, quantity, tif));
        },
        py::arg("id") = 0,
        py::arg("symbol"),
        py::arg("side"),
        py::arg("order_type") = "limit",
        py::arg("price") = 0.0,
        py::arg("quantity") = 0.0,
        py::arg("tif") = "GTC")
        .def("add_orders_batch", [](OrderBook& self,
                                     const std::vector<std::tuple<std::uint64_t, std::string, double, double>>& orders,
                                     const std::string& symbol,
                                     const std::string& order_type,
                                     const std::string& tif) {
            for (const auto& [id, side, price, quantity] : orders) {
                self.add_order(make_order(id, symbol, side, order_type, price, quantity, tif));
            }
        },
        py::arg("orders"),
        py::arg("symbol"),
        py::arg("order_type") = "limit",
        py::arg("tif") = "GTC")
        .def("cancel_order", &OrderBook::cancel_order, py::arg("id"))
        .def("best_bid", &OrderBook::best_bid)
        .def("best_ask", &OrderBook::best_ask)
        .def("mid_price", &OrderBook::mid_price)
        .def("spread", &OrderBook::spread)
        .def("bid_depth", &OrderBook::bid_depth, py::arg("levels") = 5)
        .def("ask_depth", &OrderBook::ask_depth, py::arg("levels") = 5)
        .def("order_imbalance", &OrderBook::order_imbalance, py::arg("levels") = 5)
        .def("bid_levels", [](const OrderBook& self, int depth) {
            py::list out;
            for (const auto& lvl : self.bid_levels(depth)) out.append(price_level_to_dict(lvl));
            return out;
        }, py::arg("depth") = 10)
        .def("ask_levels", [](const OrderBook& self, int depth) {
            py::list out;
            for (const auto& lvl : self.ask_levels(depth)) out.append(price_level_to_dict(lvl));
            return out;
        }, py::arg("depth") = 10);

    py::enum_<RiskViolation>(m, "RiskViolation")
        .value("None_", RiskViolation::None)
        .value("PositionLimitBreached", RiskViolation::PositionLimitBreached)
        .value("DrawdownLimitBreached", RiskViolation::DrawdownLimitBreached)
        .value("DailyLossLimitBreached", RiskViolation::DailyLossLimitBreached)
        .value("LeverageBreached", RiskViolation::LeverageBreached)
        .value("VaRBreached", RiskViolation::VaRBreached)
        .value("SingleLossBreached", RiskViolation::SingleLossBreached)
        .export_values();

    py::class_<RiskLimits>(m, "RiskLimits")
        .def(py::init<>())
        .def_readwrite("max_position_notional", &RiskLimits::max_position_notional)
        .def_readwrite("max_portfolio_notional", &RiskLimits::max_portfolio_notional)
        .def_readwrite("max_drawdown_pct", &RiskLimits::max_drawdown_pct)
        .def_readwrite("max_daily_loss", &RiskLimits::max_daily_loss)
        .def_readwrite("max_single_loss", &RiskLimits::max_single_loss)
        .def_readwrite("max_leverage", &RiskLimits::max_leverage)
        .def_readwrite("var_limit_95", &RiskLimits::var_limit_95);

    py::class_<RiskMetrics>(m, "RiskMetrics")
        .def(py::init<>())
        .def_readonly("portfolio_var_95", &RiskMetrics::portfolio_var_95)
        .def_readonly("portfolio_cvar_95", &RiskMetrics::portfolio_cvar_95)
        .def_readonly("current_drawdown", &RiskMetrics::current_drawdown)
        .def_readonly("daily_pnl", &RiskMetrics::daily_pnl)
        .def_readonly("gross_exposure", &RiskMetrics::gross_exposure)
        .def_readonly("net_exposure", &RiskMetrics::net_exposure)
        .def_readonly("leverage", &RiskMetrics::leverage)
        .def_readonly("portfolio_beta", &RiskMetrics::portfolio_beta)
        .def_readonly("portfolio_volatility", &RiskMetrics::portfolio_volatility);

    py::class_<RiskEngine>(m, "RiskEngine")
        .def(py::init<RiskLimits>(), py::arg("limits") = RiskLimits{})
        .def("check_order", [](const RiskEngine& self,
                               const std::string& symbol,
                               const std::string& side,
                               double qty,
                               double price) {
            auto [ok, v] = self.check_order(symbol, parse_side(side), qty, price);
            return py::make_tuple(ok, v);
        }, py::arg("symbol"), py::arg("side"), py::arg("qty"), py::arg("price"))
        .def("update_position", [](RiskEngine& self,
                                    const std::string& symbol,
                                    const std::string& side,
                                    double qty,
                                    double fill_price) {
            self.update_position(symbol, parse_side(side), qty, fill_price);
        }, py::arg("symbol"), py::arg("side"), py::arg("qty"), py::arg("fill_price"))
        .def("update_market_prices", &RiskEngine::update_market_prices, py::arg("prices"))
        .def("compute_metrics", &RiskEngine::compute_metrics)
        .def("kelly_fraction", &RiskEngine::kelly_fraction,
             py::arg("win_rate"), py::arg("avg_win"), py::arg("avg_loss"))
        .def("volatility_target_size", &RiskEngine::volatility_target_size,
             py::arg("target_vol"), py::arg("instrument_vol"), py::arg("capital"))
        .def("historical_var", &RiskEngine::historical_var,
             py::arg("returns"), py::arg("confidence") = 0.95)
        .def("historical_cvar", &RiskEngine::historical_cvar,
             py::arg("returns"), py::arg("confidence") = 0.95)
        .def("reset_daily_pnl", &RiskEngine::reset_daily_pnl)
        .def("positions", [](const RiskEngine& self) {
            py::dict out;
            for (const auto& [sym, p] : self.positions()) {
                py::dict entry;
                entry["symbol"] = p.symbol;
                entry["quantity"] = p.quantity;
                entry["avg_cost"] = p.avg_cost;
                entry["current_price"] = p.current_price;
                entry["market_value"] = p.market_value();
                entry["unrealized_pnl"] = p.unrealized_pnl();
                entry["gross_exposure"] = p.gross_exposure();
                out[sym.c_str()] = entry;
            }
            return out;
        });

    py::class_<BacktestConfig>(m, "BacktestConfig")
        .def(py::init<>())
        .def_readwrite("initial_capital", &BacktestConfig::initial_capital)
        .def_readwrite("resolution", &BacktestConfig::resolution)
        .def_readwrite("commission_per_share", &BacktestConfig::commission_per_share)
        .def_readwrite("commission_min", &BacktestConfig::commission_min)
        .def_readwrite("slippage_bps", &BacktestConfig::slippage_bps)
        .def_readwrite("bid_ask_half_spread_bps", &BacktestConfig::bid_ask_half_spread_bps)
        .def_readwrite("market_impact_coeff", &BacktestConfig::market_impact_coeff)
        .def_readwrite("latency_ms", &BacktestConfig::latency_ms)
        .def_readwrite("allow_partial_fills", &BacktestConfig::allow_partial_fills);

    py::class_<BacktestTrade>(m, "BacktestTrade")
        .def_readonly("symbol", &BacktestTrade::symbol)
        .def_readonly("fill_price", &BacktestTrade::fill_price)
        .def_readonly("quantity", &BacktestTrade::quantity)
        .def_readonly("commission", &BacktestTrade::commission)
        .def_readonly("slippage", &BacktestTrade::slippage);

    py::class_<BacktestResult>(m, "BacktestResult")
        .def_readonly("total_return", &BacktestResult::total_return)
        .def_readonly("annualized_return", &BacktestResult::annualized_return)
        .def_readonly("sharpe_ratio", &BacktestResult::sharpe_ratio)
        .def_readonly("sortino_ratio", &BacktestResult::sortino_ratio)
        .def_readonly("calmar_ratio", &BacktestResult::calmar_ratio)
        .def_readonly("max_drawdown", &BacktestResult::max_drawdown)
        .def_readonly("win_rate", &BacktestResult::win_rate)
        .def_readonly("profit_factor", &BacktestResult::profit_factor)
        .def_readonly("total_commission", &BacktestResult::total_commission)
        .def_readonly("total_slippage", &BacktestResult::total_slippage)
        .def_readonly("total_trades", &BacktestResult::total_trades)
        .def_readonly("equity_curve", &BacktestResult::equity_curve)
        .def_readonly("daily_returns", &BacktestResult::daily_returns)
        .def_readonly("trades", &BacktestResult::trades);

    py::class_<Backtester>(m, "Backtester")
        .def(py::init<BacktestConfig>(), py::arg("config") = BacktestConfig{})
        .def("add_ohlcv_batch", [](Backtester& self,
                                   const std::string& symbol,
                                   const std::vector<std::tuple<double, double, double, double, double, double>>& bars) {
            std::vector<Bar> native_bars;
            native_bars.reserve(bars.size());
            auto base_ts = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());

            for (std::size_t i = 0; i < bars.size(); ++i) {
                const auto& [open, high, low, close, volume, vwap] = bars[i];
                native_bars.push_back(make_bar(
                    symbol,
                    base_ts + std::chrono::hours(24 * static_cast<int>(i)),
                    open,
                    high,
                    low,
                    close,
                    volume,
                    vwap
                ));
            }

            self.add_data(symbol, std::move(native_bars));
        }, py::arg("symbol"), py::arg("bars"))
        .def("run_constant_weight", [](Backtester& self,
                                         const std::string& symbol,
                                         double target_weight) {
            self.set_strategy([symbol, target_weight](
                const std::vector<Bar>& history,
                const std::unordered_map<Symbol, double>&) {
                if (history.empty()) {
                    return std::vector<Signal>{};
                }
                return std::vector<Signal>{
                    Signal{history.back().timestamp, symbol, target_weight, target_weight}
                };
            });
            return self.run();
        }, py::arg("symbol"), py::arg("target_weight") = 1.0);
}
