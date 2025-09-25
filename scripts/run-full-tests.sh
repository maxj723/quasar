#!/bin/bash

# Comprehensive Quasar Testing Script
# Runs unit tests, builds services, starts infrastructure, and runs end-to-end + load tests

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}$1${NC}"
    echo "$(printf '=%.0s' {1..50})"
}

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Navigate to project root
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

print_header "QUASAR COMPREHENSIVE TEST SUITE"
print_status "Project root: $PROJECT_ROOT"

# Parse command line arguments
RUN_UNIT_TESTS=true
RUN_LOAD_TESTS=true
RUN_E2E_TESTS=true
KEEP_SERVICES_RUNNING=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-unit-tests)
            RUN_UNIT_TESTS=false
            shift
            ;;
        --skip-load-tests)
            RUN_LOAD_TESTS=false
            shift
            ;;
        --skip-e2e-tests)
            RUN_E2E_TESTS=false
            shift
            ;;
        --keep-running)
            KEEP_SERVICES_RUNNING=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --skip-unit-tests    Skip unit tests"
            echo "  --skip-load-tests    Skip load tests"
            echo "  --skip-e2e-tests     Skip end-to-end tests"
            echo "  --keep-running       Keep services running after tests"
            echo "  --verbose            Verbose output"
            echo "  --help               Show this help"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Function to cleanup on exit
cleanup() {
    if [ "$KEEP_SERVICES_RUNNING" = false ]; then
        print_status "Cleaning up services..."
        ./scripts/stop-all.sh >/dev/null 2>&1 || true
    fi
}

trap cleanup EXIT

# Step 1: Unit Tests
if [ "$RUN_UNIT_TESTS" = true ]; then
    print_header "STEP 1: UNIT TESTS"

    print_status "Building and running Matching Engine unit tests..."
    cd services/matching-engine/build
    if [ "$VERBOSE" = true ]; then
        make && ./tests/core_tests
    else
        make >/dev/null 2>&1 && ./tests/core_tests
    fi
    print_status "✓ Matching Engine unit tests passed"

    cd "$PROJECT_ROOT"

    print_status "Building and running HFT Gateway unit tests..."
    cd services/hft-gateway/build
    if [ "$VERBOSE" = true ]; then
        make && ./tests/hft_gateway_tests
    else
        make >/dev/null 2>&1 && ./tests/hft_gateway_tests
    fi
    print_status "✓ HFT Gateway unit tests passed"

    cd "$PROJECT_ROOT"
fi

# Step 2: Build All Services
print_header "STEP 2: BUILD SERVICES"
print_status "Building all services..."

if [ "$VERBOSE" = true ]; then
    ./scripts/build-all.sh
else
    ./scripts/build-all.sh >/dev/null 2>&1
fi

print_status "✓ All services built successfully"

# Step 3: Start Services
print_header "STEP 3: START SERVICES"
print_status "Starting Quasar services..."

if [ "$VERBOSE" = true ]; then
    ./scripts/start-local.sh
else
    ./scripts/start-local.sh >/dev/null 2>&1
fi

print_status "Waiting for services to be ready..."
sleep 15

# Verify services are running
if ! docker ps | grep -q "quasar-hft-gateway.*Up"; then
    print_error "HFT Gateway is not running"
    exit 1
fi

if ! docker ps | grep -q "quasar-matching-engine.*Up"; then
    print_error "Matching Engine is not running"
    exit 1
fi

print_status "✓ Services are running and ready"

# Step 4: End-to-End Tests
if [ "$RUN_E2E_TESTS" = true ]; then
    print_header "STEP 4: END-TO-END TESTS"

    print_status "Building end-to-end tests..."
    cd tests/end-to-end
    mkdir -p build
    cd build

    if [ "$VERBOSE" = true ]; then
        cmake .. && make
    else
        cmake .. >/dev/null 2>&1 && make >/dev/null 2>&1
    fi

    print_status "Running end-to-end tests..."
    if [ "$VERBOSE" = true ]; then
        ./e2e_tests
    else
        ./e2e_tests >/dev/null 2>&1
    fi

    print_status "✓ End-to-end tests passed"
    cd "$PROJECT_ROOT"
fi

# Step 5: Load Tests
if [ "$RUN_LOAD_TESTS" = true ]; then
    print_header "STEP 5: LOAD TESTS"

    print_status "Building load tests..."
    cd tests/load
    mkdir -p build
    cd build

    if [ "$VERBOSE" = true ]; then
        cmake .. && make
    else
        cmake .. >/dev/null 2>&1 && make >/dev/null 2>&1
    fi

    print_status "Running quick load test..."
    ./pipeline_load_test \
        --host localhost \
        --port 31337 \
        --orders 1000 \
        --clients 5 \
        --rate 500 \
        --output quick_load_test_results.csv

    print_status "Running medium load test..."
    ./pipeline_load_test \
        --host localhost \
        --port 31337 \
        --orders 5000 \
        --clients 10 \
        --rate 1000 \
        --output medium_load_test_results.csv

    print_status "Running high-volume load test..."
    ./pipeline_load_test \
        --host localhost \
        --port 31337 \
        --orders 10000 \
        --clients 20 \
        --rate 2000 \
        --output high_volume_load_test_results.csv

    print_status "✓ Load tests completed"
    cd "$PROJECT_ROOT"

    # Copy results to main results directory
    mkdir -p results/load_tests
    cp tests/load/build/*.csv results/load_tests/ 2>/dev/null || true
fi

# Step 6: Generate Report
print_header "STEP 6: TEST REPORT"

echo "Test execution completed successfully!"
echo
echo "Results summary:"
echo "├── Unit Tests: $([ "$RUN_UNIT_TESTS" = true ] && echo "✓ PASSED" || echo "SKIPPED")"
echo "├── Service Build: ✓ PASSED"
echo "├── Service Startup: ✓ PASSED"
echo "├── End-to-End Tests: $([ "$RUN_E2E_TESTS" = true ] && echo "✓ PASSED" || echo "SKIPPED")"
echo "└── Load Tests: $([ "$RUN_LOAD_TESTS" = true ] && echo "✓ PASSED" || echo "SKIPPED")"

if [ -d "results" ]; then
    echo
    echo "Generated results:"
    find results -name "*.csv" -exec echo "  📊 {}" \;
fi

echo
echo "Services status:"
docker ps --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}" | grep quasar || echo "No services running"

if [ "$KEEP_SERVICES_RUNNING" = true ]; then
    echo
    print_status "Services are kept running for manual testing"
    print_status "Access points:"
    echo "  - Kafka UI: http://localhost:8080"
    echo "  - HFT Gateway: localhost:31337"
    echo "  - Schema Registry: http://localhost:8081"
    echo
    print_status "To stop services later, run: ./scripts/stop-all.sh"
else
    echo
    print_status "Services will be stopped automatically"
fi

print_header "COMPREHENSIVE TESTING COMPLETE! ✨"