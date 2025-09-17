#!/usr/bin/env bash
set -euo pipefail

# Integration test runner for RECAP
# Place at: test/run-integration-tests.sh

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RECAP_BIN="${RECAP_BIN:-$REPO_ROOT/recap}"
TMPROOT="$(mktemp -d /tmp/recap-test.XXXXXX)"
trap 'rm -rf "$TMPROOT"' EXIT

# Helpers to capture binary output/rc while keeping set -e
LAST_OUT=""
LAST_RC=0

build_if_needed() {
  if [ ! -x "$RECAP_BIN" ]; then
    echo "Building recap (needs pcre2, libcurl, jansson dev headers)..."
    (cd "$REPO_ROOT" && make) || { echo "Build failed"; exit 2; }
  fi
}

run_cmd() {
  local cwd="$1"; shift
  pushd "$cwd" >/dev/null
  set +e
  LAST_OUT="$("$RECAP_BIN" "$@" 2>&1)"
  LAST_RC=$?
  set -e
  popd >/dev/null
  # Prevent 'set -e' from terminating the whole script when the binary returns non-zero.
  # Tests inspect LAST_RC directly, so always return success here.
  return 0
}

run_cmd_input() {
  local cwd="$1"; local input="$2"; shift 2
  pushd "$cwd" >/dev/null
  set +e
  LAST_OUT="$(printf '%s' "$input" | "$RECAP_BIN" "$@" 2>&1)"
  LAST_RC=$?
  set -e
  popd >/dev/null
  # Prevent 'set -e' from terminating the whole script when the binary returns non-zero.
  # Tests inspect LAST_RC directly, so always return success here.
  return 0
}

TOTAL=0; FAIL=0
assert_rc() {
  TOTAL=$((TOTAL+1))
  local expected=$1
  if [ "$LAST_RC" -ne "$expected" ]; then
    echo "FAIL ($TEST_NAME): expected rc $expected, got $LAST_RC"
    echo "---- output ----"
    echo "$LAST_OUT"
    FAIL=$((FAIL+1))
  else
    echo "OK  ($TEST_NAME): rc == $expected"
  fi
}
assert_out_contains() {
  TOTAL=$((TOTAL+1))
  local needle="$1"
  if ! printf '%s' "$LAST_OUT" | grep -q -- "$needle"; then
    echo "FAIL ($TEST_NAME): output missing: $needle"
    echo "---- output ----"
    echo "$LAST_OUT"
    FAIL=$((FAIL+1))
  else
    echo "OK  ($TEST_NAME): contains: $needle"
  fi
}
assert_out_not_contains() {
  TOTAL=$((TOTAL+1))
  local needle="$1"
  if printf '%s' "$LAST_OUT" | grep -q -- "$needle"; then
    echo "FAIL ($TEST_NAME): output SHOULD NOT contain: $needle"
    echo "---- output ----"
    echo "$LAST_OUT"
    FAIL=$((FAIL+1))
  else
    echo "OK  ($TEST_NAME): does not contain: $needle"
  fi
}
assert_path_not_shown_with_colon() {
  TOTAL=$((TOTAL+1))
  local path="$1"
  if printf '%s' "$LAST_OUT" | grep -q -- "$path:"; then
    echo "FAIL ($TEST_NAME): path shown with colon (indicates content shown) but expected not to: $path"
    echo "---- output ----"
    echo "$LAST_OUT"
    FAIL=$((FAIL+1))
  else
    echo "OK  ($TEST_NAME): path not shown with colon: $path"
  fi
}

# Prepare workspace
build_if_needed
cp -r "$REPO_ROOT/test" "$TMPROOT/test"

# Tests start here

TEST_NAME="help"
run_cmd "$TMPROOT" -h
assert_rc 0
assert_out_contains "Usage: recap"

TEST_NAME="version"
run_cmd "$TMPROOT" -v
assert_rc 0
assert_out_contains "recap version"

TEST_NAME="list-basic"
run_cmd "$TMPROOT" test
assert_rc 0
assert_out_contains "test/folder3/test.c"

