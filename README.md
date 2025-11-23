# SLOG

Structured logging library for C.

## Features

- Header-only C library (no dependencies)
- Structured JSON logging
- Thread-safe with memory pools
- Multiple data types support
- Auto escaping & timestamps

## Benchmarks

Running on **Arch Linux (Ryzen 9 9900X3D, GCC 15.2.1)**:

```console
make run_bench
Benchmarking SIMPLE log (1M iterations)...
Simple Log: 1.1032 seconds | 906456 Ops/sec
--------------------------------
Benchmarking COMPLEX log (1M iterations)...
Complex Log: 1.8559 seconds | 538828 Ops/sec
```

*Benchmarks run with `-O3 -DNDEBUG` and a no-op handler to isolate formatting overhead. (Run `make run_bench` to try)*

## Tutorial

```c
#include "slog.h"

int main() {
    // Message
    SLOG(SLOG_INFO, "Hello World");

    // Types
    SLOG(SLOG_INFO, "User information",
        SLOG_STRING("name", "qaqland"),
        SLOG_INT("age", 25),
        SLOG_FLOAT("score", 96.5),
        SLOG_BOOL("active", true));

    // Nested objects
    SLOG(SLOG_INFO, "Complex data structure",
        SLOG_OBJECT("user",
            SLOG_STRING("name", "qaqland"),
            SLOG_OBJECT("profile",
                SLOG_STRING("email", "qaq@qaq.land"),
                SLOG_INT("id", 114514))));

    SLOG_FREE();
    return 0;
}
```

## Acknowledgements

Special thanks to [@XJJ](https://github.com/mivinci) for the valuable
contribution and support.

## License

Mozilla Public License 2.0
