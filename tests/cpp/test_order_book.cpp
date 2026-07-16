#include <gtest/gtest.h>
#include "order_book/order_book.hpp"

using namespace quantlab;
using namespace quantlab::order_book;

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book{"AAPL"};
    Timestamp now{std::chrono::system_clock::now()};

    Order make_order(OrderId id, Side side, Price price, Quantity qty,
                     OrderType type = OrderType::Limit) {
        return Order{id, "AAPL", side, type, price, qty, 0.0,
                     TimeInForce::GTC, now};
    }
};

TEST_F(OrderBookTest, EmptyBookHasNoBestBidAsk) {
    EXPECT_DOUBLE_EQ(book.best_bid(), 0.0);
    EXPECT_DOUBLE_EQ(book.best_ask(), 0.0);
}

TEST_F(OrderBookTest, AddBidUpdatesBookState) {
    book.add_order(make_order(1, Side::Buy, 100.0, 10.0));
    EXPECT_DOUBLE_EQ(book.best_bid(), 100.0);
    EXPECT_DOUBLE_EQ(book.bid_depth(1), 10.0);
}

TEST_F(OrderBookTest, AddAskUpdatesBookState) {
    book.add_order(make_order(1, Side::Sell, 101.0, 5.0));
    EXPECT_DOUBLE_EQ(book.best_ask(), 101.0);
}

TEST_F(OrderBookTest, SpreadIsCorrect) {
    book.add_order(make_order(1, Side::Buy,  100.0, 10.0));
    book.add_order(make_order(2, Side::Sell, 101.0,  5.0));
    EXPECT_DOUBLE_EQ(book.spread(), 1.0);
    EXPECT_DOUBLE_EQ(book.mid_price(), 100.5);
}

TEST_F(OrderBookTest, MarketOrderMatchesAgainstBook) {
    book.add_order(make_order(1, Side::Sell, 100.0, 10.0));

    std::vector<Trade> trades;
    book.set_trade_callback([&](const Trade& t){ trades.push_back(t); });

    Order buy = make_order(2, Side::Buy, 0.0, 5.0, OrderType::Market);
    book.add_order(buy);

    EXPECT_EQ(trades.size(), 1u);
    EXPECT_DOUBLE_EQ(trades[0].quantity, 5.0);
    EXPECT_DOUBLE_EQ(book.ask_depth(1), 5.0); // 5 remaining
}

TEST_F(OrderBookTest, FullMatchClearsLevel) {
    book.add_order(make_order(1, Side::Sell, 100.0, 10.0));
    Order buy = make_order(2, Side::Buy, 100.0, 10.0);
    book.add_order(buy);
    EXPECT_DOUBLE_EQ(book.best_ask(), 0.0);
}

TEST_F(OrderBookTest, CancelOrderReducesDepth) {
    book.add_order(make_order(1, Side::Buy, 100.0, 10.0));
    book.cancel_order(1);
    EXPECT_DOUBLE_EQ(book.bid_depth(1), 0.0);
}

TEST_F(OrderBookTest, OrderImbalanceIsCorrect) {
    book.add_order(make_order(1, Side::Buy,  100.0, 80.0));
    book.add_order(make_order(2, Side::Sell, 101.0, 20.0));
    // (80 - 20) / (80 + 20) = 0.6
    EXPECT_NEAR(book.order_imbalance(), 0.6, 1e-9);
}
