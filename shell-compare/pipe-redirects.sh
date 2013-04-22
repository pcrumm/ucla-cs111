cat /usr/share/dict/words | head -n 20 > testwords
(head -n 5 | sort -r) <testwords > testsort
cat testsort
rm testwords testsort
