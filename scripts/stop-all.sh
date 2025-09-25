#!/bin/bash

# Stop all Quasar services
set -e

echo "Stopping Quasar Trading System"
echo "==============================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Navigate to project root
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

# Stop and remove containers
if docker-compose ps | grep -q "Up"; then
    print_status "Stopping running services..."
    docker-compose down

    # Remove volumes if requested
    if [ "$1" = "--remove-data" ]; then
        print_warning "Removing data volumes..."
        docker-compose down -v
        docker volume prune -f
    fi
else
    print_status "No services are currently running"
fi

# Clean up dangling images if requested
if [ "$1" = "--cleanup" ] || [ "$2" = "--cleanup" ]; then
    print_status "Cleaning up Docker resources..."
    docker system prune -f
    docker image prune -f
fi

print_status "All services stopped"

# Show any remaining containers
if docker ps | grep quasar; then
    print_warning "Some Quasar containers are still running:"
    docker ps | grep quasar
else
    print_status "No Quasar containers are running"
fi