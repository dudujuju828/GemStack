# GemStack

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

GemStack is a C++ automation wrapper for the [Gemini CLI](https://github.com/google-gemini/gemini-cli). It executes sequential prompts ("tickets") via a queue system, enabling batch and interactive AI-driven workflows.

> **Note:** GemStack runs Gemini in `--yolo` mode (auto-approves tool calls).

## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Usage](#usage)
- [Configuration](#configuration)
- [Testing](#testing)
- [Repository Structure](#repository-structure)
- [Architecture Overview](#architecture-overview)
- [API Key Setup](#api-key--credentials-setup)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

## Features

| Feature | Description |
|---------|-------------|
| **Batch Processing** | Execute predefined tasks from `GemStackQueue.txt` |
| **Queue System** | Sequential command execution for stable output |
| **Interactive Mode** | Append commands during runtime |
| **Reflective Mode** | AI generates follow-up prompts iteratively |
| **Prompt Blocks** | Organize prompts with `goal`, `style`, and `specify` directives |
| **Session Log** | Persistent AI memory across prompts |
| **Auto-Commit** | Optional git commits after each prompt |
| **Cooldown** | Configurable delay between prompts to reduce rate limiting |
| **Model Fallback** | Auto-downgrades when rate-limited |

## Prerequisites

- **C++ Compiler** (C++20)
- **CMake** (3.20+)
- **Node.js** (for Gemini CLI)
- **Git**

## Quick Start

```bash
git clone --recurse-submodules https://github.com/dudujuju828/GemStack.git
cd GemStack
```

| Platform | Build Command |
|----------|---------------|
| Windows | `.\build.bat` |
| Linux/macOS | `chmod +x build.sh && ./build.sh` |

The build script handles submodule init, Gemini CLI setup, and CMake build.

<details>
<summary><strong>Manual Installation</strong></summary>

```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/dudujuju828/GemStack.git
cd GemStack

# 2. Build Gemini CLI
cd gemini-cli && npm install && npm run build && cd ..

# 3. Build GemStack
cmake -B build -S . && cmake --build build
```

</details>

## Usage

### Running GemStack

| Platform | Command |
|----------|---------|
| Windows | `.\GemStack.exe` |
| Linux/macOS | `./GemStack` |

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
| `autoCommitEnabled` | `false` | Git commit after each prompt for traceability |
| `autoCommitMessagePrefix` | `[GemStack]` | Prefix for commit messages |
| `autoCommitIncludePrompt` | `true` | Include prompt summary in commits |
| `cooldownEnabled` | `false` | Delay between prompts to reduce rate limiting |
| `cooldownSeconds` | `60` | Seconds to wait between prompts |

**Precedence:** CLI flags > Config file > Defaults

> **Tip:** Snake_case keys also work (e.g., `cooldown_enabled`, `cooldown_seconds`).

### Feature Details

<details>
<summary><strong>Cooldown</strong> — Reduce rate limiting with delays between prompts</summary>

```bash
# Enable with default 60 seconds
./GemStack --cooldown

# Custom delay
./GemStack --cooldown --cooldown-seconds 30
```

Delay occurs after each prompt completes (post session-log and auto-commit), before the next prompt starts. No delay after the final prompt.

</details>

<details>
<summary><strong>Auto-Commit</strong> — Git commits after each prompt</summary>

```bash
./GemStack --auto-commit
```

Creates commits formatted as: `[GemStack] <prompt summary>` (truncated to 72 chars). Useful for traceability and easy rollback.

</details>

<details>
<summary><strong>Session Log</strong> — Persistent AI memory</summary>

`GemStackSessionLog.txt` tracks completed prompts:

```
[2026-01-24 10:30:00] [SUCCESS] Initialize React project
[2026-01-24 10:32:15] [SUCCESS] Create Header component
```

The log is prepended to each prompt so the AI knows what's been done. Clear between projects:

```bash
rm GemStackSessionLog.txt  # Linux/macOS
del GemStackSessionLog.txt  # Windows
```

</details>

## Testing

GemStack uses [GoogleTest](https://github.com/google/googletest) for unit testing.

### Running Tests

```bash
# Build (includes tests)
cmake -B build -S .
cmake --build build

# Run all tests
cd build && ctest --output-on-failure

# Run specific test suite
./GemStackTests --gtest_filter="CooldownTest.*"
```

| Platform | Test Executable |
|----------|-----------------|
| Windows | `.\GemStackTests.exe` |
| Linux/macOS | `./GemStackTests` |

### Test Coverage

| File | Coverage |
|------|----------|
| `test_parsing.cpp` | Queue file parsing, directives |
| `test_multiline.cpp` | Multi-line `{{ }}` string handling |
| `test_git_auto_commit.cpp` | Auto-commit config and overrides |
| `test_process_executor.cpp` | Cross-platform command execution |
| `test_cooldown.cpp` | Cooldown delays, CLI precedence |

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
├── CONTRIBUTING.md         # Contribution guidelines
├── LICENSE                 # MIT License
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

| Problem | Solution |
|---------|----------|
| **CMake version too old** | Upgrade to 3.20+: [cmake.org/download](https://cmake.org/download/) |
| **C++20 not supported** | Update compiler: GCC 10+, Clang 10+, or MSVC 2019+ |
| **Permission denied** (Linux/macOS) | `chmod +x GemStack build.sh` |

<details>
<summary><strong>gemini-cli not found / empty submodule</strong></summary>

```bash
git submodule update --init --recursive
cd gemini-cli && npm install && npm run build
```

</details>

<details>
<summary><strong>Rate limit errors / model exhaustion</strong></summary>

GemStack auto-downgrades models when rate-limited. If all models exhausted:
- Wait 1-2 minutes and retry
- Enable cooldown: `--cooldown --cooldown-seconds 60`
- Check API quota at [Google AI Studio](https://aistudio.google.com/)

</details>

<details>
<summary><strong>Commands not executing</strong></summary>

- Verify `GemStackQueue.txt` has `GemStackSTART`/`GemStackEND` blocks
- Ensure file is in same directory as executable
- Test with minimal queue file first

</details>

<details>
<summary><strong>API key not working</strong></summary>

- Verify environment variable is set: `echo $GEMINI_API_KEY`
- Try re-authenticating: `cd gemini-cli && npx gemini login`
- Check key is valid at [Google AI Studio](https://aistudio.google.com/apikey)

</details>

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for full guidelines.

```bash
# Quick start
git checkout -b feature/my-feature
# Make changes, add tests
git commit -m "Add my feature"
# Submit pull request
```

### Code Style

| Aspect | Convention |
|--------|------------|
| Standard | C++20 |
| Functions/variables | `camelCase` |
| Classes/structs | `PascalCase` |
| Indentation | 4 spaces |
| Braces | Same line |

## License

[MIT License](LICENSE) — free to use, modify, and distribute.
