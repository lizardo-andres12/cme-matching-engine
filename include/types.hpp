#pragma once

#include <cstdint>

namespace aux {

    /** Universal application ID type (order and priority ID types) */
    using id_t = uint64_t;

    /** Security ID field */
    using security_id_t = uint32_t;

    /** Price mantissa type */
    using price_t = int64_t;

    /** Price exponent type */
    using price_exponent_t = int32_t;

    /** Quantity type */
    using quantity_t = uint32_t;

}
