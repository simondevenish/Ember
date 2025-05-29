#!/bin/bash

# EmberScript Test Suite Runner
echo "========================================"
echo "       EmberScript Test Suite"
echo "========================================"
echo ""

# Make sure we're in the right directory
cd "$(dirname "$0")/.."

# Build EmberScript if needed
if [ ! -f "build/emberc" ]; then
    echo "Building EmberScript..."
    make
    echo ""
fi

# Test files
tests=(
    "test_01_variables.ember"
    "test_02_arrays.ember" 
    "test_03_objects.ember"
    "test_04_functions.ember"
    "test_05_operators.ember"
    "test_06_control_flow.ember"
    "test_07_mixins.ember"
    "test_08_complex_expressions.ember"
)

passed=0
failed=0

# Run each test
for test in "${tests[@]}"; do
    echo "----------------------------------------"
    echo "Running: $test"
    echo "----------------------------------------"
    
    if ./build/emberc exec "test_scripts/$test"; then
        echo "✅ $test - PASSED"
        ((passed++))
    else
        echo "❌ $test - FAILED"
        ((failed++))
    fi
    echo ""
done

# Summary
echo "========================================"
echo "          Test Results"
echo "========================================"
echo "Total Tests: $((passed + failed))"
echo "Passed: $passed"
echo "Failed: $failed"

if [ $failed -eq 0 ]; then
    echo "🎉 All tests passed!"
    exit 0
else
    echo "⚠️  Some tests failed."
    exit 1
fi 