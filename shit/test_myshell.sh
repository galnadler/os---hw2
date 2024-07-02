#!/bin/bash

# Compile the shell
echo "Compiling the shell..."
gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 shell.c myshell.c -o my_shell
if [ $? -ne 0 ]; then
    echo "Compilation failed."
    exit 1
fi
echo "Compilation successful."

# Function to run a test command and compare output
run_test() {
    local command=$1
    local expected_output=$2

    echo "Running: ./my_shell -c \"$command\""
    output=$(./my_shell -c "$command")
    if [ "$output" == "$expected_output" ]; then
        echo "Test passed: $command"
    else
        echo "Test failed: $command"
        echo "Expected: $expected_output"
        echo "Got: $output"
    fi
    echo
}

# Simple command test
run_test "echo Hello, World!" "Hello, World!"

# Test listing files
run_test "ls" "$(ls)"

# Test current directory
run_test "pwd" "$(pwd)"

# Test date command
run_test "date" "$(date)"

# Test background execution
echo "Running background command: sleep 2 &"
./my_shell -c "sleep 2 &"
echo "Background command executed"
echo

# Test input redirection
echo "Hello, World!" > input.txt
run_test "cat < input.txt" "Hello, World!"
rm input.txt

# Test output redirection
echo "Running: echo Hello, World! > output.txt"
./my_shell -c "echo Hello, World! > output.txt"
output=$(cat output.txt)
if [ "$output" == "Hello, World!" ]; then
    echo "Test passed: echo Hello, World! > output.txt"
else
    echo "Test failed: echo Hello, World! > output.txt"
    echo "Expected: Hello, World!"
    echo "Got: $output"
fi
rm output.txt
echo

# Test piping
run_test "echo Hello, World! | grep Hello" "Hello, World!"

echo "All tests completed."
