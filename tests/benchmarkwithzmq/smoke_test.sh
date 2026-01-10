#!/bin/bash
BIN_DIR="./tests/benchmarkwithzmq/bin"
patterns=("pair" "dealer_dealer" "dealer_router" "router_router" "pubsub")
transports=("inproc" "ipc" "tcp")

get_port() { python3 -c 'import socket; s=socket.socket(); s.bind(("", 0)); print(s.getsockname()[1]); s.close()'; }

echo "=== Individual Transport Smoke Test (100 msgs) ==="
printf "%-15s | %-8s | %s\n" "Pattern" "Trans" "Status"
echo "-----------------------------------------------"

for p in "${patterns[@]}"; do
    for t in "${transports[@]}"; do
        port=$(get_port)
        addr="$t://127.0.0.1:$port"
        if [ "$t" == "ipc" ]; then addr="ipc://smoke_$p.ipc"; rm -f "smoke_$p.ipc"; fi
        if [ "$t" == "inproc" ]; then addr="inproc://smoke_$p"; fi
        
        # Run ServerLink version with short timeout
        if timeout 2 ./$BIN_DIR/slk_$p "$addr" 64 100 0 > /dev/null 2>&1; then
            status="OK"
        else
            status="FAILED/HANG"
        fi
        printf "%-15s | %-8s | %s\n" "$p" "$t" "$status"
    done
    echo "-----------------------------------------------"
done
