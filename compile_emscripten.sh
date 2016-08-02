#/usr/bin/bash
~/unity/graphics/External/Emscripten/Emscripten/builds/emcc -O2 main.cpp crc.cpp MurmurHash2.cpp MurmurHash3.cpp SpookyV2.cpp xxhash.c -o emscripten.html -s TOTAL_MEMORY=268435456 --preload-file test-words.txt --preload-file test-filenames.txt --preload-file test-code.txt
