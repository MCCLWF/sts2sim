#!/usr/bin/env bash
# Reject host-built libraries that cannot load in Steam Runtime 3 (glibc 2.31).
set -Eeuo pipefail
export LC_ALL=C
LIB="${1:?Pass libsts2nosl_pilot.so}"
header="$(readelf --file-header "$LIB")"
[[ $header == *ELF64* && $header == *X86-64* ]] || { echo 'Expected Linux x86_64 ELF.' >&2; exit 1; }
versions="$(readelf --version-info "$LIB" | sed -n 's/.*Name: \([^ ]*\).*/\1/p' | sort -u)"
while read -r version; do
    [[ -n $version ]] || continue
    [[ $version =~ ^GLIBC_[0-9]+\.[0-9]+(\.[0-9]+)?$ ]] || { echo "Unsupported ABI dependency: $version" >&2; exit 1; }
    newest="$(printf '%s\n' 2.31 "${version#GLIBC_}" | sort -V | tail -1)"
    [[ $newest == 2.31 ]] || { echo "Steam ABI too new: $version (maximum GLIBC_2.31)" >&2; exit 1; }
done <<< "$versions"
needed="$(readelf --dynamic "$LIB" | sed -n 's/.*(NEEDED).*\[\(.*\)\].*/\1/p')"
while read -r dependency; do
    case "$dependency" in
        libc.so.6|libm.so.6|libpthread.so.0|librt.so.1|libdl.so.2|ld-linux-x86-64.so.2) ;;
        *) echo "Unbundled native dependency: $dependency" >&2; exit 1 ;;
    esac
done <<< "$needed"
exports="$(nm --dynamic --defined-only --format=posix "$LIB" | awk '{print $1}')"
while read -r symbol; do
    [[ $symbol == nosl_* ]] || { echo "Non-C-ABI symbol exported: $symbol" >&2; exit 1; }
done <<< "$exports"
for symbol in nosl_actor_create nosl_actor_decide nosl_actor_destroy nosl_pilot_last_error nosl_id; do
    grep -Fxq "$symbol" <<< "$exports" || { echo "Missing C ABI: $symbol" >&2; exit 1; }
done
printf 'Steam ABI OK: maximum %s; dependencies: %s\n' "$(sort -V <<< "$versions" | tail -1)" "$(tr '\n' ' ' <<< "$needed")"
