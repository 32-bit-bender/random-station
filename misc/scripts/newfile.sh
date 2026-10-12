#!/usr/bin/env bash
# Create C source/header files with a standard file header.
#
#   tools/newfile.sh src/drivers/station     -> station.c and station.h
#   tools/newfile.sh src/drivers/station.h   -> station.h only
#   tools/newfile.sh src/drivers/station.c   -> station.c only
#
# The author defaults to 32-bit-bender; override with AUTHOR=name tools/...
# Existing files are never overwritten.
set -euo pipefail

AUTHOR="${AUTHOR:-32-bit-bender}"
DATE="$(date +%Y-%m-%d)"

usage() {
    echo "Usage: $0 <path>[.c|.h]" >&2
    echo "  $0 src/drivers/station     creates station.c and station.h" >&2
    echo "  $0 src/drivers/station.h   creates only station.h" >&2
    exit 1
}

[ $# -eq 1 ] || usage

arg="$1"
case "$arg" in
    *.c) base="${arg%.c}"; make_c=1; make_h=0 ;;
    *.h) base="${arg%.h}"; make_c=0; make_h=1 ;;
    *)   base="$arg";      make_c=1; make_h=1 ;;
esac

name="$(basename "$base")"
dir="$(dirname "$base")"
guard="$(printf '%s' "$name" | tr '[:lower:]-' '[:upper:]_')_H"

# Refuse to overwrite anything, and check before creating either file.
if [ "$make_c" = 1 ] && [ -e "$dir/$name.c" ]; then
    echo "error: $dir/$name.c already exists" >&2
    exit 1
fi
if [ "$make_h" = 1 ] && [ -e "$dir/$name.h" ]; then
    echo "error: $dir/$name.h already exists" >&2
    exit 1
fi

mkdir -p "$dir"

file_header() { # $1 = file name
    cat <<EOF
/**
 * @file    $1
 * @brief   TODO: one-line description
 * @author  $AUTHOR
 * @date    $DATE
 */

EOF
}

if [ "$make_h" = 1 ]; then
    {
        file_header "$name.h"
        cat <<EOF
#ifndef $guard
#define $guard

#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}
#endif

#endif /* $guard */
EOF
    } > "$dir/$name.h"
    echo "created $dir/$name.h"
fi

if [ "$make_c" = 1 ]; then
    {
        file_header "$name.c"
        # Include the module's own header first so it is checked to be
        # self-contained.
        if [ "$make_h" = 1 ]; then
            echo "#include \"$name.h\""
        fi
    } > "$dir/$name.c"
    echo "created $dir/$name.c"
    echo "note: add $dir/$name.c to add_executable() in CMakeLists.txt"
fi
