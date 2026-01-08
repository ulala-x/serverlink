# Dockerfile for ServerLink Stability Testing
FROM ubuntu:24.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    iproute2 \
    procps \
    lsof \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build ServerLink (Release mode with ASIO)
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DSL_USE_ASIO=ON
RUN cmake --build build --parallel $(nproc)

# Install built binaries to a standard path
RUN cd build && make install

# Add scripts directory to path
ENV PATH="/app/build/tests/benchmark:/app/build/tests/router:/app/build/examples:${PATH}"

# Default command
CMD ["bash"]
