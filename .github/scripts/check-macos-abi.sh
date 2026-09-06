#!/usr/bin/env bash
# Guards the deployment target of a built dylib.
#
# The project must run on macOS 12.7, so LC_BUILD_VERSION minos may not exceed
# that. The floor is the requirement, not the build image: the workflow targets
# 11.0, which leaves headroom.
set -euo pipefail

dylib=${1:?usage: check-macos-abi.sh <dylib>}
limit=${2:-12.7}

echo "== $dylib =="
file "$dylib"

echo "Linked libraries:"
otool -L "$dylib" | tail -n +2 | sed 's/^/  /'

minos=$(vtool -show-build-version "$dylib" | awk '/minos/ { print $2; exit }')
if [ -z "$minos" ]; then
    echo "FAIL  could not read minos from LC_BUILD_VERSION" >&2
    exit 1
fi

if [ "$(printf '%s\n%s\n' "$minos" "$limit" | sort -V | head -n1)" = "$minos" ]; then
    echo "ok    minos $minos (limit $limit)"
else
    echo "FAIL  minos $minos exceeds $limit"
    echo
    echo "The dylib will not load on macOS $limit."
    echo "Check MACOSX_DEPLOYMENT_TARGET and -mmacosx-version-min in the workflow."
    exit 1
fi
