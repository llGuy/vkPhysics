@echo off

If "%1" == "clean" goto clean

etags *.cpp *.hpp

:build
pushd ..\build
msbuild /nologo vkPhysics.sln
popd

goto eof

:clean
pushd ..\build
msbuild /nologo vkPhysics.sln -t:Clean
popd

goto eof

:eof
