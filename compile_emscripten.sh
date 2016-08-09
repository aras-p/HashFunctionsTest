#/usr/bin/bash
~/unity/graphics/External/Emscripten/Emscripten/builds/emcc -O2 main.cpp HashFunctions/*.cpp HashFunctions/*.c HashFunctions/*.cc -o emscripten.html -s TOTAL_MEMORY=268435456 --preload-file TestData/test-words.txt --preload-file TestData/test-filenames.txt --preload-file TestData/test-binary.bin
