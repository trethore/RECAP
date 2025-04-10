# RECAP - Extracts Context And Packages

## Purpose

`recap` is a command-line tool designed to capture the structure and content of a workspace and consolidate it into a single text output. This output can be redirected to a file or piped directly, making it ideal for providing context to AI models that need to understand the project's layout and relevant code.

## TODO

* [ ] Change `--paste` to check if an API key is provided in environment
* [ ] Add `--out` output the generated text in a file
* [ ] Add `--out-dir` output the generated text in a directory
* [X] Add a warning to `--clear` since it removes all ctf-output files
* [X] Remove `--name` no longer useful
* [X] Remove `--dir` no longer useful
* [ ] Add `--version`
* [ ] Remove Herobrine

## TOCONSIDER

* [ ] `--content` allow multiple options instead of custom quote separation logic (current logic is fine, but what if you want to target extensions containing quotes? it's a rabbit hole of escaping mechanisms. better let the shell handle it)
* [ ] add non-option argument for source directory(ies?): currently it's always `.`
* [ ] Rename `--out` to `--output` for consistency with `--output-dir`
* [ ] Allow custom .gitignore filename as `--git` option argument, or load it from parent directory recursively (until root) if not found. Also check if there's a library to interpret the full syntax (comment, !-lines...)

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
  git clone https://github.com/trethore/RECAP.git
  cd RECAP
  ```
  
2. Build the project:
  
  ```bash
  make
  ```
  
  This will compile the source files and create the `recap` executable.
  
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
