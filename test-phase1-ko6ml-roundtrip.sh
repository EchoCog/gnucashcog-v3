#!/bin/bash

########################################################################
# test-phase1-ko6ml-roundtrip.sh
#
# Automated verification pipeline for Phase 1: ko6ml Primitives &
# Foundational Hypergraph Encoding.
#
# Compiles the engine-independent ko6ml translation layer together
# with the verification demo (real translations, no mocks) and runs
# the full round-trip + benchmark suite.  Requires only a C++17
# compiler and GLib development files.
#
# Copyright (C) 2024 GnuCash Cognitive Engine
########################################################################

set -e

echo "========================================================================"
echo "  Phase 1: ko6ml <-> AtomSpace Hypergraph Round-Trip Verification"
echo "========================================================================"
echo ""

cd "$(dirname "$0")"

BUILD_DIR="$(mktemp -d /tmp/phase1-ko6ml-XXXXXX)"
cleanup() { rm -rf "$BUILD_DIR"; }
trap cleanup EXIT

if ! pkg-config --exists glib-2.0; then
    echo "❌ glib-2.0 development files not found (install libglib2.0-dev)"
    exit 1
fi

echo "🔨 Compiling ko6ml translation layer and verification pipeline..."
g++ -std=c++17 -Wall -Wextra -O2 \
    -I libgnucash/engine \
    $(pkg-config --cflags glib-2.0) \
    phase1-ko6ml-roundtrip-demo.cpp \
    libgnucash/engine/gnc-ko6ml-translation.cpp \
    $(pkg-config --libs glib-2.0) \
    -o "$BUILD_DIR/phase1-ko6ml-roundtrip-demo"
echo "✅ Compilation succeeded"
echo ""

"$BUILD_DIR/phase1-ko6ml-roundtrip-demo"
