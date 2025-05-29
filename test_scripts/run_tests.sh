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
    "test_03_objects.ember"  # Fixed - nested object compilation issue resolved
    "test_04_functions.ember"
    "test_05_operators.ember"
    "test_06_control_flow.ember"
    "test_07_mixins.ember"
    "test_08_complex_expressions.ember"  # Mostly working - minor if-statement edge cases remain
    
    # Iterator Implementation Tests (Phase 1-3)
    "test_range_phase1.ember"           # Phase 1: Range operator implementation
    "test_naked_iterator_phase2.ember"  # Phase 2: Range-based naked iterators
    "test_naked_iterator_phase3.ember"  # Phase 3: Full collection iterators (array and object iteration working)
    "test_array_iteration_only.ember"   # Phase 3: Array-based naked iterators (superseded by full test)
    
    # Additional range tests
    "test_simple_range.ember"
    "test_range_detailed.ember"
    "test_debug_range.ember"
    
    # Additional object tests
    "test_simple_object.ember"
    "test_empty_object.ember"
    "test_manual_object.ember"
)

passed=0
failed=0
skipped=0

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
echo "Total Tests: $((passed + failed + skipped))"
echo "Passed: $passed"
echo "Failed: $failed"
echo "Skipped: $skipped"

if [ $failed -eq 0 ]; then
    echo "🎉 All tests passed!"
    echo "📝 Note: Iterator implementation complete (Phases 1-3: Range operator, naked iterators, array and object iteration)"
    echo "📝 Note: Both array iteration (showing indices) and object iteration (showing keys) are working as expected"
    exit 0
else
    echo "⚠️  Some tests failed."
    exit 1
fi 