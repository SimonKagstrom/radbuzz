
#[cxx::bridge]
mod ffi {
unsafe extern "C++" {
    fn kalkon() -> i32;
}
}

#[unsafe(no_mangle)]
extern "C" fn rust_main() -> i32 {
    log::info!("Hello, world!");
    println!("Hello from Rust!");

    ffi::kalkon()
//    42
}
