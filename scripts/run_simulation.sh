#!/bin/bash
# run_simulation.sh: Short simulation to verify Docker setup

echo "Building Docker images..."
docker compose build

echo "Starting simulation (1 Master + 10 Workers)..."
docker compose up -d --scale worker=10

echo "Waiting 60 seconds for simulation..."
sleep 60

echo "Checking Master logs..."
docker compose logs master | tail -n 20

echo "Checking Worker logs..."
docker compose logs worker | tail -n 20

echo "Verifying connectivity..."
MASTER_RECV=$(docker compose logs master | grep "Received" | tail -n 1)
if [ -n "$MASTER_RECV" ]; then
    echo "SUCCESS: Master received messages!"
else
    echo "FAILURE: Master did not receive messages."
    docker compose down
    exit 1
fi

echo "Simulation successful. Stopping containers..."
docker compose down
