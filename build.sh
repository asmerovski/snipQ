rm -rf build/snipQ_autogen
cmake -B build -DCMAKE_BUILD_TYPE=Releasec
cmake --build build --parallel
