# Assignment 3

## Overview

Implemented memory management system calls in the gemOS operating system, including `mmap()`, `munmap()`, `mprotect()`, and `cfork()` with copy-on-write functionality.

## Key Tasks

1. **Memory Mapping Support** : Implemented VMA management for the `mmap()`, `munmap()`, and `mprotect()` system calls.
2. **Page Table Manipulations** : Handled page faults and updated page tables to enable lazy allocation.
3. **Copy-on-Write fork** : Implemented the `cfork()` system call to create a new process with copy-on-write address space.
