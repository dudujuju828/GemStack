# GemStack

GemStack is a C++ automation wrapper for the [Gemini CLI](https://github.com/google-gemini/gemini-cli). It executes sequential prompts ("tickets") via a queue system, enabling batch and interactive AI-driven workflows.

**Note:** GemStack runs Gemini in `--yolo` mode (auto-approves tool calls).

## Features

- **Batch Processing** — Execute predefined tasks from `GemStackQueue.txt`
- **Queue System** — Sequential command execution for stable output
- **Interactive Mode** — Append commands during runtime
- **Reflective Mode** — AI generates follow-up prompts iteratively
- **Prompt Blocks** — Organize prompts with `goal`, `style`, and `specify` directives
- **Session Log** — Persistent AI memory across prompts (`GemStackSessionLog.txt`)
- **Auto-Commit** — Optional git commits after each prompt
- **Model Fallback** — Auto-downgrades when rate-limited

## Prerequisites

- **C++ Compiler** (C++20)
- **CMake** (3.20+)
- **Node.js** (for Gemini CLI)
- **Git**

## Quick Start

```bash
git clone --recurse-submodules https://github.com/dudujuju28/GemStack.git
cd GemStack

# Windows
.\build.bat

# Linux/macOS
chmod +x build.sh && ./build.sh
```

The build script handles submodule init, Gemini CLI setup, and CMake build.

### Manual Installation

```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/dudujuju28/GemStack.git
cd GemStack

# 2. Build Gemini CLI
cd gemini-cli && npm install && npm run build && cd ..

# 3. Build GemStack
cmake -B build -S . && cmake --build build
```

## Usage

### Running GemStack

```bash
# Windows
.\GemStack.exe

# Linux/macOS
./GemStack
```

GemStack processes `GemStackQueue.txt` in the current directory, then enters interactive mode.

### GemStackQueue.txt Syntax

```text
GemStackSTART
prompt "Tell me a joke about C++"
prompt {{
  Explain how shared_ptr works
  and compare it with unique_ptr
}}
--version
GemStackEND
```

- Wrap commands in `GemStackSTART` / `GemStackEND`
- Use `"..."` for single-line strings, `{{ ... }}` for multi-line
- Each line is a separate command

### Prompt Blocks with Goals, Styles, and Specifications

Organize related prompts and enforce consistency:

```text
GemStackSTART

PromptBlockSTART
goal "A React app with Header and Footer components"

style "Use TypeScript with functional components"
style "Follow BEM naming for CSS"

prompt "Initialize React project with TypeScript"

specify "my-app folder exists with package.json"
prompt "Create Header component with navigation"

specify "Header.tsx exists in src/components"
prompt "Add responsive CSS modules"
PromptBlockEND

GemStackEND
```

| Directive | Purpose |
|-----------|---------|
| `PromptBlockSTART/END` | Group related prompts; resets state between blocks |
| `goal "..."` | High-level objective prepended to all prompts in block |
| `style "..."` | Coding conventions prepended to all prompts (persists) |
| `specify "..."` | Checkpoint verified before next prompt (clears after use) |
| `prompt "..."` | Task for AI to execute |

**Behavior:** Goals and styles are prepended to every prompt. Specifications become verification checkpoints that the AI must confirm before proceeding.

### Reflective Mode

AI iteratively improves work by generating its own follow-up prompts:

```bash
.\GemStack.exe --reflect "Build a calculator app" --iterations 10
```

Each iteration asks: "What's the most impactful next step?" and executes the AI's response.

### Command Line Options

| Option | Description |
|--------|-------------|
| `--reflect <prompt>` | Enable reflective mode with initial prompt |
| `--iterations <n>` | Max reflective iterations (default: 5) |
| `--config <path>` | Load config from specified path |
| `--auto-commit` | Enable git auto-commit for this run |
| `--no-auto-commit` | Disable git auto-commit for this run |
| `--commit-prefix <text>` | Override commit message prefix |
| `--cooldown` | Enable cooldown delay between prompts |
| `--no-cooldown` | Disable cooldown delay between prompts |
| `--cooldown-seconds <n>` | Set cooldown delay duration (default: 60) |
| `--help` | Show help |

## Configuration

Optional `GemStackConfig.txt` in the executable directory:

```ini
# Auto-commit settings
autoCommitEnabled=true
autoCommitMessagePrefix=[GemStack]
autoCommitIncludePrompt=true

# Cooldown settings (reduces rate limiting / IP flagging)
cooldownEnabled=true
cooldownSeconds=60
```

| Setting | Default | Description |
|---------|---------|-------------|
| `autoCommitEnabled` | `false` | Enable git commits after each prompt |
| `autoCommitMessagePrefix` | `[GemStack]` | Prefix for commit messages |
| `autoCommitIncludePrompt` | `true` | Include prompt summary in commits |
| `cooldownEnabled` | `false` | Enable delay between prompts |
| `cooldownSeconds` | `60` | Seconds to wait between prompts |

**Precedence:** CLI flags > Config file > Defaults

**Note:** Alternative snake_case keys also work (e.g., `cooldown_enabled`, `cooldown_seconds`).

### Cooldown

Adds a configurable delay between prompts to reduce rate limiting and IP flagging when running large queue files or long reflective sessions.

```bash
# Enable via CLI with default 60 seconds
.\GemStack.exe --cooldown

# Custom delay duration
.\GemStack.exe --cooldown --cooldown-seconds 30

# Or via config
cooldownEnabled=true
cooldownSeconds=60
```

**Behavior:** Delay occurs after each prompt completes (including session log update and auto-commit) and before the next prompt starts. No delay after the final prompt.

### Auto-Commit

Creates a git commit after each successful prompt for traceability and rollback.

```bash
# Enable via CLI
.\GemStack.exe --auto-commit

# Or via config
autoCommitEnabled=true
```

Commits are formatted as: `[GemStack] <prompt summary>` (truncated to 72 chars).

### Session Log (AI Memory)

`GemStackSessionLog.txt` tracks completed prompts and allows AI to record notes:

```
[2024-01-15 10:30:00] [SUCCESS] Initialize React project
[2024-01-15 10:32:15] [SUCCESS] Create Header component
```

The log is prepended to each prompt so AI knows what's been done. Clear it between projects:

```bash
rm GemStackSessionLog.txt  # or: del GemStackSessionLog.txt
```

## Testing

GemStack uses [GoogleTest](https://github.com/google/googletest) for unit testing.

### Running Tests

```bash
# Build (includes tests)
cmake -B build -S .
cmake --build build

# Run all tests
cd build && ctest --output-on-failure

# Or run test executable directly
./GemStackTests        # Linux/macOS
.\GemStackTests.exe    # Windows

# Run specific test
./GemStackTests --gtest_filter="ParsingTest.*"
```

### Test Files

| File | Coverage |
|------|----------|
| `tests/test_parsing.cpp` | Queue file parsing |
| `tests/test_multiline.cpp` | Multi-line string handling |
| `tests/test_git_auto_commit.cpp` | Git auto-commit logic |
| `tests/test_process_executor.cpp` | Process execution |
| `tests/test_cooldown.cpp` | Cooldown delay logic |

## Repository Structure

```
GemStack/
├── src/                    # C++ source files
│   ├── main.cpp           # Entry point, CLI parsing, queue orchestration
│   ├── GemStackCore.cpp   # Core queue/parsing logic, utilities
│   ├── CliManager.cpp     # Gemini CLI extraction and path management
│   ├── ConsoleUI.cpp      # Progress display and status animations
│   ├── GitAutoCommit.cpp  # Auto-commit functionality
│   └── ProcessExecutor.cpp # Cross-platform command execution
├── include/                # Header files
│   ├── GemStackCore.h
│   ├── CliManager.h
│   ├── ConsoleUI.h
│   ├── GitAutoCommit.h
│   ├── ProcessExecutor.h
│   └── EmbeddedCli.h      # Embedded Gemini CLI binary (generated)
├── tests/                  # GoogleTest unit tests
├── gemini-cli/             # Gemini CLI submodule
├── build/                  # CMake build output (generated)
├── CMakeLists.txt          # CMake configuration
├── build.bat               # Windows build script
├── build.sh                # Linux/macOS build script
├── embed_cli.py            # Generates EmbeddedCli.h from gemini-cli
├── GemStackQueue.txt       # Example queue file
└── README.md
```

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         main.cpp                                │
│   Entry point, CLI args, queue orchestration, reflective mode   │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      GemStackCore                               │
│   Queue management, file parsing, config loading, utilities     │
│   - loadCommandsFromFile()  - Model fallback system             │
│   - Session log management  - String/path utilities             │
└─────────────────────────────────────────────────────────────────┘
        │              │              │              │
        ▼              ▼              ▼              ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│  CliManager  │ │  ConsoleUI   │ │GitAutoCommit │ │ProcessExecutor│
│  CLI extract │ │  Progress    │ │  Git commits │ │  Run commands │
│  & paths     │ │  & status    │ │  after tasks │ │  capture out  │
└──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────────────┐
│                     gemini-cli (Node.js)                        │
│         Actual Gemini API interactions (submodule)              │
└─────────────────────────────────────────────────────────────────┘
```

**Data Flow:**
1. `main.cpp` parses CLI args and loads queue from `GemStackQueue.txt`
2. `GemStackCore` parses directives, manages queue, handles model fallback
3. Commands execute via `ProcessExecutor` calling `gemini-cli`
4. `ConsoleUI` displays progress; `GitAutoCommit` commits changes
5. Session log updated; next command processed

## API Key / Credentials Setup

GemStack uses the Gemini CLI for AI interactions. Configure credentials via Gemini CLI's standard methods:

### Option 1: Environment Variable (Recommended)

```bash
# Linux/macOS
export GEMINI_API_KEY="your-api-key-here"

# Windows PowerShell
$env:GEMINI_API_KEY="your-api-key-here"

# Windows CMD
set GEMINI_API_KEY=your-api-key-here
```

### Option 2: Gemini CLI Login

```bash
cd gemini-cli
npx gemini login
```

This opens a browser for Google OAuth authentication.

### Getting an API Key

1. Go to [Google AI Studio](https://aistudio.google.com/apikey)
2. Create or select a project
3. Generate an API key
4. Set it via environment variable or CLI login

## Troubleshooting

### Build fails: "CMake version too old"
Upgrade CMake to 3.20+. Download from [cmake.org](https://cmake.org/download/).

### Build fails: "C++20 not supported"
Update your compiler. GCC 10+, Clang 10+, or MSVC 2019+ required.

### "gemini-cli not found" or empty submodule
```bash
git submodule update --init --recursive
cd gemini-cli && npm install && npm run build
```

### Rate limit errors / model exhaustion
GemStack auto-downgrades models. If all models exhausted, wait and retry, or check your API quota at [Google AI Studio](https://aistudio.google.com/).

### Commands not executing
- Verify `GemStackQueue.txt` syntax (must have `GemStackSTART`/`GemStackEND`)
- Check file is in same directory as executable
- Run with sample queue to test

### Permission denied (Linux/macOS)
```bash
chmod +x GemStack build.sh
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Quick start:**
1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make changes with tests
4. Submit a pull request

## Code Style

- **Standard:** C++20
- **Naming:** `camelCase` for functions/variables, `PascalCase` for classes/structs
- **Headers:** Use `#pragma once` or include guards
- **Formatting:** 4-space indentation, braces on same line
- **No external linter configured** — follow existing code patterns

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