TEST_NAME="include-regex"
run_cmd "$TMPROOT" -i '\.c$' test
assert_rc 0
assert_out_contains "test/folder3/test.c"
assert_out_not_contains "test/folder1/main.js"

TEST_NAME="include-content"
run_cmd "$TMPROOT" -I '\.c$' test
assert_rc 0
assert_out_contains "test/folder3/test.c:"
assert_out_contains "This is a test"

TEST_NAME="exclude-content"
run_cmd "$TMPROOT" -I '\.(js|ts)$' -E '\.js$' test
assert_rc 0
assert_out_contains "test/folder2/index.ts:"
assert_out_contains "toto"
assert_out_contains "test/folder1/main.js"
TEST_NAME="exclude-content-check-mainjs-no-colon"
assert_path_not_shown_with_colon "test/folder1/main.js"

TEST_NAME="strip-global"
# use (?s) to enable DOTALL so the dot matches newlines
run_cmd "$TMPROOT" -I '\.js$' -s '(?s)^\s*/\*\*.*?\*/\s*' test
assert_rc 0
assert_out_not_contains "Super cool JavaScript file"

TEST_NAME="strip-scope"
run_cmd "$TMPROOT" -I '\.js$' -S '\.js$' '(?s)^\s*/\*\*.*?\*/\s*' test
assert_rc 0
assert_out_not_contains "Super cool JavaScript file"

TEST_NAME="gitignore"
echo "folder2/" > "$TMPROOT/.gitignore"
run_cmd "$TMPROOT" --git test
assert_rc 0
assert_out_not_contains "test/folder2/index.ts"

TEST_NAME="compact"
run_cmd "$TMPROOT" --compact -I '\.(js|c|ts|json)$' test
assert_rc 0
assert_out_not_contains "Super cool JavaScript file"

TEST_NAME="output-file"
run_cmd "$TMPROOT" -o "my-output.txt" test
assert_rc 0
if [ -f "$TMPROOT/my-output.txt" ]; then
  echo "OK  (output-file): file created my-output.txt"
else
  echo "FAIL (output-file): my-output.txt not created"
  FAIL=$((FAIL+1))
fi

TEST_NAME="output-dir"
run_cmd "$TMPROOT" -O "outdir" test
assert_rc 0
if compgen -G "$TMPROOT/outdir/recap-output-*.txt" >/dev/null; then
  echo "OK  (output-dir): generated file placed in outdir"
else
  echo "FAIL (output-dir): no recap-output-*.txt in outdir"
  FAIL=$((FAIL+1))
fi

TEST_NAME="paste-no-key"
run_cmd "$TMPROOT" --paste -o pasted.txt test
assert_rc 0
assert_out_contains "Error: Gist upload requested, but no API key found."

TEST_NAME="paste-with-key-stdout"
run_cmd "$TMPROOT" --paste=FAKEKEY test
assert_rc 0
assert_out_contains "Warning: Cannot upload to Gist when outputting to stdout."

TEST_NAME="strip-scope-missing-args"
run_cmd "$TMPROOT" --strip-scope
# parse_arguments prints error and exit(1)
assert_rc 1
assert_out_contains "Error: --strip-scope requires two arguments"

TEST_NAME="clear"
# create a recap-output file and confirm clear removes it (answers 'y')
touch "$TMPROOT/recap-output-old.txt"
run_cmd_input "$TMPROOT" "y\n" --clear
assert_rc 0
if [ -f "$TMPROOT/recap-output-old.txt" ]; then
  echo "FAIL (clear): recap-output-old.txt still exists"
  FAIL=$((FAIL+1))
else
  echo "OK  (clear): recap-output-old.txt removed"
fi

# Summary

echo "-------------------------------------------------"
echo "Ran $TOTAL assertions. Failures: $FAIL"
if [ "$FAIL" -eq 0 ]; then
  echo "All tests passed."
else
  echo "Some tests failed."
fi
exit $FAIL
