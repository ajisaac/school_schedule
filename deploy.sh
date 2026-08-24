#!/bin/zsh
rm -r build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
macdeployqt build/school_schedule.app

# macdeployqt rewrites the install names of the libraries it copies in, which
# invalidates the signatures they shipped with. Re-sign ad hoc from the inside
# out, otherwise arm64 macOS refuses to load them.
find build/school_schedule.app/Contents/Frameworks -name "*.dylib" -exec codesign --force --sign - {} +
find build/school_schedule.app/Contents/Frameworks -type d -name "*.framework" -exec codesign --force --sign - {} +
codesign --force --deep --sign - build/school_schedule.app
codesign --verify --deep --strict build/school_schedule.app

cp -rf ./build/school_schedule.app /Users/aaron/Applications
