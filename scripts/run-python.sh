#!/usr/bin/env bash
# Source from repository hooks. bootstrap records the current interpreter locally.
harness_python() {
  local interpreter
  interpreter="$(git config --get harness.python || true)"
  if [ -n "$interpreter" ]; then
    "$interpreter" "$@"
  elif command -v python3 >/dev/null 2>&1; then
    python3 "$@"
  elif command -v python >/dev/null 2>&1; then
    python "$@"
  elif command -v py >/dev/null 2>&1; then
    py -3 "$@"
  else
    echo 'Python 3.11+ is required; run scripts/bootstrap.py with your Python interpreter.' >&2
    return 1
  fi
}
