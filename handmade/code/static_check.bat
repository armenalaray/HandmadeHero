@echo off

set WildCard=*.h *.cpp *.inl *.c

echo -------
echo -------

echo STATICS_FOUND:
findstr -s -n -i -l "static" %WildCard% 

echo -------
echo -------

echo GLOBALS_FOUND
findstr -s -n -i -l "local_persist" %WildCard% 
findstr -s -n -i -l "global_variable" %WildCard% 

echo -------
echo -------
