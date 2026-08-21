#!/bin/zsh
rm -r build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
macdeployqt build/school_schedule.app
cp -rf ./build/school_schedule.app /Users/aaron/Applications
