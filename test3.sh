#!/bin/bash

# Function to run a command in the custom shell and check for expected output
run_test() {
    command=$1
    expected_part=$2
    echo -e "$command" | ./mysh > output.txt
    # Check if the expected output is contained within the actual output
    if grep -q "$expected_part" output.txt; then
        echo "PASS: $command"
    else
        actual_output=$(cat output.txt)
        echo "FAIL: $command. Expected to find '$expected_part', got '$actual_output'"
    fi
}

# pwd and which tests
run_test "pwd" "$(pwd)"
run_test "which ls" "$(which ls)"


# Simple echo test
run_test "echo Hello World" "Hello World"

# Redirection test - Writing to and reading from a file
echo "Hello World" > input.txt
run_test "cat < input.txt" "Hello World"

# Pipeline test - Connecting two commands
run_test "echo Hello World | wc -w" "2"


# Clean up
rm output.txt input.txt