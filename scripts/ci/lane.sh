#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
lane=$1
mkdir /work/portal
cp -a /input/. /work/portal/
cd /work/portal
export HOME=/work/build/home LC_ALL=C.UTF-8 TZ=UTC
export JOBS=${JOBS:-2} CMAKE_BUILD_PARALLEL_LEVEL=${JOBS:-2}
export XDG_RUNTIME_DIR=/work/runtime QT_FORCE_STDERR_LOGGING=1
export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RHI_BACKEND=software
export QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=
mkdir -p "$HOME"
mkdir -m 700 "$XDG_RUNTIME_DIR"
if [ "$lane" = licensing ]; then
  reuse --version
  reuse lint
  exit
fi
mkdir -p build/ci-provenance
pacman -Q > build/ci-provenance/packages.txt
c++ --version > build/ci-provenance/compiler.txt
cmake --version > build/ci-provenance/cmake.txt
{ python3 --version; task --version; ninja --version; clang-format --version; clang-tidy --version; qmake6 --version; wayland-scanner --version; } > build/ci-provenance/versions.txt 2>&1
capture() {
  status=$?
  cp -a build/ci-provenance /output/
  if [ -d build/test/Testing ]; then cp -a build/test/Testing /output/; fi
  if [ -f build/test/compile_commands.json ]; then cp build/test/compile_commands.json /output/; fi
  if [ -d .cache/tooling ]; then cp -a .cache/tooling /output/tooling; fi
  exit "$status"
}
trap capture 0
if [ "$lane" = prepare ]; then
  export HOLONIGHT_CONFIG_SOURCE=/prepared/sources/holonight-config
  export HOLONIGHT_QT_SOURCE=/prepared/sources/holonight-qt
  export HOLONIGHT_SEARCH_SOURCE=/prepared/sources/holonight-search
  export HOLONIGHT_FILES_SOURCE=/prepared/sources/holonight-files
  if [ -d /prepared/deps ]; then cp -a /prepared/deps build/deps; fi
  task deps PRESET=test
  mkdir -p /prepared/deps
  cp -a build/deps/. /prepared/deps/
  cp -a build/ci-provenance /prepared/environment
  exit
fi
cp -a /prepared/deps build/deps
cp /prepared/provider-revisions.json build/ci-provenance/
cp build/deps/provider-revisions.tsv build/ci-provenance/
export LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib"
python3 scripts/ci/test_launcher.py
python3 scripts/ci/test_dependencies.py
case "$lane" in
  build-test)
    task test
    cat build/test/Testing/Temporary/LastTest.log
    ;;
  release)
    task build PRESET=release
    DESTDIR=/work/stage cmake --install build/release
    python3 scripts/ci/check-payload.py /work/stage
    cp -a /work/stage /output/
    ;;
  static-checks)
    task build PRESET=test
    task format-check
    task tidy
    cmake --build build/test --target all_qmllint
    python3 tooling/qml_policy.py
    python3 tooling/workflow.py qmltypes-check --preset test
    ;;
  *) echo "Unknown lane: $lane" >&2; exit 2 ;;
esac
