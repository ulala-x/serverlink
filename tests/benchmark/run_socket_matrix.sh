#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SLK_BIN="${SCRIPT_DIR}/../../build-asio/tests/benchmark/bench_throughput"
ZMQ_BIN="${SCRIPT_DIR}/../../build-asio/tests/benchmark/bench_zmq_matrix"

echo "Starting Performance Matrix..."
printf "| %-15s | %-12s | %12s | %10s |\n" "Pattern" "ServerLink" "libzmq" "Diff"
printf "|-----------------|--------------|--------------|------------|\n"

# R-R (TCP)
s=$("$SLK_BIN" tcp://127.0.0.1:6660 64 10000 0)
z=$("$ZMQ_BIN" tcp 64 10000 0)
d=$(awk "BEGIN {printf \"%.1f\", ($s - $z) / $z * 100}")
printf "| %-15s | %10.2fM/s | %10.2fM/s | %+9.1f%% |\n" "ROUTER-ROUTER" $(bc -l <<< "$s/1000000") $(bc -l <<< "$z/1000000") "$d"

# D-R (TCP)
s=$("$SLK_BIN" tcp://127.0.0.1:6661 64 10000 1)
z=$("$ZMQ_BIN" tcp 64 10000 1)
d=$(awk "BEGIN {printf \"%.1f\", ($s - $z) / $z * 100}")
printf "| %-15s | %10.2fM/s | %10.2fM/s | %+9.1f%% |\n" "DEALER-ROUTER" $(bc -l <<< "$s/1000000") $(bc -l <<< "$z/1000000") "$d"

# D-D (TCP)
s=$("$SLK_BIN" tcp://127.0.0.1:6662 64 10000 2)
z=$("$ZMQ_BIN" tcp 64 10000 2)
d=$(awk "BEGIN {printf \"%.1f\", ($s - $z) / $z * 100}")
printf "| %-15s | %10.2fM/s | %10.2fM/s | %+9.1f%% |\n" "DEALER-DEALER" $(bc -l <<< "$s/1000000") $(bc -l <<< "$z/1000000") "$d"
