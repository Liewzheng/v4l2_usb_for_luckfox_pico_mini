cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

cd source_pc
make -f Makefile.pc