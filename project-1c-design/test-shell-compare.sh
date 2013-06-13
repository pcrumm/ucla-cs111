#!/bin/bash
tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

for file in ../shell-compare/* ; do
  if [ ! -d "$file" ]; then
    bash $file >bash.out 2>bash.err || exit
    ../timetrash $file >tt.out 2>tt.err || exit

    diff -u bash.out tt.out || exit
    diff -u bash.err tt.err || exit
  fi
done
)

rm -fr "$tmp"