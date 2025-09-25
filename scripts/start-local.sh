#!/bin/bash

# Quasar Local Development Startup Script
set -e

echo "Starting Quasar Trading System Locally"
echo "====================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    print_error "Docker is not running. Please start Docker Desktop."
    exit 1
fi

# Check if Docker Compose is available
if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
    print_error "Docker Compose is not available. Please install Docker Compose."
    exit 1
fi

# Navigate to project root
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

print_status "Project root: $PROJECT_ROOT"

# Create necessary directories
print_status "Creating directories..."
mkdir -p logs results monitoring/grafana/dashboards monitoring/grafana/datasources

# Build services first
print_status "Building services..."
./scripts/build-all.sh

# Check if services are already running
if docker-compose ps | grep -q "Up"; then
    print_warning "Some services are already running. Stopping them first..."
    docker-compose down
fi

# Start infrastructure services (Kafka, Zookeeper, etc.)
print_status "Starting infrastructure services..."
docker-compose up -d zookeeper kafka schema-registry kafka-ui redis

# Wait for Kafka to be ready
print_status "Waiting for Kafka to be ready..."
sleep 30

# Check Kafka health
MAX_RETRIES=30
RETRY_COUNT=0
while [ $RETRY_COUNT -lt $MAX_RETRIES ]; do
    if docker exec quasar-kafka kafka-broker-api-versions --bootstrap-server localhost:9092 > /dev/null 2>&1; then
        print_status "Kafka is ready!"
        break
    fi

    RETRY_COUNT=$((RETRY_COUNT + 1))
    print_warning "Waiting for Kafka... (attempt $RETRY_COUNT/$MAX_RETRIES)"
    sleep 2
done

if [ $RETRY_COUNT -eq $MAX_RETRIES ]; then
    print_error "Kafka failed to start within timeout"
    exit 1
fi

# Create Kafka topics
print_status "Creating Kafka topics..."
docker exec quasar-kafka kafka-topics --create --bootstrap-server localhost:9092 --topic orders.new --partitions 4 --replication-factor 1 --if-not-exists
docker exec quasar-kafka kafka-topics --create --bootstrap-server localhost:9092 --topic trades --partitions 4 --replication-factor 1 --if-not-exists
docker exec quasar-kafka kafka-topics --create --bootstrap-server localhost:9092 --topic market_data --partitions 4 --replication-factor 1 --if-not-exists

# List topics to verify
print_status "Available Kafka topics:"
docker exec quasar-kafka kafka-topics --list --bootstrap-server localhost:9092

# Start application services
print_status "Starting application services..."
docker-compose up -d matching-engine hft-gateway

# Optional: Start monitoring services
if [ "$1" = "--with-monitoring" ]; then
    print_status "Starting monitoring services..."
    docker-compose up -d prometheus grafana
fi

# Show status
print_status "Checking service status..."
docker-compose ps

echo
print_status "Services are starting up. Access points:"
echo "  - Kafka UI: http://localhost:8080"
echo "  - HFT Gateway: localhost:31337"
echo "  - Schema Registry: http://localhost:8081"
echo "  - Redis: localhost:6379"

if [ "$1" = "--with-monitoring" ]; then
    echo "  - Prometheus: http://localhost:9090"
    echo "  - Grafana: http://localhost:3000 (admin/admin)"
fi

echo
print_status "To view logs:"
echo "  docker-compose logs -f matching-engine"
echo "  docker-compose logs -f hft-gateway"

echo
print_status "To stop all services:"
echo "  docker-compose down"

echo
print_status "Quasar Trading System is now running!"

# Optional: tail logs
if [ "$2" = "--follow-logs" ]; then
    print_status "Following logs (Ctrl+C to stop)..."
    docker-compose logs -f
fi