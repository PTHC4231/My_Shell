#!/bin/bash

# Function to run a command in the custom shell and compare the output
# against the expected result, ignoring trailing newlines.
run_test() {
    command=$1
    expected_output=$(echo -e "$2")

    # Execute the command using the custom shell and capture the output,
    # removing trailing newlines for comparison.
    output=$(echo -e "$command" | ./mysh | sed 's/[[:space:]]*$//')

    # Check if the output matches the expected output
    if [ "$output" == "$expected_output" ]; then
        echo "PASS: $command"
    else
        echo "FAIL: $command. Expected '$expected_output', got '$output'"
    fi
}

# Test 'pwd' command
expected_pwd_output=$(pwd)
run_test "pwd" "$expected_pwd_output"

# Test 'cd' command followed by 'pwd' in a single invocation to ensure
# the effect of 'cd' persists for the subsequent 'pwd'.
# This combines two commands into a single test scenario.
run_test "cd /tmp; pwd" "/tmp"

# Test 'which ls' command
# Depending on the environment, 'which ls' might return paths like
# '/bin/ls' or '/usr/bin/ls'. Adjust the expected output accordingly.
expected_which_ls_output=$(which ls)
run_test "which ls" "$expected_which_ls_output"

# Add additional tests as necessary
