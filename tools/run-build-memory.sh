#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

pio run -e nanoatmega328
printf '\n--- Detailed memory report ---\n'
pio run -e nanoatmega328 -v