# Quasar Matching Engine Load Testing

This document describes the load testing capabilities for the Quasar matching engine, including comprehensive performance benchmarks and detailed metrics collection.

## Overview

The load testing framework provides two main tools:

1. **GoogleTest-based Load Tests** (`load_tests`) - Integration tests with assertions for CI/CD pipelines
2. **Standalone Benchmark Tool** (`matching_engine_benchmark`) - Flexible performance analysis tool

## Building the Load Tests

```bash
cd services/matching-engine
mkdir -p build && cd build
cmake ..
make load_tests matching_engine_benchmark
```

## Clean Rules

Multiple clean targets are available for different cleanup scenarios:

### Make Targets (from build directory)
```bash
cd build

# Clean build artifacts, executables + results (but not build dir itself)
make clean-all

# Clean only result files (keep executables)
make clean-results

# Clean only executables (keep results)
make clean-executables

# Standard clean (build artifacts only)
make clean
```

### Complete Cleanup Script (from source directory)
```bash
# Complete cleanup (removes entire build directory + results)
./scripts/clean.sh all

# Remove only build directory
./scripts/clean.sh build

# Remove only result files
./scripts/clean.sh results

# Remove only executables
./scripts/clean.sh executables
```

**Note**: `make clean-all` cannot remove the build directory itself since it runs from within it. Use `./scripts/clean.sh all` for complete cleanup including the build directory.

## Running GoogleTest Load Tests

The GoogleTest load tests are designed for automated testing and validation:

```bash
# Run all load tests
./tests/load_tests

# Run specific test
./tests/load_tests --gtest_filter="MatchingEngineLoadTest.LowVolumeMarketMaking"

# List available tests
./tests/load_tests --gtest_list_tests
```

### Available Test Scenarios

- **LowVolumeMarketMaking**: 1,000 orders at 100 orders/sec
- **HighVolumeMarketMaking**: 10,000 orders at 1,000 orders/sec
- **AggressiveTradingScenario**: 5,000 aggressive orders that cross the spread
- **MultiSymbolLoadTest**: 5,000 orders across multiple trading symbols
- **SustainedHighFrequencyTest**: 50,000 orders at 5,000 orders/sec

## Standalone Benchmark Tool

The standalone benchmark tool provides flexible performance analysis with multiple test suites:

### Basic Usage

```bash
# Quick benchmark (default)
./matching_engine_benchmark

# Full benchmark suite
./matching_engine_benchmark --full

# Extreme stress tests
./matching_engine_benchmark --extreme

# Custom test
./matching_engine_benchmark --custom 10000 1000

# CSV output for analysis
./matching_engine_benchmark --csv > results.csv
```

### Command Line Options

| Option | Description |
|--------|-------------|
| `--help` | Show help message |
| `--quick` | Run quick benchmark suite (3 tests) |
| `--full` | Run comprehensive benchmark suite (5 tests) |
| `--extreme` | Run extreme stress tests (3 intensive tests) |
| `--csv` | Output results in CSV format |
| `--custom N R` | Custom test with N orders at R orders/sec |
| `--symbol SYM` | Use trading symbol SYM (default: BTC-USD) |
| `--mid-price P` | Set mid price to P (default: 50000) |
| `--spread S` | Set spread to S (default: 10) |

### Benchmark Suites

#### Quick Suite (`--quick`)
- **Quick_LowVolume**: 1,000 orders at 100 orders/sec
- **Quick_MediumVolume**: 5,000 orders at 500 orders/sec
- **Quick_Aggressive**: 2,000 aggressive orders at 200 orders/sec

#### Full Suite (`--full`)
- **LowVolume_MarketMaking**: 1,000 orders at 100 orders/sec
- **MediumVolume_MarketMaking**: 5,000 orders at 500 orders/sec
- **HighVolume_MarketMaking**: 10,000 orders at 1,000 orders/sec
- **Aggressive_Trading**: 5,000 aggressive orders at 500 orders/sec
- **HighFrequency_Burst**: 20,000 orders at 2,000 orders/sec

#### Extreme Suite (`--extreme`)
- **Extreme_HighFrequency**: 50,000 orders at 5,000 orders/sec
- **Extreme_Aggressive**: 25,000 aggressive orders at 2,500 orders/sec
- **Extreme_Sustained**: 100,000 orders at 10,000 orders/sec

## Performance Metrics

Both tools collect comprehensive performance metrics:

### Latency Metrics
- **Min/Max**: Fastest and slowest order processing times
- **Average**: Mean processing time across all orders
- **Percentiles**: P50, P95, P99 latency distributions
- **Unit**: Microseconds (μs)

### Throughput Metrics
- **Orders/sec**: Actual order processing rate achieved
- **Trades/sec**: Number of trades generated per second
- **Duration**: Total test execution time

### Engine Statistics
- **Active Orders**: Orders remaining in the order book
- **Total Trades**: Number of trades executed
- **Cancelled Orders**: Number of successfully cancelled orders

## Test Scenarios Explained

### Market Making Tests
Generate random buy/sell orders around a mid-price with normal spread distribution. This simulates typical market making activity where orders are placed to provide liquidity.

