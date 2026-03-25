#pragma once

#include <cstddef>
#include <memory>

#include "core/order_concept.hpp"

namespace aux::containers {

    /**
     * A doubly-linked intrusive queue optimised for orderbook price levels.
     *
     * Nodes are heap-allocated individually (except the two sentinel nodes, which
     * share a single two-element allocation) and are exposed to the caller as raw
     * pointers so that O(1) removal is possible without a secondary lookup.
     *
     * Assumptions & Constraints:
     *   - T must be completely nothrow constructible. This is to enforce the restriction that
     *     only non resource-owning types (POD types) are used within the container.
     *   - Copy construction and copy assignment are disabled.
     *   - A moved-from queue is left in a valid but empty (head_ptr_ == nullptr) state;
     *     the only safe operations on it afterwards are destruction and assignment.
     *   - Node pointers returned by enqueue() are invalidated by remove() on that node
     *     and by any move operation that transfers ownership of the queue.
     *   - Stateful allocators not supported. The allocator is rebound internally to 
     *     allocate Node objects and is reconstructed on each operation.
     */
    template <core::IsOrder T, typename Allocator = std::allocator<T>>
    class price_queue {
    public:
	struct node {
	    T data;
	    node* next;
	    node* prev;

	    node() : node(nullptr, nullptr, nullptr) {}
	    node(T data_, node* next_, node* prev_) : data(std::move(data_)), next(next_), prev(prev_) {}
	};

	using node_pointer_t = node*;
	using node_allocator_t = std::allocator_traits<Allocator>::template rebind_alloc<node>;

	class iterator {
	public:
	    using iterator_category = std::forward_iterator_tag;
	    using difference_type = std::ptrdiff_t;
	    using value_type = node;
	    using pointer = value_type*;
	    using reference = value_type&;

	    iterator(pointer ptr, size_t idx) noexcept : ptr_(ptr), idx_(idx) {}

	    iterator(const iterator& other) noexcept : ptr_(other.ptr_), idx_(other.idx_) {}

	    iterator(iterator&& other) noexcept : ptr_(other.ptr_), idx_(other.idx_) {}

	    iterator& operator=(const iterator& other) noexcept {
		if (this == &other) {
		    return *this;
		}

		ptr_ = other.ptr_;
		idx_ = other.idx_;
		return *this;
	    }

	    iterator& operator=(iterator&& other) noexcept {
		if (this == &other) {
		    return *this;
		}

		ptr_ = other.ptr_;
		idx_ = other.idx_;
		return *this;
	    }

	    reference operator*() const noexcept {
		return *ptr_;
	    }

	    pointer operator->() const noexcept {
		return ptr_;
	    }

	    iterator& operator++() noexcept {
		ptr_ = ptr_->next;
		return *this;
	    }

	    iterator operator++(int) noexcept {
		iterator tmp = *this;
		ptr_ = ptr_->next;
		return tmp;
	    }

	    bool operator==(const iterator& other) const noexcept {
		return ptr_ == other.ptr_;
	    }

	    bool operator!=(const iterator& other) const noexcept {
		return !(*this == other);
	    }

	private:
	    pointer ptr_;
	    size_t idx_;
	};


	/** Constructs an empty queue, allocating the two sentinel nodes (head / tail). */
	price_queue() noexcept {
	    node_allocator_t alloc{ Allocator{} };
	    head_ptr_ = static_cast<node_pointer_t>(alloc.allocate(2));
	    tail_ptr_ = head_ptr_ + 1;

	    std::construct_at(head_ptr_, T{}, tail_ptr_, nullptr);
	    std::construct_at(tail_ptr_, T{}, nullptr, head_ptr_);
	}

	/**
	 * Takes ownership of other's nodes and destroys all previously owned nodes. other
	 * is left in a moved-from (empty) state.
	 */
	price_queue(price_queue&& other) noexcept {
	    move_from(std::move(other));
	}

	/**
	 * Takes ownership of other's nodes and destroys all previously owned nodes. other
	 * is left in a moved-from (empty) state.
	 */
	price_queue& operator=(price_queue&& other) noexcept {
	    if (this == &other) {
		return *this;
	    }

	    move_from(std::move(other));
	    return *this;
	}

