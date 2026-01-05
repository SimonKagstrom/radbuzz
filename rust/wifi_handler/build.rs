// build.rs

fn main() {
    cxx_build::bridge("src/lib.rs");
}
