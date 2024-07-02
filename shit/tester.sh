#!/bin/bash

# Define the shell executable
SHELL_EXEC="./a.out"

# Function to run a test
run_test() {
    local test_description=$1
    local command=$2
    local expected_output=$3
    local temp_output=$(mktemp)

    echo "$test_description"
    echo "$command" | $SHELL_EXEC > $temp_output
    diff <(echo "$expected_output") $temp_output && echo "Test passed" || echo "Test failed"
    echo "Expected output:"
    echo "$expected_output"
    echo "Actual output:"
    cat $temp_output
    rm $temp_output
    echo
}

# Test simple command execution
run_test "Test simple command execution" \
         "echo Hello, World!" \
         "Hello, World!"

# Test input redirection
echo "File content" > input.txt
run_test "Test input redirection" \
         "cat < input.txt" \
         "File content"

# Test output redirection
run_test "Test output redirection" \
         "echo Hello, World! > output.txt; cat output.txt" \
         "Hello, World!"

# Test background execution
run_test "Test background execution" \
         "echo Hello, World! &" \
         "Hello, World!"

# Test pipe
run_test "Test pipe" \
         "echo Hello, World! | cat" \
         "Hello, World!"

echo "Testing completed."
