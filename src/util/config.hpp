/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_CONFIG_HPP_INCLUDED
#define SL_CONFIG_HPP_INCLUDED

// Include platform configuration
#include <serverlink/config.h>

namespace slk {

// Compile-time settings using C++20 inline constexpr
// Number of new messages in message pipe needed to trigger new memory
// allocation. Setting this parameter to 256 decreases the impact of
// memory allocation by approximately 99.6%
inline constexpr int out_batch_size = 131072; inline constexpr int in_batch_size = 131072; inline constexpr int message_pipe_granularity = 256;

// Commands in pipe per allocation event.
inline constexpr int command_pipe_granularity = 16;

// Determines how often does socket poll for new commands when it
// still has unprocessed messages to handle. Thus, if it is set to 100,
// socket will process 100 inbound messages before doing the poll.
// If there are no unprocessed messages available, poll is done
// immediately. Decreasing the value trades overall latency for more
// real-time behaviour (less latency peaks).
inline constexpr int inbound_poll_rate = 100;

// Maximal delta between high and low watermark.
inline constexpr int max_wm_delta = 1024;

// Maximum number of events the I/O thread can process in one go.
inline constexpr int max_io_events = 256;

// Maximal delay to process command in API thread (in CPU ticks).
// 3,000,000 ticks equals to 1 - 2 milliseconds on current CPUs.
// Note that delay is only applied when there is continuous stream of
// messages to process. If not so, commands are processed immediately.
inline constexpr int max_command_delay = 3000000;

// Low-precision clock precision in CPU ticks. 1ms. Value of 1000000
// should be OK for CPU frequencies above 1GHz. If should work
// reasonably well for CPU frequencies above 500MHz. For lower CPU
// frequencies you may consider lowering this value to get best
// possible latencies.
inline constexpr int clock_precision = 1000000;

// On some OSes the signaler has to be emulated using a TCP
// connection. In such cases following port is used.
// If 0, it lets the OS choose a free port without requiring use of a
// global mutex.
inline constexpr int signaler_port = 0;

}  // namespace slk

#endif
