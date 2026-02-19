# Add CMake to PATH
$env:PATH = "C:\Program Files\CMake\bin;" + $env:PATH

# Build C++ Core
cd cpp_core
mkdir build
cd build
cmake ..
cmake --build . --config Release
cd ../..

# Build Rust CLI
cd rust_cli
cargo build --release
cd ..

echo "Build Complete. Binary at rust_cli/target/release/btmanager.exe"
