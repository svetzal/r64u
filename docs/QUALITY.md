# Quality Tooling Guide

This project includes a comprehensive suite of quality tools for C++/Qt development.

## Prerequisites

Install the required tools via Homebrew:

```bash
# Code formatting
brew install clang-format

# Static analysis
brew install llvm        # Provides clang-tidy
brew install cppcheck

# Note: Coverage uses Apple's built-in llvm-cov (no installation needed)
```

After installing LLVM, add it to your PATH for clang-tidy:
```bash
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
```

## Quick Start

```bash
# Configure (from build directory)
cmake ..

# Run all quality checks
cmake --build . --target quality
```

## Available CMake Options

| Option | Description | Default |
|--------|-------------|---------|
| `BUILD_COVERAGE` | Enable code coverage instrumentation | OFF |
| `ENABLE_CLANG_TIDY` | Run clang-tidy during compilation | OFF |
| `SANITIZER` | Enable runtime sanitizers | (empty) |

## Code Coverage

Generate coverage reports for your tests:

```bash
# Configure with coverage
cmake .. -DBUILD_COVERAGE=ON

# Build and run tests with coverage
cmake --build . --target coverage_transferqueue

# View HTML report
open coverage/transferqueue_html/index.html
```

The coverage report shows:
- Line coverage (percentage of lines executed)
- Region coverage (percentage of code branches taken)
- Function coverage (percentage of functions called)

## Code Formatting

Uses `clang-format` with project-specific style (`.clang-format`):

```bash
# Format all source files
cmake --build . --target format

# Check formatting without modifying (for CI)
cmake --build . --target format-check
```

## Static Analysis

### clang-tidy

Comprehensive C++ linter with modernization suggestions:

```bash
# Enable during build (catches issues as you compile)
cmake .. -DENABLE_CLANG_TIDY=ON
cmake --build .
```

Configuration is in `.clang-tidy`. Key checks include:
- Bug-prone patterns (`bugprone-*`)
- Modern C++ idioms (`modernize-*`)
- Performance issues (`performance-*`)
- Readability concerns (`readability-*`)

### cppcheck

Complementary static analyzer:

```bash
cmake --build . --target cppcheck
```

## Runtime Sanitizers

Detect runtime errors by building with sanitizers:

### AddressSanitizer (ASan)

Detects memory errors: buffer overflows, use-after-free, memory leaks.

```bash
cmake .. -DSANITIZER=address
cmake --build .
./tests/test_transferqueue  # Runs with ASan checking
```

### UndefinedBehaviorSanitizer (UBSan)

Detects undefined behavior: integer overflow, null pointer dereference, etc.

```bash
cmake .. -DSANITIZER=undefined
cmake --build .
```

### Multiple Sanitizers

Combine sanitizers (ASan + UBSan work well together):

```bash
cmake .. -DSANITIZER="address;undefined"
cmake --build .
```

### ThreadSanitizer (TSan)

Detects data races in multithreaded code:

```bash
cmake .. -DSANITIZER=thread
cmake --build .
```

Note: TSan cannot be combined with ASan.

## CI Integration

For continuous integration, use these targets:

```bash
# Check code formatting
cmake --build . --target format-check

# Run static analysis
cmake --build . --target cppcheck

# Run tests with coverage
cmake .. -DBUILD_COVERAGE=ON
cmake --build . --target coverage

# Run tests with sanitizers
cmake .. -DSANITIZER="address;undefined"
cmake --build .
ctest --output-on-failure
```

## Project Files

| File | Purpose |
|------|---------|
| `.clang-format` | Code style configuration |
| `.clang-tidy` | Static analysis rules |
| `cmake/Quality.cmake` | Main quality module |
| `cmake/Coverage.cmake` | Coverage support |
| `cmake/Formatting.cmake` | Formatting targets |
| `cmake/StaticAnalysis.cmake` | Static analysis targets |
| `cmake/Sanitizers.cmake` | Sanitizer support |

## Recommended Workflow

1. **During development**: Use `format` before committing
2. **Before PR**: Run `quality` target to catch issues
3. **In CI**: Run `format-check`, `cppcheck`, and tests with coverage
4. **Periodically**: Run tests with sanitizers to catch runtime issues
