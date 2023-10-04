@echo off

set WildCard=*.h *.cpp *.inl *.c

echo -------
echo -------

echo TODOD_FOUND:
findstr -s -n -i -l "TODO" %WildCard% 

echo -------
echo -------
