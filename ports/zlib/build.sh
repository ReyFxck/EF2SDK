#!/usr/bin/env sh
set -eu

MODE="${1:-target}"
OUT="${2:-build/ports/zlib-${MODE}}"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
VERSION="1.3.2"
ARCHIVE_NAME="zlib-${VERSION}.tar.gz"
URL="https://github.com/madler/zlib/releases/download/v${VERSION}/${ARCHIVE_NAME}"
SHA256="bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"
CACHE="${ROOT}/.cache/ports"
ARCHIVE="${CACHE}/${ARCHIVE_NAME}"
SRC="${ROOT}/build/ports/zlib-src-${VERSION}"
OBJ="${ROOT}/${OUT}/obj"

mkdir -p "${CACHE}" "${ROOT}/${OUT}"

if [ ! -f "${ARCHIVE}" ]; then
    curl -L --fail --retry 3 -o "${ARCHIVE}.tmp" "${URL}"
    mv "${ARCHIVE}.tmp" "${ARCHIVE}"
fi

printf '%s  %s\n' "${SHA256}" "${ARCHIVE}" | sha256sum -c -

if [ ! -f "${SRC}/.ef2-prepared" ]; then
    rm -rf "${SRC}"
    mkdir -p "${SRC}"
    tar -xzf "${ARCHIVE}" --strip-components=1 -C "${SRC}"

    python3 - "${SRC}/zconf.h" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

needle = "typedef unsigned long  uLong; /* 32 bits or more */"
replacement = """/*
 * EF2SDK ABI adaptation:
 * R5900 EABI has 64-bit long by default, while zlib's uLong ABI is kept
 * explicitly 32-bit for this port.
 */
typedef unsigned int   uLong; /* EF2: fixed 32-bit public zlib scalar */"""
if needle not in text:
    raise SystemExit("zconf.h uLong declaration not found")
text = text.replace(needle, replacement, 1)

needle = "#else\n#  define z_off64_t z_off_t\n#endif"
replacement = "#else\n#  define z_off64_t long long\n#endif"
if needle not in text:
    raise SystemExit("zconf.h z_off64_t fallback not found")
text = text.replace(needle, replacement, 1)

path.write_text(text)
PY

    touch "${SRC}/.ef2-prepared"
fi

rm -rf "${OBJ}"
mkdir -p "${OBJ}"

if [ "${MODE}" = "target" ]; then
    : "${CC:=mips64r5900el-ps2-elf-gcc}"
    : "${AR:=mips64r5900el-ps2-elf-ar}"

    CFLAGS="-G0 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -mlong32"
    INCLUDES="-I${ROOT}/include/ef2/compat -I${ROOT}/include -I${SRC}"
else
    : "${CC:=cc}"
    : "${AR:=ar}"

    CFLAGS="-O2 -Wall -Wextra -Werror -fno-builtin"
    INCLUDES="-I${ROOT}/include/ef2/compat -I${ROOT}/include -I${SRC}"
fi

DEFINES="-DZLIB_BUILD -DNO_DIVIDE -DZ_TESTW=4 -DZ_U4=unsigned"

SOURCES="
adler32.c
crc32.c
deflate.c
trees.c
inflate.c
infback.c
inftrees.c
inffast.c
compress.c
uncompr.c
"

for source in ${SOURCES}; do
    object="${OBJ}/${source%.c}.o"
    "${CC}" ${CFLAGS} ${DEFINES} ${INCLUDES}         -c "${SRC}/${source}" -o "${object}"
done

"${CC}" ${CFLAGS} ${DEFINES} ${INCLUDES}     -c "${ROOT}/ports/zlib/ef2_zutil.c"     -o "${OBJ}/ef2_zutil.o"

"${AR}" rcs "${ROOT}/${OUT}/libz.a" "${OBJ}"/*.o

cp "${SRC}/zlib.h" "${ROOT}/${OUT}/zlib.h"
cp "${SRC}/zconf.h" "${ROOT}/${OUT}/zconf.h"

printf 'Built zlib %s (%s) -> %s\n'     "${VERSION}" "${MODE}" "${ROOT}/${OUT}/libz.a"
