# RECAP - Extracts Context And Packages

A versatile command-line tool for capturing and packaging project context, designed for developers, teams, and AI assistants.

---

## What is RECAP?

Providing comprehensive project context to Large Language Models (LLMs), new team members, or for documentation can be a tedious process of copying and pasting files. `recap` automates this by intelligently traversing your project directory, filtering files, and consolidating their structure and content into a single, clean text output.

It's a powerful utility for anyone who needs to quickly package a project's source code for review, analysis, or as context for generative AI.

## Key Features

*   **Intelligent Filtering**: Use powerful regular expressions (REGEX) to include or exclude specific files and directories.
*   **Git Integration**: Automatically respects your `.gitignore` files to exclude irrelevant content, ensuring a clean output (`--git`).
*   **Precise Content Control**: Decide exactly which files should have their content displayed (`--include-content`) and which should only be listed by path.
*   **Header Stripping**: Automatically remove boilerplate like license headers or comment blocks from file content using regex, on a global (`--strip`) or per-file-type basis (`--strip-scope`).
*   **Content Compaction**: Optionally removes comments and redundant whitespace from file content to create a denser, token-efficient output for LLMs (`--compact`).
*   **Versatile Output Modes**:
    *   Print to **stdout** to pipe into other commands.
    *   Save to a named file (`--output`).
    *   Save to a timestamped file (`--output-dir`).
    *   Copy directly to the system **clipboard** (`--clipboard`).
    *   Upload to a private GitHub **Gist** in one command (`--paste`).
*   **Cross-Platform**: Works on Linux, macOS, and Windows.

## Installation

### 1. Prerequisites

#### Build Dependencies
You need `make` and a C compiler, plus the development headers for the following libraries:
*   **pcre2** (for regular expressions)
*   **libcurl** (for Gist uploads)
*   **jansson** (for JSON parsing for Gist uploads)

**On Debian / Ubuntu:**
```bash
sudo apt-get install build-essential libpcre2-dev libcurl4-openssl-dev libjansson-dev
```

**On macOS (using Homebrew):**
```bash
brew install pcre2 curl jansson
```

#### Runtime Dependencies (Optional)
For the clipboard feature (`-c`, `--clipboard`), `recap` relies on standard system utilities.

*   **Linux**: `xclip` (for X11) or `wl-clipboard` (for Wayland).
    ```bash
    # For X11-based systems
    sudo apt-get install xclip
    # For Wayland-based systems
    sudo apt-get install wl-clipboard
    ```
*   **macOS / Windows**: The necessary utilities (`pbcopy` and `clip.exe`) are pre-installed.

### 2. Building from Source

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/trethore/RECAP.git
    cd RECAP
    ```

2.  **Compile the project:**
    ```bash
    make
    ```
    This will create the `recap` executable in the current directory. You can move this executable to a directory in your system's `PATH` (e.g., `/usr/local/bin`) for global access.

## Usage Examples

#### Basic: List all files, showing content for source and markdown
```bash
# Process src and docs, showing content for c, h, and md files
recap src docs --include-content '\.(c|h|md)$'
```

#### Filtering: Use `.gitignore` and exclude test directories
```bash
# Use gitignore rules but also explicitly exclude any 'test' directories
recap --git --exclude '/test/'
```

#### Clipboard: Get all Python files and copy to clipboard
This is perfect for quickly providing context to an AI.
```bash
# Find all Python files, respect .gitignore, and copy the result
recap --git --include-content '\.py$' --clipboard
```

#### Advanced: Strip license headers from JS files and upload to Gist
A powerful one-liner to package and share code.
```bash
# 1. Target only .js files.
# 2. For those files, find and strip a leading JSDoc block.
# 3. Upload the final result to a private Gist.
recap -I '\.js$' -S '\.js$' '^\s*/\*\*.*?\*/\s*' --paste
```

#### Maintenance: Clean up old outputs
```bash
# Remove all recap-output-*.txt files from the current directory
recap --clear
```
## Contributing

Contributions are welcome and greatly appreciated! Whether it's reporting a bug, proposing a new feature, or submitting a code change, your help is valuable.

### Reporting Bugs or Requesting Features

The best way to report a bug or request a new feature is to [open an issue](https://github.com/trethore/RECAP/issues) on GitHub.

*   **For Bug Reports**: Please include your operating system, the command you ran, the output you received, and what you expected to happen.
*   **For Feature Requests**: Please provide a clear description of the feature you'd like to see and why it would be useful.

### Submitting Changes (Pull Requests)

If you'd like to contribute code, please follow these steps:

1.  **Fork the repository** on GitHub.
2.  **Create a new branch** for your feature or bug fix:
    ```bash
    git checkout -b feature/my-new-feature
    ```
3.  **Make your changes**. Please try to follow the existing coding style to maintain consistency.
4.  **Test your changes** to ensure they work as expected and don't introduce new issues.
5.  **Commit your changes** with a clear and descriptive commit message:
    ```bash
    git commit -m "feat: Add support for XYZ"
    ```
6.  **Push your branch** to your fork:
    ```bash
    git push origin feature/my-new-feature
    ```
7.  **Open a Pull Request** from your branch to the `main` branch of the original repository.

Thank you for your interest in improving `recap`