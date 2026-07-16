"""Machine learning module stubs."""
from __future__ import annotations

import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier, RandomForestRegressor
from sklearn.model_selection import TimeSeriesSplit
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import accuracy_score, mean_squared_error


class DirectionalClassifier:
    """Predicts next-bar direction (up/down) using Random Forest."""

    def __init__(self, n_estimators: int = 200, n_splits: int = 5):
        self.model = RandomForestClassifier(
            n_estimators=n_estimators, n_jobs=-1, random_state=42
        )
        self.scaler = StandardScaler()
        self.n_splits = n_splits

    def fit(self, X: pd.DataFrame, y: pd.Series) -> None:
        X_scaled = self.scaler.fit_transform(X)
        self.model.fit(X_scaled, y)

    def predict_proba(self, X: pd.DataFrame) -> np.ndarray:
        X_scaled = self.scaler.transform(X)
        return self.model.predict_proba(X_scaled)

    def walk_forward_eval(
        self, X: pd.DataFrame, y: pd.Series
    ) -> list[float]:
        tscv = TimeSeriesSplit(n_splits=self.n_splits)
        scores = []
        for train_idx, test_idx in tscv.split(X):
            self.fit(X.iloc[train_idx], y.iloc[train_idx])
            preds = self.model.predict(
                self.scaler.transform(X.iloc[test_idx])
            )
            scores.append(accuracy_score(y.iloc[test_idx], preds))
        return scores


class ReturnForecaster:
    """Predicts expected return using Random Forest Regressor."""

    def __init__(self, n_estimators: int = 200):
        self.model  = RandomForestRegressor(
            n_estimators=n_estimators, n_jobs=-1, random_state=42
        )
        self.scaler = StandardScaler()

    def fit(self, X: pd.DataFrame, y: pd.Series) -> None:
        X_scaled = self.scaler.fit_transform(X)
        self.model.fit(X_scaled, y)

    def predict(self, X: pd.DataFrame) -> np.ndarray:
        return self.model.predict(self.scaler.transform(X))

    def feature_importances(self, feature_names: list[str]) -> pd.Series:
        return pd.Series(
            self.model.feature_importances_, index=feature_names
        ).sort_values(ascending=False)
