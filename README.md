# FSM-Based Cache Controller Simulator

## Overview
This project simulates a finite state machine (FSM) based direct-mapped cache controller in C++. It demonstrates cache operations such as read/write, cache hits and misses, conflict handling, and write-back using a dirty bit. The system also simulates memory latency.

## Features
- Direct-mapped cache (4 sets)
- 8-bit address space (256 memory locations)
- FSM-based control (IDLE, COMPARE_TAG, WRITE_BACK, ALLOCATE)
- Read and write operations
- Cache hit/miss handling
- Dirty bit and write-back support

## Build
g++ coa_a2_230041125_fsm.cpp -o outres

## Run
./outres

## Test
The program runs predefined CPU requests to test cache behavior:

READ 0x04
READ 0x04
WRITE 0x04 = 999
READ 0x08
READ 0x05
WRITE 0x09 = 42

Another batch of example for testing is commented out, that can be also used.

## Output
- FSM execution trace
- Cache hits and misses
- Write-back operations
- Final cache state
- Hit rate and statistics
