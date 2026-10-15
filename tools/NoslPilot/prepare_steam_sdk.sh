#!/usr/bin/env bash
# Minimal relocatable build sysroot: Valve's GCC 14 backport + Debian glibc 2.31.
# Extracted locally, never installed into /usr or the user's Steam runtime.
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
SDK="${1:-$ROOT/artifacts/steam-sdk}"
mkdir -p -- "$SDK/downloads"
SDK="$(realpath -- "$SDK")"
while read -r digest url; do
    file="$SDK/downloads/${url##*/}"
    if [[ ! -f $file ]] || ! printf '%s  %s\n' "$digest" "$file" | sha256sum --check --status; then
        curl --fail --location --retry 2 --output "$file.partial" "$url"
        printf '%s  %s\n' "$digest" "$file.partial" | sha256sum --check --status
        mv -- "$file.partial" "$file"
    fi
    dpkg-deb --extract "$file" "$SDK"
done < "$ROOT/tools/NoslPilot/steam-sdk-packages.lock"
# Debian's absolute symlinks would resolve on the host while ld inspects the
# extracted sysroot (notably libpthread.so). Make each one sysroot-relative.
while IFS= read -r -d '' link; do
    target="$(readlink -- "$link")"
    [[ $target == /* ]] || continue
    relative="$(realpath --canonicalize-missing --no-symlinks --relative-to="$(dirname -- "$link")" -- "$SDK$target")"
    ln -sfn -- "$relative" "$link"
done < <(find "$SDK" -type l -print0)
echo "Steam-compatible build sysroot: $SDK"
