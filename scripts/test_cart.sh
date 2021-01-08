#!/bin/bash
# tests for reading and writing octocarts

set -e

if [ $# -eq 0 ]; then
	echo "usage: ${0} <path-to-octo-compiler>"
	exit 1
else
	COMPILER=$1
	#echo "running tests against ${COMPILER}..."
fi

# does decoding work?
$COMPILER carts/test_tiny.gif temp.8o
if ! cmp -s temp.8o carts/test_tiny.8o; then
	echo "reference source code doesn't match for test_tiny.gif:"
	cmp -lb carts/test_tiny.8o temp.8o 
	exit 1
fi
rm -rf temp.8o

# does decoding work if the program contains an error?
$COMPILER carts/test_badcode.gif temp.8o
if ! cmp -s temp.8o carts/test_badcode.8o; then
	echo "reference source code doesn't match for test_badcode.gif:"
	cmp -lb carts/test_badcode.8o temp.8o
	exit 1
fi
rm -rf temp.8o

# does decoding work for an image mangled by another tool?
$COMPILER carts/test_optimized.gif temp.8o
if ! cmp -s temp.8o carts/test_tiny.8o; then
	echo "reference source code doesn't match for test_optimized.gif:"
	cmp -lb carts/test_tiny.8o temp.8o 
	exit 1
fi
rm -rf temp.8o

# does encoding work?
$COMPILER carts/test_tiny.gif temp.gif
$COMPILER temp.gif temp.8o
if ! cmp -s temp.8o carts/test_tiny.8o; then
	echo "reference source code doesn't match for round trip encode/decode."
	exit 1
fi
rm -rf temp.8o
rm -rf temp.gif

echo "all cartridge tests passed."
