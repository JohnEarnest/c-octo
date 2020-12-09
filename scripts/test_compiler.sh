#!/bin/bash
# integration tests for octo-cli based on
# a collection of blessed reference outputs and error logs.

if [ $# -eq 0 ]; then
	echo "usage: ${0} <path-to-octo-compiler>"
	exit 1
else
	COMPILER=$1
	echo "running tests against ${COMPILER}..."
fi

positive_test() {
	rm -rf temp.ch8
	rm -rf temp.err
	$COMPILER "$1" temp.ch8 2> temp.err
	ec=$?
	if [ -s temp.err ]; then
		echo "errors running test ${1}:"
		cat temp.err
		exit 1
	elif [ $ec != 0 ]; then
		echo "crash running test ${1}."
		exit 1
	elif [ ! -f temp.ch8 ]; then
		echo "no output for ${1}."
		exit 1
	elif ! cmp -s temp.ch8 $2; then
		echo "reference binary doesn't match for ${1}:"
		cmp -lb $2 temp.ch8 | head -n 10
		exit 1
	fi
}

negative_test() {
	rm -rf temp.ch8
	rm -rf temp.err
	$COMPILER "$1" temp.ch8 2> temp.err
	ec=$?
	if ! diff -q --strip-trailing-cr temp.err $2; then
		echo "reference error doesn't match for ${1}:"
		echo "expected:"
		cat $2
		echo "got:"
		cat temp.err
		exit 1
	fi
}

for filename in tests/*.8o; do
	if [ -f ${filename%.*}.ch8 ]; then
		positive_test $filename ${filename%.*}.ch8
	elif [ -f ${filename%.*}.err ]; then
		negative_test $filename ${filename%.*}.err
	else
		echo "no reference file found for test ${filename}!"
		exit 1
	fi
done
echo "all compiler tests passed."
rm -rf temp.ch8
rm -rf temp.err
