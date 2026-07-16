#pragma once
#include "order_book/order_book.hpp"
#include <unordered_map>
#include <memory>
#include <atomic>

namespace quantlab::matching_engine {

enum class MatchingAlgorithm { FIFO, ProRata };

struct ExecutionReport {
    OrderId   order_id;
    Symbol    symbol;
    Side      side;
    Price     avg_fill_price;
    Quantity  filled_qty;
    Quantity  remaining_qty;
    bool      fully_filled{false};
    Timestamp timestamp;
};

using ExecutionCallback = std::function<void(const ExecutionReport&)>;

class MatchingEngine {
public:
    explicit MatchingEngine(MatchingAlgorithm algo = MatchingAlgorithm::FIFO);

    void add_symbol(const Symbol& symbol);
    void remove_symbol(const Symbol& symbol);

    ExecutionReport submit_order(order_book::Order order);
    void            cancel_order(const Symbol& symbol, OrderId id);

    [[nodiscard]] const order_book::OrderBook* get_book(const Symbol& symbol) const;

    void set_execution_callback(ExecutionCallback cb);

    // Statistics
    [[nodiscard]] std::uint64_t total_trades() const;
    [[nodiscard]] std::uint64_t total_orders() const;

private:
    MatchingAlgorithm                                         algo_;
    std::unordered_map<Symbol, std::unique_ptr<order_book::OrderBook>> books_;
    ExecutionCallback                                         exec_cb_;
    std::atomic<std::uint64_t>                                trade_count_{0};
    std::atomic<std::uint64_t>                                order_count_{0};
};

} // namespace quantlab::matching_engine
