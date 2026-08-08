#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BIN="$ROOT_DIR/src/roccat-vulcan"

if [ ! -x "$BIN" ]; then
  echo "binary not built: $BIN" >&2
  exit 1
fi

OUTPUT=$($BIN -h 2>&1 || true)

printf '%s
' "$OUTPUT" | grep -q 'By default' || {
  echo 'usage text missing' >&2
  exit 1
}
