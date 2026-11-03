#!/usr/bin/env bash
# Assembles the web version for hosting (for example on GitHub Pages).
#
#   web/package.sh <qmake build dir> <output dir>
#
# The build dir is where `qmake web/classboard-web.pro && make` ran (it holds classboard.js,
# classboard.wasm and qtloader.js). pdf.js (used for PDF import) is fetched from npm.
set -euo pipefail

build="${1:?usage: package.sh <build dir> <output dir>}"
out="${2:?usage: package.sh <build dir> <output dir>}"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(dirname "$here")"
pdfjs_version="3.11.174"

for f in classboard.js classboard.wasm qtloader.js; do
    [[ -f "$build/$f" ]] || { echo "missing $build/$f (build the web version first)" >&2; exit 1; }
done

mkdir -p "$out"
cp "$build/classboard.js" "$build/classboard.wasm" "$build/qtloader.js" "$out/"
cp "$here/index.html" "$here/classboard-web.js" "$here/manifest.webmanifest" "$out/"
cp "$root/resources/app/classboard.svg" "$out/classboard.svg"
touch "$out/.nojekyll"

# pdf.js for PDF import (Apache License 2.0).
if [[ ! -f "$out/pdfjs/pdf.min.js" ]]; then
    tmp="$(mktemp -d)"
    (cd "$tmp" && npm pack --silent "pdfjs-dist@$pdfjs_version" >/dev/null && tar -xzf "pdfjs-dist-$pdfjs_version.tgz")
    mkdir -p "$out/pdfjs"
    cp "$tmp/package/build/pdf.min.js" "$tmp/package/build/pdf.worker.min.js" "$tmp/package/LICENSE" "$out/pdfjs/"
    rm -rf "$tmp"
fi

echo "Web version assembled in $out:"
ls -la "$out"
