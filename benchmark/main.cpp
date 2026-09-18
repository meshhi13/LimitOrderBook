// Simulates high-frequency order flow against the matching engine and
// reports throughput, to demonstrate the book holds up under HFT-style load.
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

#include "lob/MatchingEngine.hpp"

using namespace lob;
using Clock = std::chrono::steady_clock;

namespace {

struct BenchmarkResult {
    std::size_t order_count;
    double elapsed_ms;
    double orders_per_sec;
    std::size_t trades_generated;
    std::size_t resting_orders;
};

// Generates a mix of limit orders (90%) and cancels (10%) around a moving
// mid price, and market orders occasionally, to approximate a busy book.
BenchmarkResult run_benchmark(std::size_t num_orders, unsigned seed) {
    MatchingEngine engine;
    std::mt19937_64 rng(seed);

    constexpr Price mid_start = 100'000; // $100.00 in ticks of $0.01
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> action_dist(0, 99);
    std::uniform_int_distribution<Price> price_offset_dist(-50, 50);
    std::uniform_int_distribution<Quantity> qty_dist(1, 500);

    std::vector<OrderId> live_orders;
    live_orders.reserve(num_orders);

    const auto start = Clock::now();

    for (std::size_t i = 0; i < num_orders; ++i) {
        const Side side = side_dist(rng) == 0 ? Side::Buy : Side::Sell;
        const int action = action_dist(rng);

        if (action < 5 && !live_orders.empty()) {
            // ~5%: cancel a random live order.
            std::uniform_int_distribution<std::size_t> pick(0, live_orders.size() - 1);
            const std::size_t idx = pick(rng);
            engine.cancel_order(live_orders[idx]);
            live_orders[idx] = live_orders.back();
            live_orders.pop_back();
        } else if (action < 10) {
            // ~5%: aggressive market order that sweeps the book.
            engine.submit_market_order(side, qty_dist(rng));
        } else {
            // ~90%: resting limit order near the mid price.
            const Price price = mid_start + price_offset_dist(rng);
            const OrderId id = engine.submit_limit_order(side, price, qty_dist(rng));
            if (engine.book().contains(id)) {
                live_orders.push_back(id);
            }
        }
    }

    const auto end = Clock::now();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return BenchmarkResult{
        .order_count = num_orders,
        .elapsed_ms = elapsed_ms,
        .orders_per_sec = static_cast<double>(num_orders) / (elapsed_ms / 1000.0),
        .trades_generated = engine.trades().size(),
        .resting_orders = engine.book().order_count(),
    };
}

} // namespace

int main(int argc, char** argv) {
    std::size_t num_orders = 5'000'000;
    if (argc > 1) {
        num_orders = static_cast<std::size_t>(std::stoull(argv[1]));
    }

    const BenchmarkResult result = run_benchmark(num_orders, /*seed=*/42);

    std::printf("Orders processed : %zu\n", result.order_count);
    std::printf("Elapsed time      : %.2f ms\n", result.elapsed_ms);
    std::printf("Throughput        : %.0f orders/sec\n", result.orders_per_sec);
    std::printf("Trades generated  : %zu\n", result.trades_generated);
    std::printf("Resting orders    : %zu\n", result.resting_orders);
    return 0;
}
