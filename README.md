# CTF (Context To File)

## Purpose

`ctf` is a command-line tool designed to capture the structure and content of a workspace (directory) and consolidate it into a single text file. This file can then be easily provided as context to AI models, allowing them to understand the project's layout and relevant code.

## Usage

```bash
./ctf [options]
```

By default, `ctf` traverses the current directory (`.`) and creates a timestamped output file (e.g., `ctf-output-YYYYMMDD-HHMMSS.txt`) in the current directory, listing the directory structure.

## Options

*   `--content [ext1 ext2 ...]`: Includes the content of files with the specified extensions (e.g., `c`, `h`, `py`). If no extensions are provided, it includes content only for files explicitly defined in its internal exception list (like `Dockerfile`). Files without extensions are generally excluded unless they are in the exception list.
*   `--addf <dir1> [dir2 ...]`: Specifies directories or files to include in the traversal. Default is the current directory (`.`).
*   `--rmf <dir1> [dir2 ...]`: Specifies directories or files to exclude from the traversal.
*   `--git`: Automatically excludes files and directories listed in the `.gitignore` file found in the current directory. These exclusions are added to any specified via `--rmf`.
*   `--dir <output_directory>`: Specifies the directory where the output file should be saved. Default is the current directory (`.`).
*   `--name <output_filename>`: Specifies a custom name for the output file (without the `.txt` extension). If not provided, a timestamped name is generated.
*   `--paste <api_key>`: Uploads the output to Pastebin using the provided developer API key instead of saving it locally. Prints the Pastebin URL to the console.
*   `--clear`: Deletes all files in the current directory starting with `ctf-output`. Use with caution.

## Examples

1.  **Basic structure dump:**
    ```bash
    ./ctf
    ```
    (Outputs structure to `ctf-output-....txt`)

2.  **Include content of C/H files and specific directories, excluding build files:**
    ```bash
    ./ctf --content c h --addf src include tests --rmf build
    ```

3.  **Use .gitignore, include Python content, save to a specific file:**
    ```bash
    ./ctf --git --content py --name my_project_context --dir ./output
    ```

4.  **Include Dockerfile content (even without specifying `--content`):**
    ```bash
    ./ctf --addf . # Assuming Dockerfile is in the root
    ```
    (If `--content` *is* used with specific extensions, Dockerfile content will still be included due to the exception list).

5.  **Upload output to Pastebin:**
    ```bash
    ./ctf --content c h --paste YOUR_PASTEBIN_API_KEY
    ```
    (Outputs the structure and C/H file content, uploads it to Pastebin, and prints the link.)

6.  **Clean up previous outputs:**
    ```bash
    ./ctf --clear
    ```
