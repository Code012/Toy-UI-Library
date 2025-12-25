@echo off

if not exist build mkdir build

pushd build

cl ../main.cpp /fsanitize=address /Zi

popd build
