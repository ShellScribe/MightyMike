#!/bin/sh
set -eu
cd "$(dirname "$0")"
exec ./build-release/MightyMike.app/Contents/MacOS/MightyMike --coop
