# Token Reduction Plan for RECAP LLM Output

This document outlines language-agnostic strategies to reduce token count when RECAP output is consumed by Large Language Models.

**Design Principle:** RECAP remains content and language agnostic. No language-specific parsing or transformations.

---

## Current State

RECAP v2.0.3 already implements:

| Feature | Location | Effect |
|---------|----------|--------|
| `--compact` mode | `compact.c` | Language-aware comment removal |
| Blank line squashing | `traverse.c:126-149` | Collapses consecutive blank lines |
| `--strip` regex | `args.c`, `traverse.c` | Removes boilerplate headers |
| Content filtering | `-I`/`-E` flags | Controls which files show content |
| `.gitignore` respect | `-g` flag | Excludes build artifacts/deps |

---

## Proposed Improvements

### 1. Normalize Indentation (`--normalize-indent`)

Convert all tabs to 2 spaces. Simple, language-agnostic whitespace normalization.

**Current:** Mixed tabs and spaces, variable indent widths
```
function example() {
		if (condition) {		// tab indented
				doSomething();
		}
}
```

**After:**
```
function example() {
  if (condition) {
    doSomething();
  }
}
```

**Implementation:**
```c
// In traverse.c, during line output:
void normalize_line_indent(const char *line, FILE *output) {
    while (*line == '\t' || *line == ' ') {
        if (*line == '\t') {
            fputs("  ", output);  // tab -> 2 spaces
        } else {
            fputc(' ', output);
        }
        line++;
    }
    fputs(line, output);
}
```

**Token savings:** 10-30% for tab-heavy codebases

---

### 2. Separator Customization (`--separator [string]`)

**Current:** Files separated by `---\n`

**Proposed:**
- `--separator` (no arg) - Disable separator
- `--separator <string>` - Custom separator (e.g., `--separator "==="`)

**Implementation:**
```c
// In recap_context:
char *separator;      // Default "---", empty string "" if disabled, or custom

// In traverse.c print_output():
if (ctx->separator && ctx->separator[0] != '\0') {
    fprintf(ctx->output_stream, "%s\n", ctx->separator);
}
```

**Token savings:** ~3-4 tokens per file boundary

---

### 3. File Truncation (`--truncate N`)

Truncate files that exceed N tokens (default: 10000 tokens). Uses head truncation with `...` marker.

**Output format:**
```
path/to/large-file.js	1542	48KB
<first ~10000 tokens of content>
...
```

**Token estimation:** 1 token ≈ 4 characters (simple, language-agnostic)

**Implementation:**
```c
// In recap_context:
int truncate_tokens;  // Default 10000, 0 = disabled

// In traverse.c write_file_content_block():
#define CHARS_PER_TOKEN 4

void write_file_content_block(recap_context *ctx, const char *path) {
    // ... read file content ...
    
    size_t max_chars = ctx->truncate_tokens * CHARS_PER_TOKEN;
    size_t content_len = strlen(content);
    
    if (ctx->truncate_tokens > 0 && content_len > max_chars) {
        // Output first max_chars characters
        fwrite(content, 1, max_chars, ctx->output_stream);
        fprintf(ctx->output_stream, "\n...\n");
    } else {
        // Output full content
        fputs(content, ctx->output_stream);
    }
}
```

**CLI usage:**
```bash
recap src --truncate         # Default 10000 tokens
recap src --truncate 5000    # Custom limit
recap src --truncate 0       # Disable truncation
```

**Token savings:** Configurable, prevents large files from dominating context

---

### 4. Summary Metadata

Add line count and file size metadata next to each file path, tab-separated.

**Current:**
```
src/main.c:
<content>
---
src/args.c:
<content>
```

**Proposed:**
```
src/main.c	172	4.2KB
<content>
---
src/args.c	374	12KB
<content>
```

**Format:** `path\tlines\tsize`

**Implementation:**
```c
// In traverse.c write_file_content_block():
void write_file_header(recap_context *ctx, const char *path) {
    struct stat st;
    int line_count = 0;
    
    // Get file size
    stat(path, &st);
    
    // Count lines
    FILE *f = fopen(path, "r");
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        line_count++;
    }
    fclose(f);
    
    // Format size
    char size_str[32];
    if (st.st_size >= 1024 * 1024) {
        snprintf(size_str, sizeof(size_str), "%.1fMB", st.st_size / (1024.0 * 1024.0));
    } else if (st.st_size >= 1024) {
        snprintf(size_str, sizeof(size_str), "%.1fKB", st.st_size / 1024.0);
    } else {
        snprintf(size_str, sizeof(size_str), "%ldB", st.st_size);
    }
    
    // Output: path<TAB>lines<TAB>size
    fprintf(ctx->output_stream, "%s\t%d\t%s\n", path, line_count, size_str);
}
```

**Token overhead:** ~3-5 tokens per file (minimal)
**Benefit:** LLMs can understand file context without reading full content

---

## Implementation Plan

### Phase 1: Core Features

| Priority | Feature | Effort | Files to Modify |
|----------|---------|--------|-----------------|
| 1 | `--separator [str]` | Low | `args.c`, `traverse.c` |
| 3 | Summary metadata | Low | `traverse.c` |
| 4 | `--normalize-indent` | Medium | `traverse.c` |
| 5 | `--truncate N` | Medium | `args.c`, `traverse.c` |

### New Fields in `recap_context`

```c
typedef struct recap_context {
    // ... existing fields ...
    
    // Token optimization (new)
    char *separator;            // --separator [str], default "---", "" = disabled
    int normalize_indent;       // --normalize-indent flag
    int truncate_tokens;        // --truncate N, default 10000, 0 = disabled
} recap_context;
```

---

## Output Examples

### Default Output
```
src/main.c	172	4.2KB
#include <stdio.h>
#include "recap.h"

int main(int argc, char **argv) {
    // ...
}
---
src/utils.c	325	9.8KB
#include "utils.h"
// ...
```

### With `--no-separator --truncate 1000`
```
src/main.c	172	4.2KB
#include <stdio.h>
#include "recap.h"

int main(int argc, char **argv) {
    recap_context ctx = {0};
    // ... first ~1000 tokens ...
...
src/utils.c	325	9.8KB
#include "utils.h"
// ... first ~1000 tokens ...
...
```

### With `--normalize-indent`
All tabs converted to 2 spaces, reducing token waste from mixed/deep indentation.

---

## Estimated Token Savings

| Feature | Typical Savings | Notes |
|---------|-----------------|-------|
| `--separator` | 1-3% | ~4 tokens per file boundary |
| `--normalize-indent` | 10-30% | Significant for tab-heavy code |
| `--truncate 10000` | Variable | Prevents large files from dominating |
| Summary metadata | -1% (overhead) | Adds context for LLMs |

**Combined savings:** 10-35% depending on codebase characteristics.

---

## Testing

- Verify tab-to-space conversion preserves content semantics
- Test truncation boundary handling (mid-line, mid-word)
- Benchmark token counts with various LLM tokenizers
- Ensure metadata line count/size are accurate
