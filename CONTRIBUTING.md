# Contributing to GemStack

Thank you for your interest in contributing to GemStack!

## Getting Started

1. **Fork** the repository on GitHub
2. **Clone** your fork locally:
   ```bash
   git clone --recurse-submodules https://github.com/YOUR-USERNAME/GemStack.git
   ```
3. **Build** the project (see [README.md](README.md#quick-start))
4. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Development Workflow

### Before You Start

- Check existing [issues](https://github.com/dudujuju28/GemStack/issues) for related work
- For significant changes, open an issue first to discuss the approach

### Making Changes

1. Write code following the [code style](#code-style) guidelines
2. Add or update tests for your changes
3. Ensure all tests pass: `cd build && ctest --output-on-failure`
4. Keep commits focused and atomic

### Submitting Changes

1. Push your branch to your fork
2. Open a Pull Request against `main`
3. Fill out the PR template with:
   - What the change does
   - Why it's needed
   - How to test it
4. Address review feedback

## Code Style

- **C++ Standard:** C++20
- **Naming:**
  - Functions/variables: `camelCase`
  - Classes/structs: `PascalCase`
  - Constants: `UPPER_SNAKE_CASE`
  - Member variables: no prefix
- **Formatting:**
  - 4-space indentation (no tabs)
  - Opening braces on same line
  - Max line length: ~100 characters
- **Headers:** Use include guards (`#ifndef`/`#define`) or `#pragma once`
- **Comments:** Explain *why*, not *what*

### Example

```cpp
struct CommandResult {
    std::string output;
    int exitCode;
    bool success;
};

bool executeCommand(const std::string& command) {
    if (command.empty()) {
        return false;
    }
    // Process the command...
    return true;
}
```

## Testing

- Add tests for new functionality in `tests/`
- Follow existing test patterns (GoogleTest framework)
- Test file naming: `test_<feature>.cpp`

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run specific tests
./GemStackTests --gtest_filter="YourTest.*"
```

## Questions?

- Open an [issue](https://github.com/dudujuju28/GemStack/issues) for bugs or feature requests
- Check existing issues and discussions first

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
