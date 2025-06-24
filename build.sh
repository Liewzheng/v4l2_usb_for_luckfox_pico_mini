cd source_linux_armv7l
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

cd ../source_win_x86_64
./build_all_platforms.sh all
./create_release.sh