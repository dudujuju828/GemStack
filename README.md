# GemStack 💎🥞

**The Ultimate Vibe-Coding Tool**

GemStack is a powerful automation wrapper built on top of [Gemini CLI](https://github.com/google-gemini/gemini-cli). It allows you to queue up a series of complex prompts and tasks ("tickets") that will be executed sequentially. 

Set up your queue, run GemStack, and walk away. Come back hours later to find all your tasks completed. It's the ultimate tool for asynchronous, high-volume AI interaction.

## 🚀 Features

- **Batch Processing**: Define a list of tasks in a text file and let GemStack churn through them one by one.
- **Queue System**: Commands are processed sequentially to ensure stability and orderly output.
- **Interactive Mode**: Add new commands to the queue on the fly while the stack is already processing.
- **Built on Gemini CLI**: Leverages the full power of the Gemini CLI for robust AI interactions.

## 📋 Prerequisites

Before you start, ensure you have the following installed:

- **C++ Compiler** (supporting C++17 or later)
- **CMake** (3.10+)
- **Node.js** (Required to run the underlying Gemini CLI)
- **Git**

## 🛠️ Installation & Setup

1.  **Clone the Repository**
    Make sure to clone with submodules to get the Gemini CLI:
    ```bash
    git clone --recurse-submodules https://github.com/yourusername/GemStack.git
    cd GemStack
    ```
    *If you already cloned without submodules, run:*
    ```bash
    git submodule update --init --recursive
    ```

2.  **Setup Gemini CLI**
    GemStack relies on the nested `gemini-cli`. You need to install its dependencies:
    ```bash
    cd gemini-cli
    npm install
    npm run build # Build the CLI packages
    cd ..
    ```

3.  **Build GemStack**
    Use CMake to build the C++ application:
    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```

## 🎮 Usage

### 1. The Queue File (`GemStackQueue.txt`)

Create a file named `GemStackQueue.txt` in the root directory (next to the executable). This file contains the prompts you want to run.

**Syntax:**
Wrap your commands between `GemStackSTART` and `GemStackEND`.

**Single Line Example:**
```text
GemStackSTART --help GemStackEND
```

**Multi-Line Example (Great for long prompts):**
```text
GemStackSTART
Write a comprehensive guide on how to bake sourdough bread, 
including details on maintaining a starter and baking temperatures.
GemStackEND
```

**Batch Example:**
```text
GemStackSTART prompt "Refactor main.cpp to be more thread-safe" GemStackEND
GemStackSTART prompt "Write a unit test for the queue system" GemStackEND
```

### 2. Running GemStack

Run the built executable from the root directory so it can find the `gemini-cli` script and your queue file.

**Windows:**
```powershell
.\build\Debug\GemStack.exe
```

**Linux/macOS:**
```bash
./build/GemStack
```

### 3. Interactive Mode

Once running, GemStack will process everything in `GemStackQueue.txt`. You can also manually type commands into the console window to add them to the end of the queue.

Type `exit` or `quit` to finish processing the queue and close the application.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
