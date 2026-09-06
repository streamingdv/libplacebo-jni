#!/usr/bin/env bash
# Guards the ABI floor of a built shared object.
#
# The project must run on Ubuntu 20.04 and newer, so no symbol may be versioned
# above what 20.04 ships. The floor is the requirement, not the build image:
# the Debian 10 container currently produces glibc 2.28, well under the limit.
set -euo pipefail

so=${1:?usage: check-linux-abi.sh <shared-object>}

declare -A limits=( [GLIBC]=2.31 [GLIBCXX]=3.4.28 [CXXABI]=1.3.12 )

echo "== $so =="
echo "Dynamic dependencies:"
objdump -p "$so" | awk '/NEEDED/ { print "  " $2 }'

echo "Required symbol versions:"
mapfile -t required < <(objdump -T "$so" \
    | grep -oE '\b(GLIBC|GLIBCXX|CXXABI)_[0-9][0-9.]*' \
    | sort -u -V)

status=0
for req in "${required[@]}"; do
    set_name=${req%%_*}
    version=${req#*_}
    limit=${limits[$set_name]:-}
    [ -n "$limit" ] || continue

    if [ "$(printf '%s\n%s\n' "$version" "$limit" | sort -V | head -n1)" = "$version" ]; then
        printf '  ok    %-16s (limit %s_%s)\n' "$req" "$set_name" "$limit"
    else
        printf '  FAIL  %-16s exceeds %s_%s\n' "$req" "$set_name" "$limit"
        status=1
    fi
done

if [ "$status" -ne 0 ]; then
    echo
    echo "This binary will not load on Ubuntu 20.04."
    echo "Build it inside the Debian 10 container (see .github/docker/Dockerfile-libplacebo-jni)."
fi

exit "$status"
