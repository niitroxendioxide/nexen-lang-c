rm -rf build
cmake -S . -B build
cmake --build build

./build/nexen input/main.nx
