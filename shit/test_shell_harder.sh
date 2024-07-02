#!/bin/bash

# Compile the shell program if not compiled
if [ ! -f a.out ]; then
    echo "Compiling myshell.c..."
    gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 myshell.c -o a.out
fi

# Function to run a single test
run_test() {
    local command="$1"
    local expected="$2"
    echo "Testing command: $command"
    output=$(echo "$command" | ./a.out 2>&1)
    if [ "$output" = "$expected" ]; then
        echo "PASS"
    else
        echo "FAIL"
        echo "Expected: $expected"
        echo "Got: $output"
    fi
    echo
}

# Test cases
run_test "echo This is a test" "This is a test"

run_test "ls -l" "$(ls -l)"

run_test "pwd" "$(pwd)"

run_test "date" "$(date)"

run_test "sleep 1 &" ""

run_test "echo Background process &" "Background process"

echo "This is a test file with input redirection." > test_input.txt
run_test "cat < test_input.txt" "This is a test file with input redirection."

run_test "echo This is a test of output redirection > test_output.txt" ""
run_test "cat test_output.txt" "This is a test of output redirection"

run_test "echo Pipe test | tr a-z A-Z" "PIPE TEST"

# More complex tests
run_test "echo Combined test | tr 'a-z' 'A-Z' > test_combined_output.txt" ""
run_test "cat test_combined_output.txt" "COMBINED TEST"

echo "Input for combined test" > test_combined_input.txt
run_test "cat < test_combined_input.txt | tr 'a-z' 'A-Z' > test_combined_output.txt" ""
run_test "cat test_combined_output.txt" "INPUT FOR COMBINED TEST"

run_test "echo Nested pipes | tr 'a-z' 'A-Z' | tr 'A-Z' 'a-z' | tr 'a-z' 'A-Z'" "NESTED PIPES"

run_test "echo Multiple background processes 1 & echo Multiple background processes 2 &" ""

echo "All tests completed."
