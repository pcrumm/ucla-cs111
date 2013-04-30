#!/bin/sh
# CS111 Lab 1C - Test that parallelization works properly.

# We've decided that the post way to go about this is comparing the output
# to the expected using a number of sleep statements.
tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

cat >test.sh <<'EOF'
(cat /usr/share/dict/words | head -n 5 | sort -r; sleep 5; echo testing)

(echo intermixed parallelization; sleep 1)

(echo hello, world > test; cat test > test2; cat test2)

echo parallel
EOF

cat >test.exp <<'EOF'
intermixed parallelization
parallel
12-point
11-point
10th
10-point
1080
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