# GemStack

GemStack is a C++ automation wrapper for the [Gemini CLI](https://github.com/google-gemini/gemini-cli). It enables the execution of sequential prompts and tasks ("tickets") via a queue system.

The application allows users to define a series of commands in a file for batch processing, or to input commands interactively. This facilitates automated, high-volume interaction with the Gemini CLI.

**Note:** GemStack automatically runs Gemini in `--yolo` mode, meaning it will auto-approve tool calls without user confirmation.

## Features

- **Batch Processing**: Execute a list of pre-defined tasks from a text file.
- **Queue System**: Ensures sequential command execution for stable output.
- **Interactive Mode**: Append commands to the processing queue during runtime.
- **Reflective Mode**: AI continuously improves its work by generating its own follow-up prompts.
- **Gemini CLI Integration**: utilizes the underlying Node.js-based Gemini CLI for all AI interactions.
- **Auto-Approval**: Runs in YOLO mode by default for uninterrupted automation.
- **Automatic Model Fallback**: Automatically downgrades to a less capable model when rate limits are hit.

## Prerequisites

Ensure the following dependencies are installed:

- **C++ Compiler** (C++20 compliant)
- **CMake** (Version 3.20 or later)
- **Node.js** (Required for the underlying Gemini CLI)
- **Git**

## Quick Start

After cloning the repository, run the build script to automatically set up everything:

**Windows:**
```powershell
git clone --recurse-submodules https://github.com/dudujuju828/GemStack.git
cd GemStack
.\build.bat
```

**Linux/macOS:**
```bash
git clone --recurse-submodules https://github.com/dudujuju828/GemStack.git
cd GemStack
chmod +x build.sh
./build.sh
```

The build script handles submodule initialization, Gemini CLI setup, and CMake configuration automatically.

## Manual Installation

If you prefer to set up manually:

1.  **Clone the Repository**
    Clone the repository including submodules to ensure the Gemini CLI source is retrieved:
    ```bash
    git clone --recurse-submodules https://github.com/dudujuju828/GemStack.git
    cd GemStack
    ```
    If the repository was cloned without submodules, initialize them manually:
    ```bash
    git submodule update --init --recursive
    ```

2.  **Setup Gemini CLI**
    Install dependencies and build the nested `gemini-cli`:
    ```bash
    cd gemini-cli
    npm install
    npm run build
    cd ..
    ```

3.  **Build GemStack**
    Configure and build the C++ application using CMake:
    ```bash
    cmake -B build -S .
    cmake --build build
    ```

## Usage

### Configuration: GemStackQueue.txt

The `GemStackQueue.txt` file defines the commands to be executed. Place this file in the same directory as the executable.

#### Syntax
- **Structure**: Enclose all commands within a single `GemStackSTART` and `GemStackEND` block.
- **Commands**: Each line inside the block is treated as a separate command to be executed.
- **Blank Lines**: Empty lines are ignored.

#### Configuration Example

You can queue as many prompts or CLI arguments as needed within the block:

```text
GemStackSTART
prompt "Tell me a joke about C++"
prompt "Explain how shared_ptr works"
--version
--help
GemStackEND
```

### Execution

The executable is built directly in the project root directory for convenience.

**Windows:**
```powershell
.\GemStack.exe
```

**Linux/macOS:**
```bash
./GemStack
```

**Note:** GemStack runs in the current directory. The AI has access to read and write files in the directory from which you run the application.

### Modes of Operation

1.  **Batch Mode**: Upon launch, GemStack immediately processes all commands found in `GemStackQueue.txt`.
2.  **Interactive Mode**: Users can manually type commands into the console to append them to the queue.
3.  **Reflective Mode**: AI iteratively improves its work by generating follow-up prompts (see below).
4.  **Termination**: The application will automatically exit after processing all file-based commands. In interactive mode, type `exit` or `quit` to close the application.

### Reflective Mode

Reflective mode enables autonomous, iterative development where the AI continuously improves its work by asking itself what to do next.

**Usage:**
```powershell
# Windows
.\GemStack.exe --reflect "Your initial prompt here"

# With custom iteration limit (default is 5)
.\GemStack.exe --reflect "Build a calculator app" --iterations 10
```

```bash
# Linux/macOS
./GemStack --reflect "Your initial prompt here"
./GemStack --reflect "Build a calculator app" --iterations 10
```

**How it works:**
1. GemStack executes your initial prompt
2. After completion, it asks the AI: "What is the single most impactful next step to improve or extend this work?"
3. The AI's response becomes the next prompt
4. This cycle repeats until the iteration limit is reached

**Example:**
```powershell
.\GemStack.exe --reflect "Create a basic HTML webpage with a greeting" --iterations 3
```

This might produce iterations like:
- Iteration 1: Creates the basic HTML page
- Iteration 2: AI decides to add CSS styling
- Iteration 3: AI decides to add JavaScript interactivity

**Command Line Options:**
| Option | Description |
|--------|-------------|
| `--reflect <prompt>` | Activate reflective mode with the given initial prompt |
| `--iterations <n>` | Set maximum iterations (default: 5) |
| `--help` | Display help message |
