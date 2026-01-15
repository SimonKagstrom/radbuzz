fn main() -> miette::Result<()> {
    let include_path = "../../src/include"; // path to your C++ headers

    let mut b = autocxx_build::Builder::new("src/lib.rs", &[&include_path])
    .extra_clang_args(&["-x", "c++", "-std=c++23"])
        .extra_clang_args(&["--target", "riscv32imafc-unknown-none-elf"])
        .extra_clang_args(&["-isystem", "/Users/ska/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20251107/riscv32-esp-elf/bin/../riscv32-esp-elf/include",
        "-isystem", "/Users/ska/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20251107/riscv32-esp-elf/bin/../riscv32-esp-elf/include/c++/14.2.0",
        "-isystem", "/Users/ska/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20251107/riscv32-esp-elf/bin/../riscv32-esp-elf/include/c++/14.2.0/riscv32-esp-elf/"])
        .build()?;

    b.flag("-march=rv32imafc")
        //        .flag("-mabi=ilp32f")
        .flag("-std=c++23")
        .compile("autocxx-demo"); // arbitrary library name, pick anything
    
    println!("cargo:rerun-if-changed=src/lib.rs");

    // Add instructions to link to any C++ libraries you need.

    Ok(())
}
