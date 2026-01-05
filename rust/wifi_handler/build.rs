fn main() -> miette::Result<()> {
    let include_path = std::path::PathBuf::from("../../esp32/esp32p4/main/");

    // This assumes all your C++ bindings are in main.rs
    let mut b = autocxx_build::Builder::new("src/lib.rs", &[&include_path])
//        .extra_clang_args(&["-mabi=ilp32f"])
        .build().unwrap();

    b.flag("-march=rv32imafc")
//        .flag("-mabi=ilp32f")
        .flag("-std=c++23")
        .compile("autocxx-demo"); // arbitrary library name, pick anything
    println!("cargo:rerun-if-changed=src/lib.rs");

    // Add instructions to link to any C++ libraries you need.

    Ok(())
}
