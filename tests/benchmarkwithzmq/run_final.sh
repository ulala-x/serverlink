#!/bin/bash
set -e
BIN_DIR="./tests/benchmarkwithzmq/bin"
LIBZMQ_INC="/home/ulalax/project/ulalax/libzmq-native/deps/linux-x64/zeromq-4.3.5/include"
LIBZMQ_LIB="/home/ulalax/project/ulalax/libzmq-native/dist/linux-x64"

# Re-compile libzmq benchmarks with correct include path
for p in pair dealer_dealer dealer_router router_router pubsub; do
    g++ -O3 tests/benchmarkwithzmq/src/zmq_${p}.cpp -o $BIN_DIR/zmq_${p} -I$LIBZMQ_INC -L$LIBZMQ_LIB -lzmq -Wl,-rpath,$LIBZMQ_LIB -lpthread
done

sizes=(64 1024 65536)
patterns=("pair PAIR" "dealer_dealer D-D" "dealer_router D-R" "router_router R-R" "pubsub P-S")
get_port() { python3 -c 'import socket; s=socket.socket(); s.bind(("", 0)); print(s.getsockname()[1]); s.close()'; }

echo -e "\n==========================================================="
echo -e "  [FINAL REPORT 1] THROUGHPUT (Higher is better)"
echo -e "==========================================================="
printf "| %-15s | %-8s | %12s | %12s | %s |\n" "Pattern" "Size" "ServerLink" "libzmq" "Diff"
echo "|-----------------|----------|--------------|--------------|-------|"

for patt in "${patterns[@]}"; do
    read -r p name <<< "$patt"
    for s in "${sizes[@]}"; do
        port=$(get_port); addr="tcp://127.0.0.1:$port"
        slk=$(./$BIN_DIR/slk_$p "$addr" $s 10000 0 || echo "0")
        zmq=$(./$BIN_DIR/zmq_$p "$addr" $s 10000 0 || echo "0")
        diff=$(awk "BEGIN {if($zmq==0) print \"0.0\"; else printf \"%.1f\", ($slk - $zmq) / $zmq * 100}")
        printf "| %-15s | %8dB | %10.2fM | %10.2fM | %+6.1f%% |\n" "$name" "$s" $(bc -l <<< "$slk/1000000") $(bc -l <<< "$zmq/1000000") "$diff"
    done
    echo "|-----------------|----------|--------------|--------------|-------|"
done

echo -e "\n\n==========================================================="
echo -e "  [FINAL REPORT 2] LATENCY (RTT, Lower is better)"
echo -e "==========================================================="
printf "| %-15s | %-8s | %12s | %12s | %s |\n" "Pattern" "Size" "ServerLink" "libzmq" "Diff"
echo "|-----------------|----------|--------------|--------------|-------|"

for patt in "${patterns[@]}"; do
    read -r p name <<< "$patt"
    for s in "${sizes[@]}"; do
        port=$(get_port); addr="tcp://127.0.0.1:$port"
        slk=$(./$BIN_DIR/slk_$p "$addr" $s 500 1 || echo "0")
        zmq=$(./$BIN_DIR/zmq_$p "$addr" $s 500 1 || echo "0")
        # For latency, positive diff means SLK is faster (shorter latency)
        diff=$(awk "BEGIN {if($zmq==0) print \"0.0\"; else printf \"%.1f\", ($zmq - $slk) / $zmq * 100}")
        printf "| %-15s | %8dB | %10.2fus | %10.2fus | %+6.1f%% |\n" "$name" "$s" "$slk" "$zmq" "$diff"
    done
    echo "|-----------------|----------|--------------|--------------|-------|"
done
