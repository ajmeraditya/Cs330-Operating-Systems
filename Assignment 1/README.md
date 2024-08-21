
# Assignment 2

## Part 1: Chain of Unary Operations

Implemented three C programs - `square.c`, `double.c`, and `sqroot.c` to perform square, double, and square root operations respectively. The generated executables can be chained to perform composite operations.

## Part 2: Directory Space Usage

Implemented a program `myDU.c` that finds the space used by a directory, including its files and sub-directories. Used `fork` to create child processes for sub-directories and `pipe` for communication.

## Part 3: Dynamic Memory Management Library

Implemented `memalloc()` and `memfree()` functions that are equivalent to `malloc` and `free`. Used `mmap` to request memory from the OS and maintained metadata for allocated and free memory chunks.
