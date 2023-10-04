@echo off
REM TODO -can we just build both with one exe?
x:
cd handmade\code\

REM -FR and flag for making sbr files and bscmake *.sbr to create .bs file. Enabling browser info for msdev   

set CommonCompilerFlags= -Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4238 -wd4310 -wd4335 -wd4010 -wd4201 -wd4100 -wd4189 -wd4456 -wd4505 -wd4805 -FC -Z7 
set CommonCompilerFlags= -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW_PERFORMANCE=1 -DHANDMADE_WIN32=1 %CommonCompilerFlags%  
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST  ..\..\build mkdir ..\..\build
pushd ..\..\build

del *.pdb > NUL 2> NUL

REM Asset file builder build
REM cl  %CommonCompilerFlags% -D_CRT_SECURE_NO_WARNINGS ..\handmade\code\test_asset_builder.cpp /link %CommonLinkerFlags% 

REM simple_preprocessor.cpp
cl  %CommonCompilerFlags% ..\handmade\code\simple_preprocessor.cpp /link %CommonLinkerFlags% 

pushd ..\handmade\code\
..\..\build\simple_preprocessor.exe > handmade_generated.cpp
popd


REM 32-bit build
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags% 

REM 64-bit build
REM Optimization switches /O2 
echo WAITING FOR PDB > lock.tmp
cl  %CommonCompilerFlags% -O2 -I..\iaca-win64\ -c ..\handmade\code\handmade_optimized.cpp -Fohandmade_optimized.obj -LD 

cl  %CommonCompilerFlags% -I..\iaca-win64\ ..\handmade\code\handmade.cpp handmade_optimized.obj -Fmhandmade.map -LD /link -incremental:no -opt:ref -PDB:handmade_%random%.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender -EXPORT:GameDEBUGFrameEnd

REM you know here that recompilation has finished so we remove the lock
del lock.tmp

cl  %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags% 

popd	