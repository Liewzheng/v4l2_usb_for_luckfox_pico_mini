cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

cd source_linux_x86_64
make