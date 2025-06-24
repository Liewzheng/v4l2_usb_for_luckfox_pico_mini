WORK_DIR=$(pwd)

cd $WORK_DIR/source_linux_armv7l
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./create_release.sh

cd $WORK_DIR/source_all_platform
./build_all_platforms.sh all
./create_release.sh