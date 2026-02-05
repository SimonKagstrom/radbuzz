#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use embassy_executor::{Executor, Spawner};
use static_cell::StaticCell;
use std::os::raw::c_char;

use statig::prelude::*;
//use state_machines::state_machine;

mod wifi_handler;

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

/*
state_machine! {
    name: WifiStateMachine,

    initial: On,
    dynamic: true,
    async: true,
    states : [On, Scanning, Connect, Connected, LostConnection, Off],
    events: {
        scan {
            transition: { from: On, to: Scanning },
        }
        moving {
            transition: { from: [Scanning, LostConnection], to: Off },
        }
        not_moving {
            transition: { from: Off, to: On },
            transition: { from: LostConnection, to: Scanning},
        }
        matching_ssid {
            transition: { from: Scanning, to: Connect },
        }
        connected {
            transition: { from: Connect, to: Connected },
        }
        connection_failed {
            transition: { from: Connect, to: Scanning },
        }
        lost_connection {
            transition: { from: Connected, to: LostConnection },
        }
    }
}
*/

#[derive(Debug)]
pub enum Event {
    SsidFound,
    Connected,
    ConnectionFailed,
    LostConnection,
    Moving,
    NotMoving,
}

#[derive(Debug, Default)]
pub struct WifiStateMachine;

#[state_machine(initial = "State::on()", state(derive(Debug)))]
impl WifiStateMachine {
    #[action]
    async fn enter_on(&mut self) {
        // TODO: Do something
    }

    #[state(entry_action = "enter_on")]
    async fn on(event: &Event) -> Outcome<State> {
        match event {
            _ => Transition(State::scanning()),
        }
    }


    #[state]
    async fn scanning(event: &Event) -> Outcome<State> {
        match event {
            Event::SsidFound => Transition(State::connect()),
            Event::Moving => Transition(State::connect()), // TODO
            _ => Super,
        }
    }

    #[action]
    async fn enter_connect(&mut self) {
        // TODO: Do something
    }

    #[state(entry_action = "enter_connect")]
    async fn connect(event: &Event) -> Outcome<State> {
        match event {
            _ => Transition(State::scanning()),
        }
    }

}


#[embassy_executor::task]
async fn main_task(spawner: Spawner) {}
static EXECUTOR: StaticCell<Executor> = StaticCell::new();

#[unsafe(no_mangle)]
extern "C" fn rust_main(mut handle: IWifiHandle) -> i32 {
    log::info!("Hello, world!");
    println!("Hello from Rust! SIMON");

    unsafe {
        ConnectToHotspot(
            &mut handle,
            "MySSID\0".as_ptr() as *const c_char,
            "MyPassword\0".as_ptr() as *const c_char,
        );
    }

    let executor = EXECUTOR.init(Executor::new());
    executor.run(|spawner| {
        spawner.spawn(main_task(spawner));
    });
}
