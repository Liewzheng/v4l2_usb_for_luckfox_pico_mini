cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

cd source_linux
make -f Makefile.pc