# Gemini Code Assistant Context: serverlink

## Project Overview

**ServerLink** is a high-performance, cross-platform messaging library written in C++20. It provides a ZeroMQ-inspired API for various socket patterns and a location-transparent Pub/Sub system named **SPOT** (Scalable Partitioned Ordered Topics).

The library is designed with a clean, public-facing **C API** (`serverlink.h`) for broad compatibility, while the core implementation is object-oriented C++ organized into a `slk` namespace.

**Key Technologies:**
- **Language:** C++20 (core), C99 (public API)
- **Build System:** CMake
- **Dependencies:** Boost.Asio (optional, enabled by default) is downloaded via `FetchContent`.
- **Platforms:** Linux, macOS, Windows, BSD

**Architecture:**
- **Modular Core:** The source code in `src/` is highly modular, with components for core logic (`core`), I/O (`io`), transport protocols (`transport`), the SPOT system (`spot`), and more.
- **Pluggable I/O Backend:** The library abstracts the I/O polling mechanism, with platform-specific implementations for `epoll` (Linux), `kqueue` (macOS/BSD), and `select` (fallback). The presence of `build-iocp` and many related files suggests significant work is also focused on the Windows IOCP backend.
- **C API Layer:** A single C header, `include/serverlink/serverlink.h`, provides a stable, opaque-pointer-based interface to the underlying C++ implementation.

## Building and Running

The project uses CMake. The standard build process is documented in the `README.md`.

### Standard Build (Release)

1.  **Configure:**
    ```bash
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    ```
2.  **Build:**
    ```bash
    cmake --build build --config Release
    ```

### Running Tests

Tests are enabled by default and can be run via CTest.

```bash
ctest --test-dir build -C Release --output-on-failure
```

### Windows-Specific Build (User Workflow)

The user has several `.ps1` and `.bat` scripts that indicate a specific workflow on Windows using a `build-iocp` directory. For example, `rebuild_and_test.ps1` performs these steps:

1.  **Build Library:**
    ```powershell
    cmake --build build-iocp --config Release --target serverlink
    ```
2.  **Build a Test:**
    ```powershell
    cmake --build build-iocp --config Release --target test_ctx
    ```
3.  **Run Test:**
    ```powershell
    cd build-iocp/tests/Release
    ./test_ctx.exe
    ```

## Development Conventions

### Coding Style

- **C API (Public):**
    - All symbols are prefixed with `slk_` (functions) or `SLK_` (macros/constants).
    - Functions use snake_case: `slk_ctx_new()`.
    - Implementation details are hidden behind opaque pointers (`slk_ctx_t*`).
- **C++ (Internal):**
    - Code is encapsulated in the `slk` namespace.
    - Classes are postfixed with `_t`: `class tcp_listener_t`.
    - Methods use snake_case: `create_socket()`.
    - Member variables often have a `_` prefix or suffix.
    - Platform-specific code is handled with `#ifdef` preprocessor directives.

### Commits

- The Git history suggests the use of **Conventional Commits** (e.g., `feat:`, `fix:`, `docs:`).

### Language Preference

- The user communicates in **Korean**. Commit messages and documentation may be written in Korean or have Korean translations.
