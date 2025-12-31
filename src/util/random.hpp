/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_RANDOM_HPP_INCLUDED
#define SL_RANDOM_HPP_INCLUDED

#include <cstdint>

namespace slk {

// Seeds the random number generator.
void seed_random();

// Generates random value.
uint32_t generate_random();

// [De-]Initialise random subsystem (no-op in ServerLink, no crypto).
void random_open();
void random_close();

}  // namespace slk

#endif
