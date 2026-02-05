@echo off

if not exist build mkdir build

pushd build

cl ../main.cpp /fsanitize=address /Zi user32.lib Gdi32.lib

popd build
