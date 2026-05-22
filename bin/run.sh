#!/bin/bash
# Run the TriangleGame from the bin directory
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

LD_LIBRARY_PATH=".:/home/neo/.local/crab-tools/usr/lib/x86_64-linux-gnu:/home/neo/.local/crab-sdk/usr/lib/x86_64-linux-gnu" \
exec ./TriangleGame
