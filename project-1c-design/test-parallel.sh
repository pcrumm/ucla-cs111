#!/bin/sh
# CS111 Lab 1C - Test that parallelization works properly.

# We've decided that the post way to go about this is comparing the output
# to the expected using a number of sleep statements.
tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

echo "apples
oranges
bananas
pears
pineapples
avocados
mangoes" > fruit

cat >test.sh <<'EOF'
(cat fruit | head -n 5 | sort -r; sleep 5; echo testing)

(sleep 1; echo intermixed parallelization; sleep 1; echo passed)

(sleep 3; echo hello, world > test; cat test > test2; cat test2)

echo parallel

rm test test2
EOF

cat >test.exp <<'EOF'
pineapples
pears
oranges
bananas
apples
parallel
intermixed parallelization
passed
hello, world
testing
EOF

../timetrash -t test.sh >test.out 2>test.err || exit

diff -u test.exp test.out || exit
test ! -s test.err || {
  cat test.err
  exit 1
}

) || exit

rm -fr "$tmp"