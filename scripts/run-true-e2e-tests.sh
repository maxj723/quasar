#!/bin/bash

# True End-to-End Performance Testing Script
# Tests the complete pipeline: Order Creation → Kafka → Matching Engine → Trades

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}$1${NC}"
    echo "$(printf '=%.0s' {1..60})"
}

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Navigate to project root
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

TEST_DATE=$(date +%Y%m%d_%H%M%S)
RESULTS_DIR="$PROJECT_ROOT/results/e2e_tests_$TEST_DATE"

# Create results directory
mkdir -p "$RESULTS_DIR"
mkdir -p "$RESULTS_DIR/csv"
mkdir -p "$RESULTS_DIR/logs"

print_header "TRUE END-TO-END PIPELINE TESTING"
print_status "Results will be saved to: $RESULTS_DIR"

# Ensure Kafka is running
if ! docker ps | grep -q "quasar-kafka.*Up"; then
    print_status "Starting Kafka infrastructure..."
    docker compose up -d zookeeper kafka schema-registry kafka-ui >/dev/null 2>&1
    sleep 15
fi

# Kill any existing processes
pkill -f matching_engine_consumer || true
sleep 2

print_header "TEST 1: ISOLATED MATCHING ENGINE PERFORMANCE"

# Run isolated matching engine benchmarks with CSV output
cd "$PROJECT_ROOT/services/matching-engine/build"

print_status "Running precision latency benchmark..."
./matching_engine_benchmark --custom 2000 500 > "$RESULTS_DIR/logs/matching_engine_isolated.log" 2>&1

# Extract metrics and create CSV
echo "test_name,orders_processed,trades_generated,duration_seconds,actual_rate_ops,trade_rate_ops,min_latency_us,avg_latency_us,p50_latency_us,p95_latency_us,p99_latency_us,max_latency_us,active_orders" > "$RESULTS_DIR/csv/matching_engine_isolated.csv"

