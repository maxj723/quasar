# Multi-stage build for HFT Gateway
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    librdkafka-dev \
    libspdlog-dev \
    libflatbuffers-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code (excluding build directory)
COPY services/hft-gateway/src ./src
COPY services/hft-gateway/include ./include
COPY services/hft-gateway/tests ./tests
COPY services/hft-gateway/CMakeLists.txt .

# Build the application
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    librdkafka1 \
    libspdlog1 \
    && rm -rf /var/lib/apt/lists/*

# Create app user
RUN useradd -r -s /bin/false appuser && \
    mkdir -p /app/logs && \
    chown -R appuser:appuser /app

WORKDIR /app

# Copy built executable
COPY --from=builder /app/build/hft_gateway ./

# Set permissions
RUN chmod +x hft_gateway

USER appuser

EXPOSE 31337

# Default command
CMD ["./hft_gateway"]