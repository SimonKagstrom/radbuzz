use autocxx::prelude::*; // use all the main autocxx functions

include_cpp! {
    #include "hal/i_wifi.hh" // your header file name
    safety!(unsafe_ffi) // see details of unsafety policies described in the 'safety' section of the book
    generate!("IWifi") // add this line for each function or type you wish to generate
}

#[cxx::bridge]
mod ffi_bridge {
    extern "Rust" {
        fn rust_main(wifi: &mut IWifi) -> i32;
    }
}

#[unsafe(no_mangle)]
extern "C" fn rust_main(wifi: &mut ffi::IWifi) -> i32 {
    log::info!("Hello, world!");
    println!("Hello from Rust! SIMON");

    //ffi::kalkon().into()
    42
}
