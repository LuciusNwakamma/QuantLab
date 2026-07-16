"""QuantLab Streamlit dashboard (initial scaffold).

Run:
    streamlit run dashboard/streamlit_app.py
"""
from __future__ import annotations

import json
from pathlib import Path

import pandas as pd
import plotly.express as px
import streamlit as st

REPORT_DIR = Path("research/reports")
SUMMARY_PATH = REPORT_DIR / "summary.json"
RETURNS_PATH = REPORT_DIR / "portfolio_returns.csv"
EQUITY_PATH = REPORT_DIR / "equity_curve.csv"
WEIGHTS_PATH = REPORT_DIR / "weights.csv"

st.set_page_config(page_title="QuantLab Dashboard", layout="wide")
st.title("QuantLab Research Dashboard")

if not SUMMARY_PATH.exists():
    st.warning("No report found. Run the pipeline first to generate research/reports/summary.json")
    st.stop()

summary = json.loads(SUMMARY_PATH.read_text(encoding="utf-8"))
config = summary.get("config", {})
metrics = summary.get("metrics") or {}

col1, col2, col3, col4 = st.columns(4)
col1.metric("Provider", summary.get("provider", "N/A"))
col2.metric("Strategy", summary.get("strategy", "N/A"))
col3.metric("Symbols", ", ".join(config.get("symbols", [])))
col4.metric("Bars", str(sum(summary.get("num_bars", {}).values())))

st.subheader("Performance")
if metrics:
    m1, m2, m3, m4 = st.columns(4)
    m1.metric("Total Return", f"{metrics.get('total_return', 0.0):.2%}")
    m2.metric("Sharpe", f"{metrics.get('sharpe_ratio', 0.0):.3f}")
    m3.metric("Max Drawdown", f"{metrics.get('max_drawdown', 0.0):.2%}")
    m4.metric("Win Rate", f"{metrics.get('win_rate', 0.0):.2%}")

    st.json(metrics)
else:
    st.info("No metrics available in summary.")

left, right = st.columns(2)

with left:
    st.subheader("Equity Curve")
    if EQUITY_PATH.exists():
        eq = pd.read_csv(EQUITY_PATH)
        time_col = eq.columns[0]
        val_col = eq.columns[-1]
        eq[time_col] = pd.to_datetime(eq[time_col], errors="coerce")
        fig = px.line(eq, x=time_col, y=val_col, title="Equity")
        st.plotly_chart(fig, use_container_width=True)
    else:
        st.caption("No equity curve file found.")

with right:
    st.subheader("Returns Distribution")
    if RETURNS_PATH.exists():
        rets = pd.read_csv(RETURNS_PATH)
        val_col = rets.columns[-1]
        fig = px.histogram(rets, x=val_col, nbins=80, title="Return Histogram")
        st.plotly_chart(fig, use_container_width=True)
    else:
        st.caption("No returns file found.")

st.subheader("Portfolio Weights")
if WEIGHTS_PATH.exists():
    w = pd.read_csv(WEIGHTS_PATH)
    if not w.empty and len(w.columns) > 1:
        time_col = w.columns[0]
        w[time_col] = pd.to_datetime(w[time_col], errors="coerce")
        melted = w.melt(id_vars=[time_col], var_name="symbol", value_name="weight")
        fig = px.line(melted, x=time_col, y="weight", color="symbol", title="Weights Over Time")
        st.plotly_chart(fig, use_container_width=True)
    else:
        st.caption("Weights file is empty.")
else:
    st.caption("No weights file found.")

st.subheader("Run Configuration")
st.json(config)
