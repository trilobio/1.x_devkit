use anyhow::{Result, anyhow};
use clap::Parser;
use dialoguer::{Input, Select};
use embedded_can::Frame;
use socketcan::{CanFdFrame, ExtendedId, tokio::CanFdSocket};
use std::time::Duration;
use strum::IntoEnumIterator;

mod trilo;
use trilo::*;

#[derive(Parser, Debug)]
struct Args {
    #[arg(short, long, default_value = "can1")]
    interface: String,

    #[arg(long, default_value = "3")]
    period_seconds: u64,
}

#[tokio::main]
async fn main() -> Result<()> {
    let args = Args::parse();

    let tx_id = build_frame_id()?;
    let rx_sock = CanFdSocket::open(&args.interface)?;
    let tx_sock = CanFdSocket::open(&args.interface)?;

    println!("Opened CAN-FD interface {}", args.interface);

    let rx_task = tokio::spawn(async move {
        loop {
            match rx_sock.read_frame().await {
                Ok(frame) => println!("RX {:?}", frame),
                Err(error) => eprintln!("RX error: {error}"),
            }
        }
    });

    let tx_interface = args.interface.clone();
    let tx_task = tokio::spawn(async move {
        loop {
            let frame = match CanFdFrame::new(tx_id, &[]) {
                Some(frame) => frame,
                None => {
                    eprintln!("Could not create CAN-FD frame");
                    return;
                }
            };

            println!("TX interface={tx_interface} id=0x{:08X}", tx_id.as_raw());

            if let Err(error) = tx_sock.write_frame(&frame).await {
                eprintln!("TX error: {error}");
            }

            tokio::time::sleep(Duration::from_secs(args.period_seconds)).await;
        }
    });

    tokio::select! {
        result = rx_task => Err(anyhow!("RX task exited: {result:?}")),
        result = tx_task => Err(anyhow!("TX task exited: {result:?}")),
    }
}

fn build_frame_id() -> Result<ExtendedId> {
    let priorities = Priority::iter().collect::<Vec<_>>();
    let boards = BoardId::iter().collect::<Vec<_>>();
    let commands = CommandId::iter().collect::<Vec<_>>();
    let errors = ErrorBit::iter().collect::<Vec<_>>();

    let priority_index = Select::new()
        .with_prompt("Select priority")
        .items(&labels(&priorities))
        .default(index_of_priority(&priorities, Priority::Low).unwrap_or(0))
        .interact()?;

    let board_index = Select::new()
        .with_prompt("Select board")
        .items(&labels(&boards))
        .default(index_of_board(&boards, BoardId::ToolDevKit).unwrap_or(0))
        .interact()?;

    let command_index = Select::new()
        .with_prompt("Select command")
        .items(&labels(&commands))
        .default(0)
        .interact()?;

    let request_id = Input::<u8>::new()
        .with_prompt("Request ID")
        .default(1)
        .interact_text()?;

    let error_index = Select::new()
        .with_prompt("Select error bit")
        .items(&labels(&errors))
        .default(index_of_error(&errors, ErrorBit::NoError).unwrap_or(0))
        .interact()?;

    let trilo_id = TriloCanId::new(
        priorities[priority_index],
        boards[board_index],
        commands[command_index],
        RequestId::new(request_id),
        errors[error_index],
    );

    let raw_id = trilo_id.to_raw_id();
    let extended_id = ExtendedId::new(raw_id)
        .ok_or_else(|| anyhow!("Trilo raw CAN ID 0x{raw_id:08X} is not a valid extended ID"))?;

    println!("Selected CAN ID: 0x{raw_id:08X} ({trilo_id:?})");
    Ok(extended_id)
}

fn labels<T: std::fmt::Debug>(items: &[T]) -> Vec<String> {
    items.iter().map(|item| format!("{item:?}")).collect()
}

fn index_of_priority(items: &[Priority], value: Priority) -> Option<usize> {
    items.iter().position(|item| *item == value)
}

fn index_of_board(items: &[BoardId], value: BoardId) -> Option<usize> {
    items.iter().position(|item| *item == value)
}

fn index_of_error(items: &[ErrorBit], value: ErrorBit) -> Option<usize> {
    items.iter().position(|item| *item == value)
}
