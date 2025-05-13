#!/bin/bash

# Replace spaces with underscores for processing
board="$1"

# AI move logic
while true; do
  randNum=$((RANDOM % 9))  
  if [ "${board:$randNum:1}" == "_" ]; then
    board="${board:0:$randNum}O${board:$((randNum+1))}"
    break
  fi
done

# Convert back to spaces before returning
echo "$board"