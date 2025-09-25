# Multi-stage build for Matching Engine
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    librdkafka-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code (excluding build directory)
COPY services/matching-engine/src ./src
COPY services/matching-engine/include ./include
COPY services/matching-engine/tests ./tests
COPY services/matching-engine/CMakeLists.txt .
COPY scripts/ ./scripts/

# Build the application
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    librdkafka1 \
    && rm -rf /var/lib/apt/lists/*

# Create app user
RUN useradd -r -s /bin/false appuser && \
    mkdir -p /app/results /app/logs && \
    chown -R appuser:appuser /app

WORKDIR /app

# Copy built executables
COPY --from=builder /app/build/matching_engine_consumer ./
COPY --from=builder /app/build/matching_engine_benchmark ./
COPY --from=builder /app/build/matching_engine_cli ./

# Copy scripts
COPY --from=builder /app/scripts/ ./scripts/

# Set permissions
RUN chmod +x matching_engine_consumer matching_engine_benchmark matching_engine_cli

USER appuser

EXPOSE 8080

# Default command
CMD ["./matching_engine_consumer"]