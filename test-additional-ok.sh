#! /bin/sh

# UCLA CS 111 Lab 1 - Test that valid syntax is processed correctly.

tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

cat >test.sh <<'EOF'
a || b ; c && d || (a && c ; d )

( a > out || b ) && c > out2

c && ( (a) || b )

( a || b ) && ( c && d )

( a && ( b || c ) )

( a || b ) <in

( a || b )>out

( a || b )<in>out

c && d || ( a && ( ( c ; d ) | e > out ) > out2 ) > out3 | f < in2

a || b && c | d && e || f

( (a||(b)|( (c)|f))||c&&( ( (a)||b)))
EOF

cat >test.exp <<'EOF'
# 1
    a \
  ||
    b
# 2
      c \
    &&
      d \
  ||
    (
         a \
       &&
         c \
     ;
       d
    )
# 3
    (
       a>out \
     ||
       b
    ) \
  &&
    c>out2
# 4
    c \
  &&
    (
       (
        a
       ) \
     ||
       b
    )
# 5
    (
       a \
     ||
       b
    ) \
  &&
    (
       c \
     &&
       d
    )
# 6
  (
     a \
   &&
     (
        b \
      ||
        c
     )
  )
# 7
  (
     a \
   ||
     b
  )<in
# 8
  (
     a \
   ||
     b
  )>out
# 9
  (
     a \
   ||
     b
  )<in>out
# 10
      c \
    &&
      d \
  ||
      (
         a \
       &&
         (
            (
               c \
             ;
               d
            ) \
          |
            e>out
         )>out2
      )>out3 \
    |
      f<in2
# 11
        a \
      ||
        b \
    &&
        c \
      |
        d \
    &&
      e \
  ||
    f
# 12
  (
       (
          a \
        ||
            (
             b
            ) \
          |
            (
               (
                c
               ) \
             |
               f
            )
       ) \
     ||
       c \
   &&
     (
      (
         (
          a
         ) \
       ||
         b
      )
     )
  )
EOF

../timetrash -p test.sh >test.out 2>test.err || ( echo "there were errors running $0: maybe an invalid syntax error was found" ; exit )

diff -u test.exp test.out || exit
test ! -s test.err || {
  cat test.err
  exit 1
}

) || exit

rm -fr "$tmp"
