#pragma once
#include "order_book/order_book.hpp"
#include "risk_engine/risk_engine.hpp"
#include <unordered_map>
#include <memory>
#include <chrono>
#include <queue>
#include <mutex>

namespace quantlab::execution_simulator {

enum class OrderStatus {
    Pending, PartiallyFilled, Filled, Cancelled, Rejected
};

struct SimOrder {
    order_book::Order  order;
    OrderStatus        status{OrderStatus::Pending};
    Price              avg_fill_price{0.0};
    Quantity           filled_qty{0.0};
    double             commission{0.0};
    Timestamp          submitted_at;
    Timestamp          filled_at;
};

struct SimConfig {
    double latency_ms{50.0};
    double commission_per_share{0.005};
    double commission_min{1.0};
    double slippage_bps{2.0};
    bool   simulate_partial_fills{true};
    double partial_fill_probability{0.2};
};

using FillCallback = std::function<void(const SimOrder&)>;

class ExecutionSimulator {
public:
    explicit ExecutionSimulator(SimConfig config = {});

    OrderId submit_order(order_book::Order order);
    void    cancel_order(OrderId id);
    void    process_tick(const Tick& tick);

    [[nodiscard]] const SimOrder* get_order(OrderId id) const;
    [[nodiscard]] std::vector<SimOrder> open_orders() const;

    void set_fill_callback(FillCallback cb);

private:
    SimConfig                                   config_;
    std::unordered_map<OrderId, SimOrder>        orders_;
    std::priority_queue<std::pair<Timestamp, OrderId>,
                        std::vector<std::pair<Timestamp, OrderId>>,
                        std::greater<>> pending_queue_;
    mutable std::mutex                          mutex_;
    FillCallback                                fill_cb_;
    std::atomic<OrderId>                        next_id_{1};

    void try_fill(SimOrder& sim_order, const Tick& tick);
    double compute_commission(Quantity qty, Price price) const;
    Price  apply_slippage(Side side, Price price) const;
};

} // namespace quantlab::execution_simulator
