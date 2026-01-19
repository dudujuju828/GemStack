# GemStack

GemStack is a C++ automation wrapper for the [Gemini CLI](https://github.com/google-gemini/gemini-cli). It enables the execution of sequential prompts and tasks ("tickets") via a queue system.

The application allows users to define a series of commands in a file for batch processing, or to input commands interactively. This facilitates automated, high-volume interaction with the Gemini CLI.

**Note:** GemStack automatically runs Gemini in `--yolo` mode, meaning it will auto-approve tool calls without user confirmation.

## Features

- **Batch Processing**: Execute a list of pre-defined tasks from a text file.
- **Queue System**: Ensures sequential command execution for stable output.
- **Interactive Mode**: Append commands to the processing queue during runtime.
- **Gemini CLI Integration**: utilizes the underlying Node.js-based Gemini CLI for all AI interactions.
- **Auto-Approval**: Runs in YOLO mode by default for uninterrupted automation.

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

The `GemStackQueue.txt` file defines the commands to be executed. Place this file in the project root directory (or the same directory as the executable).

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

Run the executable from the project root to ensure it can locate the `gemini-cli` scripts and the `GemStackQueue.txt` file.

**Windows:**
```powershell
.\build\Debug\GemStack.exe
# Or depending on configuration:
.\build\GemStack.exe
```

**Linux/macOS:**
```bash
./build/GemStack
```

### Modes of Operation

1.  **Batch Mode**: Upon launch, GemStack immediately processes all commands found in `GemStackQueue.txt`.
2.  **Interactive Mode**: Users can manually type commands into the console to append them to the queue.
3.  **Termination**: The application will automatically exit after processing all file-based commands. in interactive mode, type `exit` or `quit` to close the application.
