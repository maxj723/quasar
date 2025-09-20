#!/bin/bash
# Complete cleanup script for Quasar Matching Engine

set -e

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "🧹 Quasar Matching Engine - Complete Cleanup"
echo "============================================="

# Function to safely remove directory
safe_remove_dir() {
    if [ -d "$1" ]; then
        echo "Removing: $1"
        rm -rf "$1"
    else
        echo "Directory not found: $1"
    fi
}

# Function to safely remove file
safe_remove_file() {
    if [ -f "$1" ]; then
        echo "Removing: $1"
        rm -f "$1"
    else
        echo "File not found: $1"
    fi
}

cd "$PROJECT_DIR"

case "${1:-all}" in
    "all")
        echo "Cleaning everything (build directory + results)..."
        safe_remove_dir "build"
        safe_remove_dir "results"
        echo "✅ Complete cleanup finished"
        ;;

    "build")
        echo "Cleaning build directory only..."
        safe_remove_dir "build"
        echo "✅ Build directory cleaned"
        ;;

    "results")
        echo "Cleaning results only..."
        safe_remove_dir "results"
        safe_remove_dir "build/results"
        echo "✅ Results cleaned"
        ;;

    "executables")
        echo "Cleaning executables only..."
        if [ -d "build" ]; then
            cd build
            safe_remove_file "matching_engine_cli"
            safe_remove_file "matching_engine_benchmark"
            safe_remove_file "tests/load_tests"
            safe_remove_file "tests/core_tests"
            cd ..
        fi
        echo "✅ Executables cleaned"
        ;;

    *)
        echo "Usage: $0 [all|build|results|executables]"
        echo ""
        echo "Options:"
        echo "  all         - Remove build directory and results (default)"
        echo "  build       - Remove only build directory"
        echo "  results     - Remove only result files"
        echo "  executables - Remove only executable files"
        exit 1
        ;;
esac

echo ""
echo "Current state:"
echo "Build directory exists: $([ -d "build" ] && echo "Yes" || echo "No")"
echo "Results directory exists: $([ -d "results" ] && echo "Yes" || echo "No")"