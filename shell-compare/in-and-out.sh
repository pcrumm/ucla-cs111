cat /usr/share/dict/words | head -n 20 > pipetmp
head -n 5 <pipetmp > pipeout
cat pipeout
rm pipetmp pipeout