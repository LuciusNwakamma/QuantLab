"""Live/paper trading via Alpaca."""
from __future__ import annotations

import os
from dataclasses import dataclass
from typing import Literal


@dataclass
class OrderRequest:
    symbol: str
    qty: float
    side: Literal["buy", "sell"]
    type: Literal["market", "limit"] = "market"
    limit_price: float | None = None
    time_in_force: str = "gtc"


class AlpacaPaperTrader:
    """Thin wrapper around alpaca-py for paper trading."""

    def __init__(
        self,
        api_key: str | None = None,
        secret_key: str | None = None,
        paper: bool = True,
    ):
        # Keys sourced from env variables; never hard-code credentials
        self._api_key    = api_key    or os.environ.get("ALPACA_API_KEY", "")
        self._secret_key = secret_key or os.environ.get("ALPACA_SECRET_KEY", "")
        self._paper      = paper
        self._client     = None  # initialized lazily

    def _get_client(self):
        if self._client is None:
            from alpaca.trading.client import TradingClient
            self._client = TradingClient(
                self._api_key, self._secret_key, paper=self._paper
            )
        return self._client

    def submit_order(self, req: OrderRequest):
        from alpaca.trading.requests import MarketOrderRequest, LimitOrderRequest
        from alpaca.trading.enums import OrderSide, TimeInForce

        side = OrderSide.BUY if req.side == "buy" else OrderSide.SELL
        tif  = TimeInForce.GTC

        if req.type == "market":
            order = MarketOrderRequest(
                symbol=req.symbol, qty=req.qty, side=side, time_in_force=tif
            )
        else:
            from alpaca.trading.requests import LimitOrderRequest
            order = LimitOrderRequest(
                symbol=req.symbol, qty=req.qty, side=side,
                time_in_force=tif, limit_price=req.limit_price
            )
        return self._get_client().submit_order(order)

    def get_positions(self):
        return self._get_client().get_all_positions()

    def get_account(self):
        return self._get_client().get_account()

    def cancel_all_orders(self):
        return self._get_client().cancel_orders()
