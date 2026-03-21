#pragma once

#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "containers/price_queue.hpp"
#include "core/order_concept.hpp"
#include "core/order_types.hpp"
#include "types.hpp"

using namespace aux::core;

namespace aux::containers {

    /**
     * Price-time priority order book for a single security.
     *
     * Maintains bid and ask sides as separate sorted maps of price level containers, where
     * each level holds container of resting orders. An auxilary hash map stores stable references
     * keyed by order ID, enabling efficient update and cancellation without scanning the book and
     * price level container.
     *
     * Assumptions / constraints:
     *   - T must satisfy the IsOrder concept (implement the defined getters/setters and must be POD with
     *     no throwing constructors or assignment operators).
     *   - Each order ID must be globally unique within the book. Any duplicate IDs in the scope of one
     *     orderbook will cause an error (throw an exception as of now but will be updated).
     *   - Timestamps are assigned interanally on add() and update() using the system clock.
     *   - update() re-enqueues the updated order to the price level container.
     *   - No copying/copy assigning orderbooks, only moving.
     *   - The default-constructed orderbook has no associated security. Prefer the security ID constructor.
     */
    template <IsOrder T, typename Allocator = std::allocator<T>>
    class order_book {
    public:
	/** Stable reference types for O(1) cancel/modify */
	/** TODO: change to stable iterator defined behind an interface to allow for different price level data structure impls */
	using node_t = price_queue<T, Allocator>::node;
	using node_ptr_t = price_queue<T, Allocator>::node_pointer_t;

	/** Value type aliases to shorten map-related allocator rebinding */
	using book_value_t = std::pair<const price_t, price_queue<T, Allocator>>;
	using map_value_t = std::pair<const id_t, node_ptr_t>;

	/** Allocators rebound to the concrete pair types stored internally in maps */
	using book_alloc_t = typename std::allocator_traits<Allocator>::template rebind_alloc<book_value_t>;
	using map_alloc_t = typename std::allocator_traits<Allocator>::template rebind_alloc<map_value_t>;

	/** Final composite types */
	using book_t = std::map<price_t, price_queue<T, Allocator>, std::less<price_t>, book_alloc_t>;
	using order_map_t = std::unordered_map<id_t, node_ptr_t, std::hash<id_t>, std::equal_to<id_t>, map_alloc_t>;
	using flat_book_array_t = std::array<book_t, 2>;

	/** Default constructor leaves security_id zero-initialized */
	order_book() = default;

	/** Constructs an empty book associated to the given security ID */
	order_book(security_id_t id) : security_id_(id) {
	    std::construct_at(std::addressof(books_[0]), book_t{});
	    std::construct_at(std::addressof(books_[1]), book_t{});
	}

	/** Steals security ID, books, and orders from the other book. Leaved the moved
	 from book in a valid but empty state */
	order_book(order_book&& other) noexcept {
	    move_from(other);
	}

	/** Steals security ID, books, and orders from the other book. Leaved the moved
	 from book in a valid but empty state */
	order_book& operator=(order_book&& other) noexcept {
	    if (this == &other) {
		return *this;
	    }

	    move_from(other);
	    return *this;
	}

	order_book(const order_book& other) = delete;
	order_book& operator=(const order_book& other) = delete;

	/**
	 * Inserts a new order into the book.
	 *
	 * Stamps the order with the current system time (ns since epoch),
	 * places it at the back of its price level container, and records
	 * a stable reference in the books order map
	 *
	 * @throws std::invalid_argument if an order with the same ID already
	 * 	   exists in the book.
	 */
	void add(T order) {
	    if (order_map_.contains(order.id())) {
		throw std::invalid_argument("Cannot add previously existing order, id: " + std::to_string(order.id()));
	    }

	    auto& book = books_[side_to_uint8(order.side())];
	    auto [price_level_it, _] = book.try_emplace(order.price(), price_queue<T, Allocator>{});

	    set_timestamp(order);

	    const node_ptr_t order_ptr = price_level_it->second.enqueue(std::move(order));
	    order_map_.emplace(order.id(), order_ptr);
	}

