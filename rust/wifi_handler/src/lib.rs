use autocxx::prelude::*; // use all the main autocxx functions

include_cpp! {
    #include "kalkon.hh" // your header file name
    safety!(unsafe) // see details of unsafety policies described in the 'safety' section of the book
    generate!("kalkon") // add this line for each function or type you wish to generate
}

#[unsafe(no_mangle)]
extern "C" fn rust_main() -> i32 {
    log::info!("Hello, world!");
    println!("Hello from Rust! SIMON");

    ffi::kalkon().into()
//    42
}
