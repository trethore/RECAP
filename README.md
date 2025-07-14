# RECAP - Extracts Context And Packages

## Purpose

`recap` is a command-line tool designed to capture the structure and content of a workspace and consolidate it into a single text output. This output can be redirected to a file or piped directly, making it ideal for providing context to AI models that need to understand the project's layout and relevant code.

## Features

*   **Powerful Filtering**: Use extended regular expressions (REGEX) to precisely include or exclude files and directories (`--include`, `--exclude`).
*   **Granular Content Control**: Specify exactly which files should have their content displayed using REGEX (`--include-content`), and which should not (`--exclude-content`).
*   **Flexible Content Stripping**: Automatically remove boilerplate from file content. Use `--strip` for a global rule, or the more powerful `--strip-scope` to apply specific stripping rules only to files matching a given path pattern.
*   **Git Integration**: Automatically uses patterns from `.gitignore` files for exclusion. It searches for the gitignore file from the current directory upwards to the root (`--git`).
*   **Flexible Traversal**: Specify one or more starting directories or files for the tool to process.
*   **Versatile Output**:
    *   Output to `stdout` (default).
    *   Save to a specific file (`--output`).
    *   Save to a timestamped file in a specific directory (`--output-dir`).
*   **One-Command Sharing**: Upload the generated output directly to a private GitHub Gist with the `--paste` option.
*   **Maintenance**: Utility option to clean up previously generated `recap-output*` files (`--clear`).

## Building the Project

### Dependencies

This project requires the following libraries:

*   libcurl (for HTTP requests)
*   jansson (for JSON parsing)

On Debian/Ubuntu systems, you can install these dependencies with:

```bash
sudo apt-get install libcurl4-openssl-dev libjansson-dev
```

### Build Instructions

1.  Clone the repository:
    ```bash
    git clone https://github.com/trethore/RECAP.git
    cd RECAP
    ```

2.  Build the project:
    ```bash
    make
    ```
    This will compile the source files and create the `recap` executable.

3.  To clean the build files:
    ```bash
    make clean
    ```

### Build Configuration

The project is compiled with the following flags:

*   `-Wall -Wextra`: Enable all warnings
*   `-std=gnu11`: Use GNU C11 standard
*   `-g`: Include debugging information
*   `-D_POSIX_C_SOURCE=200809L`

If you need to modify the compiler or flags, you can edit the variables at the top of the Makefile.