	/**
	 * Removes an existing order from the book.
	 *
	 * The reference to the order is invalidated and removed from the
	 * order map. Erases the price level if no orders at that price remain.
	 *
	 * @throws std::invalid_argument if the ID does not reference an added order.
	 */
	void cancel(id_t id) {
	    auto order_map_it = order_map_.find(id);
	    if (order_map_it == order_map_.end()) {
		throw std::invalid_argument("Cannot cancel order that has not been added, id: " + std::to_string(id));
	    }

	    node_ptr_t order_ptr = order_map_it->second;
	    T& order = order_ptr->data;
	    auto& book = books_[side_to_uint8(order.side())];

	    auto price_level_it = book.find(order.price());
	    price_queue<T, Allocator>& pq = price_level_it->second;
	    pq.remove(order_ptr);
	    if (pq.size() == 0) { 
		book.erase(price_level_it);
	    }

	    order_map_.erase(order_map_it);
	}

	/**
	 * Updates the priority and quantity of an existing order.
	 *
	 * The order is re-inserted to the price level container, losing its original
	 * time priority in price-time priority based containers.
	 *
	 * @throws std::invalid_argument if the ID does not reference an added order.
	 */
	void update(id_t id, id_t priority, quantity_t qty) {
	    auto order_map_it = order_map_.find(id);
	    if (order_map_it == order_map_.end()) {
		throw std::invalid_argument("Cannot update order that has not been added, id: " + std::to_string(id));
	    }

	    node_ptr_t order_ptr = order_map_it->second;
	    T order = std::move(order_ptr->data);
	    order.set_quantity(qty);
	    order.set_priority(priority);
	    set_timestamp(order);

	    auto& book = books_[side_to_uint8(order.side())];
	    auto price_level_it = book.find(order.price());
	    auto& pq = price_level_it->second;

	    // TODO: Write a move_to_back() method that does not deallocate or allocate new object
	    pq.remove(order_ptr);
	    order_map_it->second = pq.enqueue(std::move(order));
	}

	security_id_t security_id() const noexcept {
	    return security_id_;
	}

    private:
	// TODO: Remove branching by using constexpr map and binding book side implementations to it

	/** Specific security this book is tracking; FK back to global security index to retrieve metadata */
	security_id_t security_id_{};

	/** Order ID to internally-stored stable reference map for O(1) cancelation/modification */
	order_map_t order_map_{};

	/**
	 * Flat two-element array holding bid (index 0) to ask (index 1) sides. Use `side_to_uint8()` to
	 * convert a Side enum value to the correct index.
	 */
	flat_book_array_t books_{};

	/**
	 * Move helper function used by both move construction and move assignment. Destroys the original
	 * state and reconstructs based on the state of `other`. `other` is left in an empty but valid state.
	 */
	void move_from(order_book&& other) {
	    security_id_ = other.security_id_;

	    std::destroy_at(std::addressof(books_[side_to_uint8(Side::Bid)]));
	    std::destroy_at(std::addressof(books_[side_to_uint8(Side::Ask)]));

	    std::construct_at(std::addressof(books_[side_to_uint8(Side::Bid)]), std::move(other.books_[side_to_uint8(Side::Bid)]));
	    std::construct_at(std::addressof(books_[side_to_uint8(Side::Ask)]), std::move(other.books_[side_to_uint8(Side::Ask)]));
	    order_map_ = std::move(other.order_map_);
	}

	void set_timestamp(T& order) {
	    const auto now = std::chrono::system_clock::now();
	    const uint64_t ns_since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
	    order.set_timestamp(ns_since_epoch);
	}

    protected:
	/** Accessors expost for test subclasses only. These bypass encapsulation and should
	 not be part of any public API. */

	static const flat_book_array_t& get_books(const order_book<T, Allocator>& other) noexcept {
	    return other.books_;
	}

	static const order_map_t& get_order_map(const order_book<T, Allocator>& other) noexcept {
	    return other.order_map_;
	}
    };
}

