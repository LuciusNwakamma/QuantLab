"""End-to-end research pipeline.

Flow:
1) Fetch market data
2) Compute features
3) Generate strategy signals over time
4) Run a lightweight portfolio backtest
5) Compute performance metrics and persist outputs
"""
from __future__ import annotations

from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
import argparse
import json

import pandas as pd

from python.feature_engineering import add_all_features
from python.market_data import AlpacaProvider, PolygonProvider, YFinanceProvider
from python.performance_analytics import compute_metrics, print_tearsheet
from python.strategy_research.strategies import (
    MeanReversionStrategy,
    MomentumStrategy,
    PairsTradingStrategy,
)


@dataclass
class PipelineConfig:
    provider: str = "yfinance"
    symbols: tuple[str, ...] = ("SPY", "QQQ", "IWM")
    start: str = "2020-01-01"
    end: str = "2024-12-31"
    resolution: str = "1d"
    strategy: str = "mean_reversion"
    warmup_bars: int = 80
    max_leverage: float = 1.0
    transaction_cost_bps: float = 1.0
    output_dir: str = "research/reports"


def get_provider(provider_name: str):
    provider_name = provider_name.lower()
    if provider_name == "yfinance":
        if YFinanceProvider is None:
            raise RuntimeError("YFinanceProvider unavailable; install yfinance.")
        return YFinanceProvider()
    if provider_name == "alpaca":
        if AlpacaProvider is None:
            raise RuntimeError("AlpacaProvider unavailable; install alpaca-py.")
        return AlpacaProvider()
    if provider_name == "polygon":
        if PolygonProvider is None:
            raise RuntimeError("PolygonProvider unavailable; install polygon-api-client.")
        return PolygonProvider()
    raise ValueError(f"Unsupported provider: {provider_name}")


def get_strategy(strategy_name: str, symbols: tuple[str, ...]):
    strategy_name = strategy_name.lower()
    if strategy_name == "mean_reversion":
        return MeanReversionStrategy(window=20, z_entry=2.0, z_exit=0.5)
    if strategy_name == "momentum":
        return MomentumStrategy(lookback=252, hold=21, skip=21)
    if strategy_name == "pairs":
        if len(symbols) < 2:
            raise ValueError("Pairs strategy requires at least 2 symbols.")
        return PairsTradingStrategy(symbol_a=symbols[0], symbol_b=symbols[1], window=60)
    raise ValueError(f"Unsupported strategy: {strategy_name}")


def _prepare_close_matrix(data: dict[str, pd.DataFrame]) -> pd.DataFrame:
    close = {}
    for sym, df in data.items():
        if df.empty or "close" not in df.columns:
            continue
        s = df["close"].copy()
        s.index = pd.to_datetime(s.index, utc=True)
        close[sym] = s
    if not close:
        return pd.DataFrame()
    close_df = pd.DataFrame(close).sort_index().dropna(how="all")
    return close_df


def run_signal_backtest(
    data: dict[str, pd.DataFrame],
    strategy,
    warmup_bars: int = 80,
    max_leverage: float = 1.0,
    transaction_cost_bps: float = 1.0,
) -> tuple[pd.Series, pd.Series, pd.DataFrame]:
    """Run a simple signal-based backtest and return (returns, equity, weights)."""
    close = _prepare_close_matrix(data)
    if close.empty or len(close) < warmup_bars + 2:
        return pd.Series(dtype=float), pd.Series(dtype=float), pd.DataFrame()

    asset_returns = close.pct_change().fillna(0.0)
    symbols = list(close.columns)

    portfolio_returns: list[float] = []
    portfolio_index: list[pd.Timestamp] = []
    weights_history: list[dict[str, float]] = []

    current_weights = {s: 0.0 for s in symbols}

    dates = list(close.index)
    for i in range(warmup_bars, len(dates) - 1):
        t = dates[i]
        t_next = dates[i + 1]

        # Slice data up to t for strategy inference
        sliced = {}
        for sym in symbols:
            df_sym = data[sym].copy()
            df_sym.index = pd.to_datetime(df_sym.index, utc=True)
            sliced[sym] = df_sym.loc[:t]

        signals = strategy.generate_signals(sliced)
        proposed = {s: 0.0 for s in symbols}
        for sig in signals:
            if sig.symbol in proposed:
                proposed[sig.symbol] = float(sig.target_weight)

        gross = sum(abs(v) for v in proposed.values())
        if gross > max_leverage and gross > 0:
            scale = max_leverage / gross
            proposed = {k: v * scale for k, v in proposed.items()}

        turnover = sum(abs(proposed[s] - current_weights[s]) for s in symbols)
        trade_cost = turnover * (transaction_cost_bps / 10000.0)

        next_ret = sum(proposed[s] * float(asset_returns.loc[t_next, s]) for s in symbols)
        pnl = next_ret - trade_cost

        portfolio_returns.append(pnl)
        portfolio_index.append(t_next)
        current_weights = proposed
        weights_history.append({"timestamp": t_next.isoformat(), **current_weights})

    returns = pd.Series(portfolio_returns, index=pd.Index(portfolio_index, name="timestamp"), name="returns")
    equity = (1.0 + returns).cumprod()
    weights_df = pd.DataFrame(weights_history)
    if not weights_df.empty:
        weights_df.set_index("timestamp", inplace=True)
    return returns, equity, weights_df


