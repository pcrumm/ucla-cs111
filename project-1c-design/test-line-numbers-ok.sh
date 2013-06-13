#! /bin/sh

# UCLA CS 111 Lab 1 - Test that valid syntax is processed correctly.

tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

cat >test.sh <<'EOF'
true

g++ -c foo.c

: : :

cat < /etc/passwd | tr a-z A-Z | sort -u || echo sort failed!

a b<c > d

cat < /etc/passwd | tr a-z A-Z | sort -u > out || echo sort failed!

a&&b||
 c &&
  d | e && f|

g<h

# This is a weird example: nobody would ever want to run this.
a<b>c|d<e>f|g<h>i
EOF

cat >test.exp <<'EOF'
# 1
 (line: 1)  true
# 2
 (line: 3)  g++ -c foo.c
# 3
 (line: 5)  : : :
# 4
 (line: 7)      cat</etc/passwd \
    |
 (line: 7)      tr a-z A-Z \
    |
 (line: 7)      sort -u \
  ||
 (line: 7)    echo sort failed!
# 5
 (line: 9)  a b<c>d
# 6
 (line: 11)      cat</etc/passwd \
    |
 (line: 11)      tr a-z A-Z \
    |
 (line: 11)      sort -u>out \
  ||
 (line: 11)    echo sort failed!
# 7
 (line: 13)        a \
      &&
 (line: 13)        b \
    ||
 (line: 14)      c \
  &&
 (line: 15)      d \
    |
 (line: 15)      e \
  &&
 (line: 15)      f \
    |
 (line: 17)      g<h
# 8
 (line: 20)    a<b>c \
  |
 (line: 20)    d<e>f \
  |
 (line: 20)    g<h>i
EOF

../timetrash -p -l test.sh >test.out 2>test.err || ( echo "there were errors running $0: maybe an invalid syntax error was found" ; exit )

diff -u test.exp test.out || exit
test ! -s test.err || {
  cat test.err
  exit 1
}

) || exit

rm -fr "$tmp"
