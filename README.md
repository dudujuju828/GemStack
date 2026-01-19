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

### 📝 Guide: Writing GemStackQueue.txt

The `GemStackQueue.txt` file is the heart of GemStack. It tells the tool what to do. Place this file in the same directory where you run the executable.

#### Syntax Rules
1.  **Delimiters**: Every command must be enclosed between `GemStackSTART` and `GemStackEND`.
2.  **Separation**: You can place `GemStackSTART` and `GemStackEND` on the same line or separate lines.
3.  **Newlines**: Text spanning multiple lines will be joined into a single command string (newlines become spaces).

#### Examples

**1. Basic Prompt**
Simple one-line command to ask Gemini something.
```text
GemStackSTART prompt "Tell me a joke about C++" GemStackEND
```

**2. Multi-line Prompt (Cleaner formatting)**
Use multiple lines for readability. GemStack will join them with spaces before sending to the CLI.
```text
GemStackSTART 
prompt "Refactor the following code to be cleaner:
int x = 5; if(x==5){return;}"
GemStackEND
```

**3. Using CLI Flags**
You can pass any standard Gemini CLI flags inside the block.
```text
GemStackSTART prompt "Explain quantum computing" --model gemini-1.5-pro --system "You are a physics professor" GemStackEND
```

**4. Batching Multiple Tasks**
Stack as many tasks as you like. They run one after another.
```text
GemStackSTART prompt "Write a poem about rust" GemStackEND
GemStackSTART prompt "Convert that poem to python code" GemStackEND
GemStackSTART --help GemStackEND
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
