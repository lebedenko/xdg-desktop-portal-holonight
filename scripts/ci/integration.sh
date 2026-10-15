#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
task build:release
DESTDIR="$PWD/build/stage" cmake --install build/release
python3 scripts/ci/check-payload.py "$PWD/build/stage"