def run_research_pipeline(config: PipelineConfig) -> dict:
    provider = get_provider(config.provider)
    strategy = get_strategy(config.strategy, config.symbols)

    start_dt = datetime.fromisoformat(config.start)
    end_dt = datetime.fromisoformat(config.end)

    raw_data = provider.fetch_multiple(
        list(config.symbols),
        start=start_dt,
        end=end_dt,
        resolution=config.resolution,
    )

    featured_data = {}
    for sym, df in raw_data.items():
        featured_data[sym] = add_all_features(df) if not df.empty else df

    returns, equity, weights = run_signal_backtest(
        featured_data,
        strategy,
        warmup_bars=config.warmup_bars,
        max_leverage=config.max_leverage,
        transaction_cost_bps=config.transaction_cost_bps,
    )

    metrics = compute_metrics(returns) if not returns.empty else None

    out_dir = Path(config.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if not returns.empty:
        returns.to_csv(out_dir / "portfolio_returns.csv", header=True)
        equity.to_csv(out_dir / "equity_curve.csv", header=True)
    if not weights.empty:
        weights.to_csv(out_dir / "weights.csv")

    payload = {
        "config": asdict(config),
        "provider": provider.name,
        "strategy": config.strategy,
        "num_bars": {k: int(len(v)) for k, v in raw_data.items()},
        "metrics": metrics._asdict() if metrics is not None else None,
    }

    with (out_dir / "summary.json").open("w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)

    if metrics is not None:
        print_tearsheet(metrics)

    return payload


def _parse_args() -> PipelineConfig:
    parser = argparse.ArgumentParser(description="Run QuantLab research pipeline.")
    parser.add_argument("--provider", default="yfinance", choices=["yfinance", "alpaca", "polygon"])
    parser.add_argument("--symbols", default="SPY,QQQ,IWM")
    parser.add_argument("--start", default="2020-01-01")
    parser.add_argument("--end", default="2024-12-31")
    parser.add_argument("--resolution", default="1d")
    parser.add_argument("--strategy", default="mean_reversion", choices=["mean_reversion", "momentum", "pairs"])
    parser.add_argument("--warmup", type=int, default=80)
    parser.add_argument("--max-leverage", type=float, default=1.0)
    parser.add_argument("--cost-bps", type=float, default=1.0)
    parser.add_argument("--output-dir", default="research/reports")
    args = parser.parse_args()

    return PipelineConfig(
        provider=args.provider,
        symbols=tuple(s.strip().upper() for s in args.symbols.split(",") if s.strip()),
        start=args.start,
        end=args.end,
        resolution=args.resolution,
        strategy=args.strategy,
        warmup_bars=args.warmup,
        max_leverage=args.max_leverage,
        transaction_cost_bps=args.cost_bps,
        output_dir=args.output_dir,
    )


def main() -> None:
    config = _parse_args()
    run_research_pipeline(config)


if __name__ == "__main__":
    main()
