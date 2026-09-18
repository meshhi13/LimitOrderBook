# Limit Order Book & Matching Engine

A high-performance limit order book (LOB) in C++20, simulating real-time
financial exchange matching with strict price-time priority.

## Design

- **Price levels** (`bids_` / `asks_`) are `std::map<Price, PriceLevel>`,
  keyed so the best bid/ask is always `begin()` — highest price first for
  bids, lowest first for asks.
- Each `PriceLevel` holds a `std::list<Order>` in FIFO arrival order, giving
  O(1) append and O(1) cancel-by-iterator, with strict price-time priority
  within a level.
- An `unordered_map<OrderId, OrderLocation>` maps every live order directly
  to its list iterator and price, giving average O(1) lookup and cancel
  instead of scanning price levels.
- Matching walks price levels from the top of book outward while the
  incoming order's price crosses the resting price, filling FIFO within each
  level and erasing exhausted orders/levels as it goes.

## Layout

```
include/lob/       Order.hpp, OrderBook.hpp, MatchingEngine.hpp
src/OrderBook.cpp   Matching engine implementation
tests/              GoogleTest unit tests
benchmark/          Throughput simulator
```

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

## Benchmark

`lob_benchmark [num_orders]` simulates a busy book: ~90% resting limit
orders near a moving mid price, ~5% cancels, ~5% sweeping market orders.

```sh
./build/lob_benchmark 5000000
```

## API sketch

```cpp
lob::MatchingEngine engine;
lob::OrderId id = engine.submit_limit_order(lob::Side::Buy, /*price=*/10050, /*qty=*/100);
engine.cancel_order(id);

for (const auto& trade : engine.trades()) { /* ... */ }
```