	/**
	 * Destroys all data nodes and the two sentinel nodes.
	 * A moved-from queue (head_ptr_ == nullptr) is handled safely with an early return.
	 */
	~price_queue() {
	    if (head_ptr_ == nullptr) return;

	    node_allocator_t alloc{ Allocator{}};
	    if (size_ > 0) {
		node_pointer_t cur = head_ptr_->next;
		node_pointer_t next = cur->next;

		for (size_t i{}; i < size_; ++i) {
		    std::destroy_at(cur);
		    alloc.deallocate(cur, 1);

		    cur = next;
		    next = next->next;
		}
	    }

	    std::destroy_at(head_ptr_);
	    std::destroy_at(tail_ptr_);
	    alloc.deallocate(head_ptr_, 2);
	}

	price_queue(const price_queue& other) = delete;
	price_queue& operator=(price_queue& other) = delete;

	/**
	 * Appends a new node at the back of the queue.
	 *
	 * @param the value to store; moved into the new node.
	 * @return A pointer to the newly allocated node. Remains valid until
	 *         remove() is called on it or the queue is moved/destroyed.
	 */
	node_pointer_t enqueue(T data) {
	    node_allocator_t alloc{ Allocator{} };
	    node_pointer_t prev_last_node = tail_ptr_->prev;

	    node_pointer_t node_ptr = alloc.allocate(1);
	    std::construct_at(node_ptr, std::move(data), tail_ptr_, prev_last_node);

	    tail_ptr_->prev = node_ptr;
	    prev_last_node->next = node_ptr;
	    ++size_;

	    return node_ptr;
	}

	/**
	 * Removes and deallocates an arbitrary node in O(1).
	 *
	 * @param node_ptr A pointer to a live node belonging to this queue.
	 *                 The pointer is invalid after this call.
	 */
	void remove(node_pointer_t node_ptr) noexcept {
	    node_allocator_t alloc{ Allocator{} };
	    node_pointer_t prev_node = node_ptr->prev;
	    node_pointer_t next_node = node_ptr->next;

	    std::destroy_at(node_ptr);
	    alloc.deallocate(node_ptr, 1);

	    prev_node->next = next_node;
	    next_node->prev = prev_node;
	    --size_;
	}

	/**
	 * Sets result to a reference to the front element, or a default-constructed T if empty.
	 * Does not remove the element.
	 */
	void front(T& result) const noexcept {
	    if (size_ == 0) [[unlikely]] {
		result = T{};
		return;
	    }
	    result = head_ptr_->next->data;
	}

	/** Returns the number of data nodes (sentinels not counted). */
	size_t size() const noexcept {
	    return size_;
	}

    private:
	node_pointer_t head_ptr_{ nullptr };
	node_pointer_t tail_ptr_{ nullptr };
	size_t size_{ 0 };

	// TODO: Construct allocator once and reuse.

	/**
	 * Core move helper shared by the move constructor and move assignment operator.
	 *
	 * Cleans up any existing data nodes and the sentinel pair (if this queue is
	 * non-empty or was previously constructed), then steals other's pointers.
	 * Leaves other in a moved-from state (all pointers null, size zero).
	 *
	 * Assumption: when called from the move constructor, head_ptr_ is nullptr so
	 * the sentinel cleanup block is skipped via the destructor's null guard pattern.
	 */
	void move_from(price_queue&& other) noexcept {
	    if (head_ptr_ != nullptr) {
		node_allocator_t alloc{ Allocator{}};
		if (size_ > 0) {
		    node_pointer_t cur = head_ptr_->next;
		    node_pointer_t next = cur->next;

		    for (size_t i{}; i < size_; ++i) {
			std::destroy_at(cur);
			alloc.deallocate(cur, 1);

			cur = next;
			next = next->next;
		    }
		}

		std::destroy_at(head_ptr_);
		std::destroy_at(tail_ptr_);
		alloc.deallocate(head_ptr_, 2);
	    }

	    head_ptr_ = other.head_ptr_;
	    tail_ptr_ = other.tail_ptr_;
	    size_ = other.size_;
	    other.head_ptr_ = nullptr;
	    other.tail_ptr_ = nullptr;
	    other.size_ = 0;
	}
    };
}