### Aggressive Trading Tests
After warming up the order book with market making orders, submit aggressive orders that cross the spread to immediately trigger trades. This tests the matching engine's ability to handle high-frequency trading scenarios.

### Multi-Symbol Tests
Distribute orders across multiple trading symbols to test the engine's ability to handle concurrent order books without cross-contamination.

## Performance Expectations

Based on the current matching engine implementation:

### Typical Performance
- **Latency**: P95 < 100μs, P99 < 1ms under normal load
- **Throughput**: 1,000+ orders/sec sustained
- **Memory**: Efficient memory usage with priority queue implementation

### High Load Performance
- **Latency**: P99 < 10ms under extreme load (50k+ orders)
- **Throughput**: 3,000+ orders/sec burst capacity
- **Stability**: No memory leaks or performance degradation over time

## Interpreting Results

### Good Performance Indicators
- **Low Latency**: P95 < 500μs, P99 < 5ms
- **High Throughput**: Actual rate within 10% of target rate
- **Efficient Matching**: High trade rate in aggressive scenarios
- **Stable Memory**: Active orders count reasonable vs. total submitted

### Performance Issues
- **High Latency**: P99 > 50ms may indicate lock contention
- **Low Throughput**: Actual rate < 80% of target suggests bottlenecks
- **Memory Growth**: Active orders growing without bounds
- **Failed Assertions**: GoogleTest assertions failing in CI

## Automatic Result Saving

Both testing tools now automatically save results to timestamped CSV files in the source `results/` directory:

### GoogleTest Load Tests
- **Auto-save**: Each test automatically saves results as `results/TestName_YYYYMMDD_HHMMSS_mmm.csv`
- **Format**: Timestamped filenames with millisecond precision
- **Example**: `results/Low_Volume_Market_Making_20240919_143052_123.csv`

### Benchmark Tool
- **Auto-save**: Complete benchmark suites saved automatically (unless `--csv` used for stdout)
- **Format**: `results/benchmark_SUITE_YYYYMMDD_HHMMSS_mmm.csv`
- **Examples**:
  - `results/benchmark_quick_20240919_143052_456.csv`
  - `results/benchmark_full_20240919_143052_789.csv`
  - `results/benchmark_custom_20240919_143052_012.csv`

### Manual CSV Output
```bash
# Save to specific file (no auto-save)
cd build && ./matching_engine_benchmark --quick --csv > my_custom_results.csv

# Auto-save with console output (default behavior)
cd build && ./matching_engine_benchmark --full
# Creates: ../results/benchmark_full_YYYYMMDD_HHMMSS_mmm.csv

# GoogleTest with XML output for CI (run from source directory)
./build/tests/load_tests --gtest_output=xml:results/load_test_results.xml
```

## Integration with CI/CD

The GoogleTest load tests are designed for continuous integration:

```bash
# Run in CI pipeline with XML output
./tests/load_tests --gtest_output=xml:results/load_test_results.xml

# CSV results are automatically saved to results/ directory
ls results/*.csv

# Check exit code
if [ $? -eq 0 ]; then
    echo "All load tests passed"
    echo "Results saved in results/ directory"
else
    echo "Load tests failed - performance regression detected"
    exit 1
fi
```

## Troubleshooting

### Build Issues
- Ensure GoogleTest is properly fetched: `cmake .. && make`
- Check C++17 support: GCC 7+ or Clang 5+

### Performance Issues
- **High Latency**: Check system load, disable CPU scaling
- **Low Throughput**: Verify no debugging symbols, use Release build
- **Inconsistent Results**: Run multiple times, check system interference

### Memory Issues
- Monitor with `valgrind` or `AddressSanitizer`
- Check for proper order cleanup in matching engine

## Advanced Usage

### Custom Analysis
```bash
# Auto-saved results with console output
./matching_engine_benchmark --extreme
# Creates: results/benchmark_extreme_YYYYMMDD_HHMMSS_mmm.csv

# Manual CSV output (no auto-save)
./matching_engine_benchmark --extreme --csv > extreme_results.csv

# Custom high-frequency test with auto-save
./matching_engine_benchmark --custom 100000 20000 --symbol ETH-USD
# Creates: results/benchmark_custom_YYYYMMDD_HHMMSS_mmm.csv

# Multiple symbol analysis with manual naming
for symbol in BTC-USD ETH-USD ADA-USD; do
    ./matching_engine_benchmark --custom 10000 1000 --symbol $symbol --csv >> multi_symbol_results.csv
done

# View saved results
ls -la results/
cat results/benchmark_quick_*.csv
```

### Performance Profiling
Combine with profiling tools for deeper analysis:
```bash
# CPU profiling with perf
perf record -g ./matching_engine_benchmark --full
perf report

# Memory profiling with valgrind
valgrind --tool=callgrind ./matching_engine_benchmark --quick
```

## Future Enhancements

Planned improvements to the load testing framework:
- Support for order cancellation load tests
- WebSocket/TCP network layer testing
- Multi-threaded client simulation
- Historical market data replay
- Real-time performance monitoring dashboard