# GemStack

GemStack is a C++ automation wrapper for the [Gemini CLI](https://github.com/google-gemini/gemini-cli). It enables the execution of sequential prompts and tasks ("tickets") via a queue system.

The application allows users to define a series of commands in a file for batch processing, or to input commands interactively. This facilitates automated, high-volume interaction with the Gemini CLI.

## Features

- **Batch Processing**: Execute a list of pre-defined tasks from a text file.
- **Queue System**: Ensures sequential command execution for stable output.
- **Interactive Mode**: Append commands to the processing queue during runtime.
- **Gemini CLI Integration**: utilizes the underlying Node.js-based Gemini CLI for all AI interactions.

## Prerequisites

Ensure the following dependencies are installed:

- **C++ Compiler** (C++20 compliant)
- **CMake** (Version 3.20 or later)
- **Node.js** (Required for the underlying Gemini CLI)
- **Git**

## Installation

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
- **Delimiters**: Commands must be enclosed between `GemStackSTART` and `GemStackEND`.
- **Formatting**: Delimiters can be on the same line or separate lines.
- **Newlines**: Multi-line text within a block is joined into a single command string; newlines are converted to spaces.

#### Configuration Examples

**Basic Prompt**
```text
GemStackSTART prompt "Tell me a joke about C++" GemStackEND
```

**Multi-line Prompt**
```text
GemStackSTART 
prompt "Refactor the following code to be cleaner:
int x = 5; if(x==5){return;}"
GemStackEND
```

**Using CLI Flags**
```text
GemStackSTART prompt "Explain quantum computing" --model gemini-1.5-pro --system "You are a physics professor" GemStackEND
```

**Batching Tasks**
```text
GemStackSTART prompt "Write a poem about rust" GemStackEND
GemStackSTART prompt "Convert that poem to python code" GemStackEND
GemStackSTART --help GemStackEND
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