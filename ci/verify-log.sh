#!/bin/bash
# Fail if the build log contains any compiler/linker warning or error.
# Used as the final gate after ci/build.sh; the build itself runs with the
# kernel's usual -w for target code, so a hit here means a host tool, script,
# modpost or linker problem that must be fixed.
set -uo pipefail

log="${1:-build.log}"
if [ ! -f "$log" ]; then
    echo "missing build log: $log"
    exit 1
fi

bad=$(grep -inE '(warning|error):|Syntax error|undefined reference|No rule to make target' "$log" || true)
if [ -n "$bad" ]; then
    echo "BUILD_LOG_NOT_CLEAN:"
    printf '%s\n' "$bad" | head -60
    exit 1
fi
echo "BUILD_LOG_CLEAN: no warnings or errors"
