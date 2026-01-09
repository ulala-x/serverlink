/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_CONFIG_HPP_INCLUDED
#define SL_CONFIG_HPP_INCLUDED

#include <cstddef>

namespace slk {

// libzmq Parity: High-performance defaults
inline constexpr int out_batch_size = 131072; // 128KB
inline constexpr int in_batch_size = 131072;  // 128KB
inline constexpr int message_pipe_granularity = 256;
inline constexpr int command_pipe_granularity = 16;

// VSM limits
inline constexpr size_t max_vsm_size = 41;

// Clock precision
inline constexpr int clock_precision = 1000000;

} // namespace slk

#endif