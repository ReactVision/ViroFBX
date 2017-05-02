#!/bin/bash

set -e

VIRO_RENDERER="../../ViroRenderer/ViroRenderer"
VIRO_FBX="../ViroFBX"

which protoc > /dev/null
if test $? -ne 0; then
  echo "No 'protoc' found. Please download protobuf-3.2.0 and install locally" >&2
  exit 1
fi

if test "$(protoc --version)" != "libprotoc 3.2.0"; then
  echo "Wrong version of 'protoc' installed, please install protobuf-3.2.0" >&2
  exit 1
fi

if test -d "$VIRO_FBX"; then
  echo "ViroFBX found, generating cpp files..." >&2
  protoc --cpp_out="$VIRO_FBX" *.proto
else
  echo "ViroFBX not found, skipping protobuf generation for ViroFBX" >&2
fi

if test -d "$VIRO_RENDERER"; then
  echo "ViroRenderer found, generating cpp files..." >&2
  protoc --cpp_out="$VIRO_RENDERER" *.proto
else
  echo "ViroRenderer not found, skipping protobuf generation for ViroRenderer" >&2
fi

echo "Protobuf generation complete" >&2

