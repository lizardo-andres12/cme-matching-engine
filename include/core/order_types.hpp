#pragma once

#include <cstdint>
#include <utility>

#include "types.hpp"

namespace aux::core {
    
    enum class Side : uint8_t {
	Bid = 0,
	Ask = 1
    };

    /** Returns the numeric value of the side enumeration for book array indexing */
    static constexpr uint8_t side_to_uint8(core::Side side) {
	return static_cast<uint8_t>(side);
    }

    struct order {
	id_t order_id_; // Tag 37
	id_t priority_id_; // Tag 5979
	price_t price_; // Price mantissa, multiplied by instrument_metadata.price_exponent;
	quantity_t quantity_; // Tag 38
	Side side_;
	uint64_t timestamp_; // Internally set

	id_t id() const noexcept {
	    return order_id_;
	}

	id_t priority() const noexcept {
	    return priority_id_;
	}

	void set_priority(id_t priority) noexcept {
	    priority_id_ = priority;
	}

	Side side() const noexcept {
	    return side_;
	}

	price_t price() const noexcept {
	    return price_;
	}

	quantity_t quantity() const noexcept {
	    return quantity_;
	}

	void set_quantity(quantity_t qty) noexcept {
	    quantity_ = qty;
	}

	uint64_t timestamp() const noexcept {
	    return timestamp_;
	}

	void set_timestamp(uint64_t ts) noexcept {
	    timestamp_ = ts;
	}

	quantity_t fill(quantity_t fill_qty) noexcept {
	    quantity_t filled = fill_qty > quantity_ ? quantity_ : fill_qty;
	    quantity_ -= filled;
	    return filled;
	}

	bool operator==(const order& other) const noexcept {
	    return order_id_ == other.order_id_
		&& priority_id_ == other.priority_id_
		&& price_ == other.price_
		&& quantity_ == other.quantity_
		&& side_ == other.side_;
	}

	bool operator!=(const order& other) const noexcept {
	    return !(*this == other);
	}
    };

    struct instrument_metadata {
	price_t min_price_increment; // Tag 969
	quantity_t lot_size; // Tag 1231
	security_id_t security_id; // Tag 48
	price_exponent_t price_exponent;
	char symbol[20];

	double strike_price; // Tag 202
	uint8_t put_call; // Tag 201 (put=0, call=1)
    };


    /** Helper class to track reference count and memory leaks. This 
      class is an IsOrder compliant type. */
    struct stub_order {
	static constinit inline int active_instances = 0;
	order order_inst;

	stub_order() noexcept : order_inst{} { ++active_instances; }
	stub_order(order&& order) noexcept : order_inst(order) { ++active_instances; }

	stub_order(const stub_order& other) noexcept : order_inst(other.order_inst) { ++active_instances; }

	stub_order& operator=(const stub_order& other) noexcept {
	    if (this == &other) {
		return *this;
	    }

	    order_inst = other.order_inst;
	    return *this;
	}

	stub_order(stub_order&& other) noexcept : order_inst(std::move(other.order_inst)) { ++active_instances; }

	stub_order& operator=(stub_order&& other) noexcept {
	    if (this == &other) {
		return *this;
	    }

	    order_inst = std::move(other.order_inst);
	    return *this;
	}

	~stub_order() { --active_instances; }

	id_t id() const noexcept {
	    return order_inst.id();
	}

	id_t priority() const noexcept {
	    return order_inst.priority();
	}

	void set_priority(id_t priority) noexcept {
	    order_inst.set_priority(priority);
	}

	Side side() const noexcept {
	    return order_inst.side();
	}

	price_t price() const noexcept {
	    return order_inst.price();
	}

	quantity_t quantity() const noexcept {
	    return order_inst.quantity();
	}

	void set_quantity(quantity_t qty) noexcept {
	    order_inst.set_quantity(qty);
	}

	uint64_t timestamp() const noexcept {
	    return order_inst.timestamp();
	}

	void set_timestamp(uint64_t ts) noexcept {
	    order_inst.set_timestamp(ts);
	}

	quantity_t fill(quantity_t amount) noexcept {
	    return order_inst.fill(amount);
	}

	bool operator==(const stub_order& other) const noexcept {
	    return order_inst == other.order_inst;
	}

	bool operator!=(const stub_order& other) const noexcept {
	    return order_inst != other.order_inst;
	}
    };
}

