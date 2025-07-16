# RECAP - Extracts Context And Packages

## Purpose

`recap` is a command-line tool designed to capture the structure and content of a workspace and consolidate it into a single text output. This output can be redirected to a file or piped directly, making it ideal for providing context to AI models that need to understand the project's layout and relevant code.

## Features

*   **Powerful Filtering**: Use extended regular expressions (REGEX) to precisely include or exclude files and directories (`--include`, `--exclude`).
*   **Granular Content Control**: Specify exactly which files should have their content displayed using REGEX (`--include-content`), and which should not (`--exclude-content`).
*   **Flexible Content Stripping**: Automatically remove the leading portion of file content (like boilerplate or license headers) that matches a given regex. Use `--strip` for a global rule, or the more powerful `--strip-scope` to apply specific stripping rules only to files matching a given path pattern.
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
*   pcre2 (for regular expressions)

On Debian/Ubuntu systems, you can install these dependencies with:
```bash
sudo apt-get install libcurl4-openssl-dev libjansson-dev libpcre2-dev
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

## Examples

#### Basic Content Inclusion
Process the `src` and `doc` directories, showing content only for C, header, and markdown files.
```bash
recap src doc -I '\.(c|h|md)$'
```

#### Excluding Directories
Process the current directory, but exclude any path starting with `obj/` or `test/`.
```bash
recap -e '^(obj|test)/'
```

#### Stripping Boilerplate License Headers
Include content for all `.js` files, but use a scoped strip rule to remove the leading JSDoc-style comment block from each one.
```bash
recap -I '\.js$' -S '\.js$' '^\s*/\*\*.*?\*/'
```

#### Using Gitignore and Uploading to Gist
Process the project using `.gitignore` rules for exclusion and automatically upload the result to a private GitHub Gist.
```bash
recap -g --paste
```