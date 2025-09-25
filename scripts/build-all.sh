#!/bin/bash

# Build all Quasar services
set -e

echo "Building all Quasar services..."
echo "==============================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Navigate to project root
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

print_status "Building from: $PROJECT_ROOT"

# Build Matching Engine
print_status "Building Matching Engine..."
cd services/matching-engine
mkdir -p build
cd build

if ! cmake ..; then
    print_error "Failed to configure Matching Engine"
    exit 1
fi

if ! make -j$(nproc 2>/dev/null || echo 4); then
    print_error "Failed to build Matching Engine"
    exit 1
fi

print_status "Matching Engine built successfully"

# Return to project root
cd "$PROJECT_ROOT"

# Build HFT Gateway
print_status "Building HFT Gateway..."
cd services/hft-gateway
mkdir -p build
cd build

if ! cmake ..; then
    print_error "Failed to configure HFT Gateway"
    exit 1
fi

if ! make -j$(nproc 2>/dev/null || echo 4); then
    print_error "Failed to build HFT Gateway"
    exit 1
fi

print_status "HFT Gateway built successfully"

# Return to project root
cd "$PROJECT_ROOT"

# Run tests
print_status "Running tests..."

# Test Matching Engine
cd services/matching-engine/build
if [ -f "./tests/core_tests" ]; then
    print_status "Running Matching Engine tests..."
    if ! ./tests/core_tests; then
        print_error "Matching Engine tests failed"
        exit 1
    fi
    print_status "Matching Engine tests passed"
fi

# Test HFT Gateway
cd "$PROJECT_ROOT/services/hft-gateway/build"
if [ -f "./tests/hft_gateway_tests" ]; then
    print_status "Running HFT Gateway tests..."
    if ! ./tests/hft_gateway_tests; then
        print_error "HFT Gateway tests failed"
        exit 1
    fi
    print_status "HFT Gateway tests passed"
fi

cd "$PROJECT_ROOT"

print_status "All services built and tested successfully!"

# Show build artifacts
echo
print_status "Build artifacts:"
echo "Matching Engine:"
ls -la services/matching-engine/build/matching_engine* 2>/dev/null || echo "  (no executables found)"
echo
echo "HFT Gateway:"
ls -la services/hft-gateway/build/hft_gateway* 2>/dev/null || echo "  (no executables found)"

echo
print_status "Ready to deploy with Docker Compose!"