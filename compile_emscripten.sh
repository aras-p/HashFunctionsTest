#/usr/bin/bash
# note: needs 'source ~/proj/other/emsdk/emsdk_env.sh'
# note: then run a http server, e.g. python -m SimpleHTTPServer, and go to 0.0.0.0:8000
# note: set privacy.reduceTimerPrecision to false in about:config on Firefox
emcc -O2 main.cpp HashFunctions/*.cpp HashFunctions/*.c HashFunctions/*.cc HashFunctions/t1ha_src/*.c -o emscripten.html -s TOTAL_MEMORY=268435456 --preload-file TestData/test-words.txt --preload-file TestData/test-filenames.txt --preload-file TestData/test-binary.bin -s WASM=1
