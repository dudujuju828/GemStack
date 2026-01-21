# GemStack

GemStack is a C++ automation wrapper for the [Gemini CLI](https://github.com/google-gemini/gemini-cli). It enables the execution of sequential prompts and tasks ("tickets") via a queue system.

The application allows users to define a series of commands in a file for batch processing, or to input commands interactively. This facilitates automated, high-volume interaction with the Gemini CLI.

**Note:** GemStack automatically runs Gemini in `--yolo` mode, meaning it will auto-approve tool calls without user confirmation.

## Features

- **Batch Processing**: Execute a list of pre-defined tasks from a text file.
- **Queue System**: Ensures sequential command execution for stable output.
- **Interactive Mode**: Append commands to the processing queue during runtime.
- **Reflective Mode**: AI continuously improves its work by generating its own follow-up prompts.
- **Prompt Blocks**: Organize related prompts into logical blocks with `PromptBlockSTART`/`PromptBlockEND`.
- **Specification Checkpoints**: Use `specify` to define expectations that are verified before each prompt executes.
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

#### Basic Syntax
- **Structure**: Enclose all commands within a single `GemStackSTART` and `GemStackEND` block.
- **Commands**: Each line inside the block is treated as a separate command to be executed.
- **Blank Lines**: Empty lines are ignored.

#### Basic Example

You can queue as many prompts or CLI arguments as needed within the block:

```text
GemStackSTART
prompt "Tell me a joke about C++"
prompt "Explain how shared_ptr works"
--version
--help
GemStackEND
```

---

### Prompt Blocks and the `specify` Directive

GemStack supports **Prompt Blocks** and **Specification Checkpoints** for more structured, reliable automation workflows.

#### Overview

When building complex projects with multiple sequential prompts, it's important to verify that each step completed correctly before proceeding. The `specify` directive allows you to define **expectations** that must be verified before the next prompt executes.

#### Syntax

| Directive | Description |
|-----------|-------------|
| `PromptBlockSTART` | Marks the beginning of a prompt block |
| `PromptBlockEND` | Marks the end of a prompt block |
| `goal "..."` | High-level objective for the block (one per block) |
| `specify "..."` | Defines an expectation to verify before the next prompt |
| `prompt "..."` | A task for the AI to execute |

#### How It Works

1. **Prompt Blocks**: Group related prompts together using `PromptBlockSTART` and `PromptBlockEND`. This provides logical organization and resets specification state between blocks.

2. **Goals**: Each prompt block can have one `goal "..."` statement that describes the high-level objective or final product for that block. The goal is prepended to **every** prompt in the block, giving the AI consistent context about what it's ultimately working towards.

3. **Specifications**: When you add `specify "..."` statements between prompts, they accumulate and are automatically prepended to the **next** prompt as verification checkpoints.

4. **Verification Checkpoints**: When a prompt has pending specifications, GemStack transforms it into a verification-first task:
   - The AI first checks if all specified expectations are met
   - If any expectation is NOT met, the AI fixes the issues first
   - Only after verification does the AI proceed with the actual task

#### Example with Goal and Specifications

```text
GemStackSTART

PromptBlockSTART
goal "A modern React website with responsive Header and Footer components using TypeScript and CSS modules"

prompt "Initialize a new React project with TypeScript called 'my-app'"

specify "The 'my-app' folder should exist with package.json containing React and TypeScript"
prompt "Create a Header component with a navigation menu"

specify "A Header component should exist in src/components/Header.tsx"
specify "The Header should include a navigation element with at least 3 links"
prompt "Add responsive styling using CSS modules"

specify "Header.module.css should exist with responsive breakpoints"
prompt "Create a Footer component that matches the Header styling"
PromptBlockEND

GemStackEND
```

#### What Happens Internally

When the above queue is processed, the **first** prompt (which has a goal but no specifications) is transformed into:

```
GOAL - The ultimate objective you are working towards:
  A modern React website with responsive Header and Footer components using TypeScript and CSS modules

CURRENT TASK:
Initialize a new React project with TypeScript called 'my-app'
```

The **second** prompt receives both the goal and a verification checkpoint:

```
GOAL - The ultimate objective you are working towards:
  A modern React website with responsive Header and Footer components using TypeScript and CSS modules

CHECKPOINT - Before proceeding, verify the following expectations are met.
If any are NOT correct, fix them first and explain what was missing:
  1. The 'my-app' folder should exist with package.json containing React and TypeScript

After verification is complete, proceed with the following task:
Create a Header component with a navigation menu
```

The **third** prompt receives the goal plus two verification checkpoints:

```
GOAL - The ultimate objective you are working towards:
  A modern React website with responsive Header and Footer components using TypeScript and CSS modules

CHECKPOINT - Before proceeding, verify the following expectations are met.
If any are NOT correct, fix them first and explain what was missing:
  1. A Header component should exist in src/components/Header.tsx
  2. The Header should include a navigation element with at least 3 links

After verification is complete, proceed with the following task:
Add responsive styling using CSS modules
```

#### Multiple Prompt Blocks

You can define multiple prompt blocks for different phases of a project, each with its own goal:

```text
GemStackSTART

PromptBlockSTART
goal "A fully configured project with CMake build system and organized folder structure"
prompt "Set up the project infrastructure"
specify "Project should have proper folder structure"
prompt "Configure the build system"
PromptBlockEND

PromptBlockSTART
goal "A working core module with comprehensive unit test coverage"
prompt "Implement the core features"
specify "Core module should be functional"
prompt "Add unit tests for core features"
PromptBlockEND

PromptBlockSTART
goal "Complete API documentation with auto-generated reference docs"
prompt "Add documentation"
specify "All public APIs should be documented"
prompt "Generate API reference docs"
PromptBlockEND

GemStackEND
```

#### Best Practices for `goal`

1. **Describe the Final Product**: Focus on what should exist at the end of the block
   - Good: `goal "A responsive dashboard with real-time data visualization and user authentication"`
   - Vague: `goal "Make a good website"`

2. **Keep It High-Level**: The goal provides context, not step-by-step instructions
   - Good: `goal "A RESTful API with CRUD operations for users, posts, and comments"`
   - Too detailed: `goal "Create routes at /users, /posts, /comments with GET, POST, PUT, DELETE methods..."`

3. **One Goal Per Block**: If you need different goals, use separate prompt blocks

#### Best Practices for `specify`

1. **Be Specific**: Write clear, verifiable expectations
   - Good: `specify "The UserService class should have a 'login' method that returns a Promise<User>"`
   - Vague: `specify "The code should work"`

2. **Keep It Focused**: Each `specify` should check one thing
   - Good: Use multiple `specify` statements for multiple checks
   - Avoid: Combining many checks into one long specification

3. **Check Artifacts**: Verify files, functions, or structures exist
   - `specify "package.json should include 'jest' as a devDependency"`
   - `specify "The API endpoint /users should return a JSON array"`

4. **Validate Behavior**: Describe expected functionality
   - `specify "The login form should display an error message for invalid credentials"`
   - `specify "The responsive layout should switch to single column below 768px"`

#### Warnings and Edge Cases

- **Multiple Goals**: If you define more than one `goal` in a block, GemStack will warn you and use the last one
- **Unused Specifications**: If `specify` statements appear at the end of a block (with no following prompt), GemStack will warn you
- **Block Boundaries**: Goals and specifications don't carry across `PromptBlockEND` boundaries—they reset at each block start
- **Backwards Compatibility**: If you don't use `PromptBlockSTART`/`PromptBlockEND`, prompts work exactly as before
- **Goal Without Prompts**: A `goal` without any `prompt` in the block has no effect

---

### Generating a GemStackQueue with AI

You can use any AI assistant (ChatGPT, Claude, Gemini, etc.) to generate a complete `GemStackQueue.txt` file for your project. Simply copy the boilerplate prompt below, fill in your project description, and paste it into your preferred AI.

#### Boilerplate Prompt

Copy and paste this prompt into any AI, replacing `[YOUR PROJECT DESCRIPTION]` with your specific requirements:

```
I need you to generate a GemStackQueue.txt file for the following project:

[YOUR PROJECT DESCRIPTION]

GemStack is an automation tool that executes sequential AI prompts. Generate a complete queue file using this format:

SYNTAX RULES:
- Wrap everything in GemStackSTART and GemStackEND
- Use PromptBlockSTART/PromptBlockEND to group related phases
- Use goal "..." once per block to describe the high-level objective
- Use prompt "..." for tasks the AI should execute
- Use specify "..." BETWEEN prompts to define checkpoints that verify the previous work before proceeding

STRUCTURE:
1. Each PromptBlock should have one goal describing the final product for that phase
2. Each prompt should be a single, focused task
3. After each prompt, add 1-3 specify statements describing what should exist/be true after that prompt completes
4. The goal and specify statements will be prepended to each prompt as context and verification checkpoints
5. Break the project into logical phases using PromptBlockSTART/PromptBlockEND

EXAMPLE OUTPUT FORMAT:
GemStackSTART

PromptBlockSTART
goal "Description of what this phase should ultimately produce"
prompt "First task description"

specify "Expected outcome 1 from first task"
specify "Expected outcome 2 from first task"
prompt "Second task that builds on the first"

specify "What should exist after second task"
prompt "Third task"
PromptBlockEND

PromptBlockSTART
goal "Description of next phase's final product"
prompt "Next phase task"

specify "Verification for next phase"
prompt "Final task in this phase"
PromptBlockEND

GemStackEND

GUIDELINES:
- Be specific in prompts - include file names, technologies, and exact requirements
- Specify statements should be verifiable (file exists, function works, component renders)
- Start with project setup/initialization
- Progress logically: setup → core features → styling → testing → polish
- Each PromptBlock should represent a distinct phase (e.g., "Backend Setup", "Frontend Components", "Testing")

Now generate the complete GemStackQueue.txt for my project.
```

#### Example: Portfolio Website

**Input to AI:**
```
I need you to generate a GemStackQueue.txt file for the following project:

A modern portfolio website for a software developer. Should include:
- Next.js with TypeScript and Tailwind CSS
- Home page with hero section and brief intro
- Projects section with filterable grid
- About page with skills and experience
- Contact form with validation
- Dark mode toggle
- Responsive design

[rest of boilerplate prompt...]
```

**AI generates a complete GemStackQueue.txt** with proper prompts, specify checkpoints, and logical phase organization.

#### Tips for Better Results

1. **Be Detailed**: The more specific your project description, the better the generated queue
2. **Mention Technologies**: Specify frameworks, languages, and tools you want to use
3. **List Features**: Enumerate all features you want in the final product
4. **Include Constraints**: Mention any specific requirements (accessibility, performance, etc.)
5. **Iterate**: Run the generated queue, then ask the AI to generate additional phases if needed

---

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
| `--config <path>` | Load configuration from specified file path |
| `--auto-commit` | Force enable auto-commit for this run |
| `--no-auto-commit` | Force disable auto-commit for this run |
| `--commit-prefix <text>` | Override commit message prefix for this run |
| `--commit-include-prompt <bool>` | Include prompt summary in commits (`true`/`false`) |
| `--help` | Display help message |

---

### Configuration: GemStackConfig.txt

GemStack supports an optional configuration file (`GemStackConfig.txt`) that allows you to customize behavior. This file is **not required** to run GemStack—if it doesn't exist, defaults will be used.

Place `GemStackConfig.txt` in the same directory as the executable.

#### Configuration Format

The configuration file uses a simple `key=value` format. Comments start with `#` or `;`.

```ini
# GemStack Configuration File
# All settings are optional - defaults are used if not specified

# Auto-commit settings
autoCommitEnabled=true
autoCommitMessagePrefix=[GemStack]
autoCommitIncludePrompt=true
```

#### Available Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `autoCommitEnabled` | `false` | Enable automatic git commits after each prompt |
| `autoCommitMessagePrefix` | `[GemStack]` | Prefix for auto-commit messages |
| `autoCommitIncludePrompt` | `true` | Include a summary of the prompt in commit messages |

**Alternative key formats:** You can also use snake_case versions (e.g., `auto_commit_enabled`).

#### Precedence

Settings can come from multiple sources. GemStack applies them in this order (highest priority first):

1. **CLI flags** (`--auto-commit`, `--commit-prefix`, etc.) — always win
2. **Config file** (`GemStackConfig.txt` or path specified via `--config`)
3. **Defaults** — auto-commit disabled, prefix `[GemStack]`, include prompt `true`

This means you can have auto-commit disabled in your config file but override it for a single run with `--auto-commit`.

---

### Auto-Commit Feature

The auto-commit feature creates a git commit after each successful prompt execution. This provides a traceable history of all AI-driven changes, which is valuable for high-autonomy automation workflows.

#### Why Use Auto-Commit?

- **Traceability**: Every AI action is recorded as a separate commit
- **Rollback**: Easily revert specific changes if needed
- **Audit Trail**: Clear history of what the AI did at each step
- **Debugging**: Identify which prompt introduced a bug or unwanted change

#### Enabling Auto-Commit

**Option 1: Via config file**

Create a `GemStackConfig.txt` file with:

```ini
autoCommitEnabled=true
```

**Option 2: Via CLI flag (no config file needed)**

```powershell
.\GemStack.exe --auto-commit
```

**Option 3: Custom config file path**

```powershell
.\GemStack.exe --config ./configs/production.txt
```

#### Example Workflow

1. Create your configuration file:
   ```ini
   # GemStackConfig.txt
   autoCommitEnabled=true
   autoCommitMessagePrefix=[GemStack]
   autoCommitIncludePrompt=true
   ```

2. Run GemStack with your prompts:
   ```powershell
   .\GemStack.exe
   ```

3. After each prompt, if changes were made, a commit is automatically created:
   ```
   [GemStack] Auto-committing changes...
   [GemStack] Auto-commit successful: [GemStack] Create a Header component with navigation
   ```

4. View your commit history:
   ```bash
   git log --oneline
   # a1b2c3d [GemStack] Create a Header component with navigation
   # e4f5g6h [GemStack] Initialize a new React project with TypeScript
   ```

#### Commit Message Format

Auto-commit messages follow this format:
```
<prefix> <prompt summary>
```

For example:
- `[GemStack] Create a login form with validation`
- `[GemStack] Add unit tests for the UserService class`
- `[GemStack] Fix the responsive layout for mobile devices`

#### CLI Override Examples

**Enable auto-commit for a single run (config file has it disabled):**
```powershell
.\GemStack.exe --auto-commit --reflect "Build a calculator"
```

**Disable auto-commit for a single run (config file has it enabled):**
```powershell
.\GemStack.exe --no-auto-commit
```

**Custom prefix and include prompt setting:**
```powershell
.\GemStack.exe --auto-commit --commit-prefix "[AI-Build]" --commit-include-prompt false
```

#### Commit Message Hygiene

Auto-commit messages are sanitized to ensure clean git history:

- **Single-line subject**: Newlines and tabs in prompt summaries are replaced with spaces
- **Whitespace normalization**: Multiple consecutive spaces are collapsed to one
- **Length cap**: Subject line is capped at 72 characters (including prefix); longer summaries are truncated with `...`

Example transformation:
- Prompt: `"Create a\nmulti-line\tdescription with   extra spaces"`
- Commit message: `[GemStack] Create a multi-line description with extra sp...`

#### Behavior Notes

- **No changes detected**: If a prompt doesn't result in file changes, no commit is created
- **Non-git directories**: Auto-commit is silently skipped if not in a git repository
- **Failed prompts**: No commit is created for failed prompt executions
- **Reflective mode**: Each iteration in reflective mode creates its own commit
- **CLI precedence**: `--auto-commit` and `--no-auto-commit` always override config file settings
