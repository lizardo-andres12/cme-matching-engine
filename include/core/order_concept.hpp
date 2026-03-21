#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

#include "core/order_types.hpp"
#include "types.hpp"

namespace aux::core {

    /** Order types must have all noexcept construction, meaning they must not allocate or
     own any special resources. */
    template <typename T>
    concept IsOrder = requires(T order, uint64_t ts) {
	
	/** Must have some ID retreiving mechanism */
	{ order.id() } -> std::same_as<aux::id_t>;

	/** Must have a side associated with it */
	{ order.side() } -> std::same_as<aux::core::Side>;

	/** Must have the price mantissa stored with it */
	{ order.price() } -> std::same_as<aux::price_t>;

	/** Must be timestamp-able for internal latency tracking */
	{ order.set_timestamp(ts) } -> std::same_as<void>;

    } && std::is_nothrow_default_constructible_v<T>
    && std::is_nothrow_copy_constructible_v<T>
    && std::is_nothrow_copy_assignable_v<T>
    && std::is_nothrow_move_constructible_v<T>
    && std::is_nothrow_move_assignable_v<T>;
};