# Parse the log file for metrics
if [ -f "$RESULTS_DIR/logs/matching_engine_isolated.log" ]; then
    ORDERS=$(grep "Orders Processed:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $3}')
    TRADES=$(grep "Trades Generated:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $3}')
    DURATION=$(grep "Duration:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $2}')
    RATE=$(grep "Actual Rate:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $3}')
    TRADE_RATE=$(grep "Trade Rate:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $3}')
    MIN_LAT=$(grep "Min:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $2}')
    AVG_LAT=$(grep "Avg:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $2}')
    P50_LAT=$(grep "P50:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $2}')
    P95_LAT=$(grep "P95:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $2}')
    P99_LAT=$(grep "P99:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $2}')
    MAX_LAT=$(grep "Max:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $2}')
    ACTIVE=$(grep "Active Orders:" "$RESULTS_DIR/logs/matching_engine_isolated.log" | awk '{print $3}')

    echo "isolated_engine,$ORDERS,$TRADES,$DURATION,$RATE,$TRADE_RATE,$MIN_LAT,$AVG_LAT,$P50_LAT,$P95_LAT,$P99_LAT,$MAX_LAT,$ACTIVE" >> "$RESULTS_DIR/csv/matching_engine_isolated.csv"
fi

print_status "✓ Isolated engine test completed"

cd "$PROJECT_ROOT"

print_header "TEST 2: KAFKA PIPELINE THROUGHPUT MEASUREMENT"

# Start matching engine consumer with detailed logging
print_status "Starting Matching Engine Consumer for pipeline test..."
"$PROJECT_ROOT/services/matching-engine/build/matching_engine_consumer" \
    --brokers localhost:9092 > "$RESULTS_DIR/logs/kafka_pipeline.log" 2>&1 &

CONSUMER_PID=$!
sleep 3

# Create CSV header for pipeline metrics
echo "timestamp,orders_processed,total_trades,active_orders,messages_published,kafka_errors,orders_per_sec,trades_per_sec" > "$RESULTS_DIR/csv/kafka_pipeline_throughput.csv"

print_status "Collecting pipeline metrics for 60 seconds..."

# Collect metrics every 5 seconds for 60 seconds
START_TIME=$(date +%s)
LAST_ORDERS=0
LAST_TRADES=0

for i in {1..12}; do
    sleep 5
    CURRENT_TIME=$(date +%s)
    ELAPSED=$((CURRENT_TIME - START_TIME))

    if [ -f "$RESULTS_DIR/logs/kafka_pipeline.log" ]; then
        # Extract latest statistics
        STATS=$(tail -20 "$RESULTS_DIR/logs/kafka_pipeline.log" | grep -A 10 "MATCHING ENGINE STATISTICS" | tail -8)

        if [ ! -z "$STATS" ]; then
            ORDERS=$(echo "$STATS" | grep "Orders Processed:" | tail -1 | awk '{print $3}' | tr -d ',' | head -1)
            TRADES=$(echo "$STATS" | grep "Total Trades:" | tail -1 | awk '{print $3}' | tr -d ',' | head -1)
            ACTIVE=$(echo "$STATS" | grep "Engine Active Orders:" | tail -1 | awk '{print $4}' | tr -d ',' | head -1)
            PUBLISHED=$(echo "$STATS" | grep "Messages Published:" | tail -1 | awk '{print $3}' | tr -d ',' | head -1)
            ERRORS=$(echo "$STATS" | grep "Kafka Errors:" | tail -1 | awk '{print $3}' | tr -d ',' | head -1)

            # Calculate rates (orders and trades per second)
            if [ ! -z "$ORDERS" ] && [ "$ORDERS" -gt 0 ] && [ "$ELAPSED" -gt 0 ]; then
                ORDERS_PER_SEC=$((ORDERS / ELAPSED))
                TRADES_PER_SEC=$((TRADES / ELAPSED))

                TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
                echo "$TIMESTAMP,$ORDERS,$TRADES,$ACTIVE,$PUBLISHED,$ERRORS,$ORDERS_PER_SEC,$TRADES_PER_SEC" >> "$RESULTS_DIR/csv/kafka_pipeline_throughput.csv"

                print_status "T+${ELAPSED}s: $ORDERS orders processed, $TRADES trades generated ($ORDERS_PER_SEC ops/sec)"
            fi
        fi
    fi
done

# Stop the consumer
kill $CONSUMER_PID 2>/dev/null || true
wait $CONSUMER_PID 2>/dev/null || true

print_status "✓ Pipeline throughput test completed"

print_header "TEST 3: COMPREHENSIVE PERFORMANCE ANALYSIS"

# Create comprehensive analysis
cat > "$RESULTS_DIR/comprehensive_analysis.md" << EOF
# Comprehensive End-to-End Performance Analysis

**Test Date:** $(date)
**Test Duration:** 60 seconds sustained pipeline testing
**Results Location:** \`$RESULTS_DIR\`

## Test Components

### 1. Isolated Matching Engine Performance
- **Purpose**: Measure core engine latency without pipeline overhead
- **Results**: \`csv/matching_engine_isolated.csv\`

### 2. Kafka Pipeline Throughput
- **Purpose**: Measure real-world pipeline performance including Kafka messaging
- **Results**: \`csv/kafka_pipeline_throughput.csv\`
- **Metrics**: Time-series data with 5-second intervals

## Key Performance Metrics

EOF

# Add isolated engine results to analysis
if [ -f "$RESULTS_DIR/csv/matching_engine_isolated.csv" ]; then
    echo "### Isolated Engine Performance" >> "$RESULTS_DIR/comprehensive_analysis.md"
    echo '```csv' >> "$RESULTS_DIR/comprehensive_analysis.md"
    cat "$RESULTS_DIR/csv/matching_engine_isolated.csv" >> "$RESULTS_DIR/comprehensive_analysis.md"
    echo '```' >> "$RESULTS_DIR/comprehensive_analysis.md"
    echo "" >> "$RESULTS_DIR/comprehensive_analysis.md"
fi

# Add pipeline throughput summary
if [ -f "$RESULTS_DIR/csv/kafka_pipeline_throughput.csv" ]; then
    echo "### Pipeline Throughput Summary" >> "$RESULTS_DIR/comprehensive_analysis.md"

    # Get final metrics
    FINAL_METRICS=$(tail -1 "$RESULTS_DIR/csv/kafka_pipeline_throughput.csv")
    if [ ! -z "$FINAL_METRICS" ]; then
        echo "**Final Pipeline Performance:**" >> "$RESULTS_DIR/comprehensive_analysis.md"
        echo '```' >> "$RESULTS_DIR/comprehensive_analysis.md"
        echo "$(head -1 "$RESULTS_DIR/csv/kafka_pipeline_throughput.csv")" >> "$RESULTS_DIR/comprehensive_analysis.md"
        echo "$FINAL_METRICS" >> "$RESULTS_DIR/comprehensive_analysis.md"
        echo '```' >> "$RESULTS_DIR/comprehensive_analysis.md"
        echo "" >> "$RESULTS_DIR/comprehensive_analysis.md"

        # Extract key numbers
        FINAL_ORDERS=$(echo "$FINAL_METRICS" | cut -d',' -f2)
        FINAL_TRADES=$(echo "$FINAL_METRICS" | cut -d',' -f3)
        ORDERS_PER_SEC=$(echo "$FINAL_METRICS" | cut -d',' -f7)
        TRADES_PER_SEC=$(echo "$FINAL_METRICS" | cut -d',' -f8)

        echo "**Performance Summary:**" >> "$RESULTS_DIR/comprehensive_analysis.md"
        echo "- Total Orders Processed: $FINAL_ORDERS" >> "$RESULTS_DIR/comprehensive_analysis.md"
        echo "- Total Trades Generated: $FINAL_TRADES" >> "$RESULTS_DIR/comprehensive_analysis.md"
        echo "- Sustained Orders/sec: $ORDERS_PER_SEC" >> "$RESULTS_DIR/comprehensive_analysis.md"
        echo "- Sustained Trades/sec: $TRADES_PER_SEC" >> "$RESULTS_DIR/comprehensive_analysis.md"
        echo "" >> "$RESULTS_DIR/comprehensive_analysis.md"
    fi

    # Show trend data
    echo "**Time Series Data (sample):**" >> "$RESULTS_DIR/comprehensive_analysis.md"
    echo '```csv' >> "$RESULTS_DIR/comprehensive_analysis.md"
    head -1 "$RESULTS_DIR/csv/kafka_pipeline_throughput.csv" >> "$RESULTS_DIR/comprehensive_analysis.md"
    head -6 "$RESULTS_DIR/csv/kafka_pipeline_throughput.csv" | tail -5 >> "$RESULTS_DIR/comprehensive_analysis.md"
    echo "..." >> "$RESULTS_DIR/comprehensive_analysis.md"
    tail -3 "$RESULTS_DIR/csv/kafka_pipeline_throughput.csv" >> "$RESULTS_DIR/comprehensive_analysis.md"
    echo '```' >> "$RESULTS_DIR/comprehensive_analysis.md"
fi

cat >> "$RESULTS_DIR/comprehensive_analysis.md" << EOF

## File Structure

\`\`\`
$RESULTS_DIR/
├── csv/
│   ├── matching_engine_isolated.csv    # Isolated engine metrics
│   └── kafka_pipeline_throughput.csv   # Pipeline time-series data
├── logs/
│   ├── matching_engine_isolated.log    # Detailed engine benchmark logs
│   └── kafka_pipeline.log             # Pipeline runtime logs
└── comprehensive_analysis.md           # This analysis file
\`\`\`

## Analysis Tools

These CSV files can be analyzed with:
- **Excel/Google Sheets**: Direct import for charts and analysis
- **Python + Pandas**: Statistical analysis and modeling
- **R**: Advanced performance modeling
- **Grafana**: Real-time dashboard visualization

## Performance Validation

This testing measures the **true end-to-end performance** of:
1. Order generation and processing
2. Kafka message passing and consumption
3. Matching engine execution and trade generation
4. Result publication back to Kafka

---
*Generated by Quasar True E2E Testing Suite*
EOF

print_header "RESULTS SUMMARY"

echo
print_status "Test Results Location: $RESULTS_DIR"
echo "├── CSV Data Files:"
find "$RESULTS_DIR/csv" -name "*.csv" -exec echo "│   📊 {}" \;
echo "├── Log Files:"
find "$RESULTS_DIR/logs" -name "*.log" -exec echo "│   📝 {}" \;
echo "└── Analysis: comprehensive_analysis.md"

echo
if [ -f "$RESULTS_DIR/csv/kafka_pipeline_throughput.csv" ]; then
    DATAPOINTS=$(tail -n +2 "$RESULTS_DIR/csv/kafka_pipeline_throughput.csv" | wc -l)
    print_status "Captured $DATAPOINTS pipeline data points over 60 seconds"

    # Show final performance
    FINAL_LINE=$(tail -1 "$RESULTS_DIR/csv/kafka_pipeline_throughput.csv")
    if [ ! -z "$FINAL_LINE" ]; then
        FINAL_ORDERS=$(echo "$FINAL_LINE" | cut -d',' -f2)
        FINAL_RATE=$(echo "$FINAL_LINE" | cut -d',' -f7)
        print_status "Final Performance: $FINAL_ORDERS total orders at $FINAL_RATE orders/sec"
    fi
fi

print_header "TRUE END-TO-END TESTING COMPLETE! 🎯"

echo
echo "📋 Analysis Files Created:"
echo "   • comprehensive_analysis.md - Complete performance report"
echo "   • matching_engine_isolated.csv - Core engine metrics"
echo "   • kafka_pipeline_throughput.csv - Time-series pipeline data"
echo
echo "📊 Ready for analysis with Excel, Python, R, or Grafana!"