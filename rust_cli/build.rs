fn main() {
    // Tell Cargo that if the given file changes, to rerun this build script.
    println!("cargo:rerun-if-changed=../cpp_core/src/main.cpp");
    println!("cargo:rerun-if-changed=../cpp_core/CMakeLists.txt");

    // Use the `cmake` crate to build the C++ library.
    // let dst = cmake::build("../cpp_core");

    // Search the output directory for the static library.
    // println!("cargo:rustc-link-search=native={}/build", dst.display());
    // Point to our manual build location
    println!("cargo:rustc-link-search=native=../cpp_core/build/Release");

    // Link the `bt_core` static library.
    println!("cargo:rustc-link-lib=static=bt_core");
}
