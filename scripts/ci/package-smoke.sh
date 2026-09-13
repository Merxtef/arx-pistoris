#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

set -euo pipefail

prefix=${1:?usage: package-smoke.sh <install-prefix>}
exe="$prefix/bin/arx-pistor"

[[ -x "$exe" ]] || { echo "missing executable: $exe" >&2; exit 1; }

"$exe" --version >/dev/null
"$exe" --help >/dev/null

set +e
"$exe" >/dev/null 2>&1
rc=$?
set -e
[[ $rc -eq 1 ]] || { echo "no-argument invocation returned $rc, expected 1" >&2; exit 1; }

doc_dir="$prefix/share/doc/arx-pistoris"
for name in \
  LICENSE \
  ADDITIONAL_TERMS \
  AUTHORS \
  ARX_LIBERTATIS_AUTHORS \
  README.md \
  cgltf-LICENSE \
  stb-LICENSE \
  dr_libs-LICENSE \
  blast-LICENSE \
  pklib-LICENSE \
  nlohmann-json-LICENSE \
  mapbox-earcut-LICENSE; do
  [[ -f "$doc_dir/$name" ]] || { echo "missing installed document: $doc_dir/$name" >&2; exit 1; }
done

bash -n scripts/install.sh

if ldd "$exe" | grep -Eq 'libstdc\+\+|libgcc_s'; then
  echo "Linux package has a dynamic libstdc++ or libgcc dependency" >&2
  exit 1
fi
