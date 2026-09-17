#!/usr/bin/env sh
set -eu

VERSION="$(cat VERSION)"
ROOT="EF2SDK-${VERSION}"
DIST="dist"
STAGE="${DIST}/${ROOT}"

rm -rf "${DIST}"
mkdir -p "${STAGE}/bin" "${STAGE}/lib" "${STAGE}/include" "${STAGE}/ld" "${STAGE}/docs"

cp build/ef2-boot.elf "${STAGE}/bin/"
cp build/libef2.a "${STAGE}/lib/"
cp -R include/ef2 "${STAGE}/include/"
cp ld/ee.ld "${STAGE}/ld/"
cp README.md LICENSE VERSION "${STAGE}/"
cp docs/ARCHITECTURE.md docs/ROADMAP.md docs/AUDIO.md "${STAGE}/docs/"

(
    cd "${DIST}"
    tar -czf "${ROOT}.tar.gz" "${ROOT}"
    zip -qr "${ROOT}.zip" "${ROOT}"
)

rm -rf "${STAGE}"
printf 'Created %s/%s.tar.gz\n' "${DIST}" "${ROOT}"
printf 'Created %s/%s.zip\n' "${DIST}" "${ROOT}"
