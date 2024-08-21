# Assignment 2

## Part 1: Trace Buffer Support

Implemented the `create_trace_buffer()`, `read()`, `write()`, and `close()` system calls to manage a trace buffer in the gemOS kernel. Handled trace buffer state and user buffer validity checks.

## Part 2: System Call Tracing

Implemented the `start_strace()`, `end_strace()`, `strace()`, and `read_strace()` system calls to trace system calls and store the information in the trace buffer. Maintained a list of traced system calls and performed tracing using the `perform_tracing()` function.

## Part 3: Function Call Tracing

Implemented the `ftrace()` and `read_ftrace()` system calls to trace function calls, including function address, arguments, and backtrace (if enabled). Used the `handle_ftrace_fault()` function to handle the invalid opcode fault when a traced function is executed.
