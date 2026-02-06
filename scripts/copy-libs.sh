#!/usr/bin/env bash
set -euo pipefail

bin="${1:?binary path required}"
rootfs="${2:?rootfs dir required}"

if [[ "${CLEAN_ROOTFS:-0}" == "1" ]]; then
    rm -rf "$rootfs"
fi
mkdir -p "$rootfs"

if [[ ! -e "$bin" ]]; then
    echo "error: binary not found: $bin" >&2
    exit 1
fi

interpreter="$(readelf -l "$bin" 2>/dev/null | sed -n 's/.*program interpreter: \(.*\)]/\1/p')"

needed="$(readelf -d "$bin" 2>/dev/null | awk -F'[][]' '/NEEDED/ {print $2}')"
rpaths="$(readelf -d "$bin" 2>/dev/null | awk -F'[][]' '/RPATH|RUNPATH/ {print $2}')"

if [[ -z "$needed" && -z "$interpreter" ]]; then
    echo "error: no dynamic loader or shared libs found; binary may be static" >&2
    exit 1
fi

origin="$(cd "$(dirname "$bin")" && pwd)"
interp_dir=""
interp_real=""
if [[ -n "$interpreter" ]]; then
    interp_dir="$(dirname "$interpreter")"
    if [[ -e "$interpreter" ]]; then
        interp_real="$(readlink -f "$interpreter")"
    fi
fi

expand_origin() {
    local path="$1"
    path="${path//\\$ORIGIN/$origin}"
    path="${path//\\$\\{ORIGIN\\}/$origin}"
    printf '%s\n' "$path"
}

search_dirs=()
if [[ -n "$rpaths" ]]; then
    IFS=':' read -r -a _rpaths <<<"$rpaths"
    for p in "${_rpaths[@]}"; do
        [[ -z "$p" ]] && continue
        search_dirs+=("$(expand_origin "$p")")
    done
fi
[[ -n "$interp_dir" ]] && search_dirs+=("$interp_dir")
[[ -n "$interp_real" ]] && search_dirs+=("$(dirname "$interp_real")")
search_dirs+=(
    /lib /lib64 /usr/lib /usr/lib64
    /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu
    /lib/x86_64-linux-musl /usr/lib/x86_64-linux-musl
)

copy_file() {
    local src="$1"
    local dest="$rootfs$src"
    mkdir -p "$(dirname "$dest")"
    cp -L "$src" "$dest"
    chmod 0755 "$dest" || true
}

if [[ -n "$interpreter" ]]; then
    if [[ ! -e "$interpreter" ]]; then
        echo "error: missing runtime file: $interpreter" >&2
        echo "hint: install the matching libc or build with the intended toolchain" >&2
        exit 1
    fi
    copy_file "$interpreter"
fi

if [[ -n "$needed" ]]; then
    while IFS= read -r lib; do
        [[ -z "$lib" ]] && continue
        found=""
        for dir in "${search_dirs[@]}"; do
            if [[ -e "$dir/$lib" ]]; then
                found="$dir/$lib"
                break
            fi
        done
        if [[ -z "$found" ]]; then
            echo "error: unable to locate needed library: $lib" >&2
            echo "hint: install the matching libc or update library search paths" >&2
            exit 1
        fi
        copy_file "$found"
    done <<<"$needed"
fi
