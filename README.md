# CTF (Context To File)

## Purpose

`ctf` is a command-line tool designed to capture the structure and content of a workspace (directory) and consolidate it into a single text file. This file can then be easily provided as context to AI models, allowing them to understand the project's layout and relevant code.

## Usage

```bash
./ctf [options]
```

By default, `ctf` traverses the current directory (`.`) and creates a timestamped output file (e.g., `ctf-output-YYYYMMDD-HHMMSS.txt`) in the current directory, listing the directory structure.

## Options

* `--help`, `-h`: Show help message and exit
* `--clear`: Delete previous `ctf-output` files in the current directory
* `--content`, `-c` `[ext1 ext2 ...]`: Include content of files with specified extensions (e.g., `c`, `h`, `py`). If no extensions are provided, includes all text files.
* `--include`, `-i` `[paths...]`: Include specific files or directories (default is current directory `.`)
* `--exclude`, `-e` `[paths...]`: Exclude specific files or directories
* `--git`, `-g`: Use `.gitignore` entries as exclude patterns
* `--dir`, `-d` `DIR`: Output directory (default is current directory)
* `--name`, `-n` `NAME`: Output filename (default is timestamped)
* `--paste`, `-p` `API_KEY`: Upload output as GitHub Gist using the provided API key

## Examples

1. **Basic structure dump:**

    ```bash
    ./ctf
    ```

2. **Include content of C/H files and specific directories, excluding build directory:**

    ```bash
    ./ctf -c c h -i src include tests -e build
    ```

3. **Use .gitignore, include Python content, save to a specific file:**

    ```bash
    ./ctf -g -c py -n my_project_context -d ./output
    ```

4. **Upload output to GitHub Gist:**

    ```bash
    ./ctf -c c h -p YOUR_GITHUB_TOKEN
    ```

5. **Clean up previous outputs:**

    ```bash
    ./ctf --clear
    ```

## Building the Project

### Dependencies

This project requires the following libraries:

* libcurl (for HTTP requests)
* jansson (for JSON parsing)

On Debian/Ubuntu systems, you can install these dependencies with:

```bash
sudo apt-get install libcurl4-openssl-dev libjansson-dev
```

### Build Instructions

1. Clone the repository:
  
  ```bash
  git clone https://github.com/trethore/Context-To-File
  cd Context-To-File
  ```
  
2. Build the project:
  
  ```bash
  make
  ```
  
  This will compile the source files and create the `ctf` executable.
  
3. To clean the build files:
  
  ```bash
  make clean
  ```

### Build Configuration

The project is compiled with the following flags:

* `-Wall -Wextra`: Enable all warnings
* `-std=c11`: Use C11 standard
* `-g`: Include debugging information

If you need to modify the compiler or flags, you can edit the variables at the top of the Makefile.
