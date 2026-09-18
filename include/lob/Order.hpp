#pragma once

#include <cstdint>

namespace lob {

using OrderId = std::uint64_t;
using Price = std::int64_t;      // price in integer ticks
using Quantity = std::uint64_t;
using Timestamp = std::uint64_t; // monotonic sequence number, used for FIFO priority

enum class Side : std::uint8_t { Buy, Sell };
enum class OrderType : std::uint8_t { Limit, Market };

inline Side opposite(Side side) {
    return side == Side::Buy ? Side::Sell : Side::Buy;
}

struct Order {
    OrderId id;
    Side side;
    Price price;      // unused (0) for market orders
    Quantity quantity; // remaining, live quantity
    Timestamp timestamp;
};

struct Trade {
    OrderId maker_id;
    OrderId taker_id;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
};

} // namespace lob
