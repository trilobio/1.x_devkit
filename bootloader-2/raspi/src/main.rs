use anyhow::anyhow;
use clap::Parser;
use embedded_can::{Frame as EmbeddedFrame, Id};
use indicatif::{ProgressBar, ProgressStyle};
use socketcan::{CanFdFrame, CanFdSocket, Socket};
use std::fs;
use std::path::{Path, PathBuf};
use std::time::Duration;

mod trilo;
use trilo::*;

const APP_BASE_ADDRESS: u32 = 0x0800_8000;

#[derive(Parser, Debug)]
struct Args {
    #[arg(short, long)]
    binary: PathBuf,

    #[arg(short, long, default_value = "can1")]
    interface: String,

    /// Target board ID as hex, e.g. 0101 for ToolDevKit.
    #[arg(short, long)]
    node_id: String,
}

fn parse_board_id(node_id: &str) -> anyhow::Result<BoardId> {
    let trimmed = node_id
        .strip_prefix("0x")
        .or_else(|| node_id.strip_prefix("0X"))
        .unwrap_or(node_id);
    let raw = u16::from_str_radix(trimmed, 16)
        .map_err(|e| anyhow::format_err!("Error parsing hex node_id '{}': {}", node_id, e))?;

    BoardId::try_from(raw).map_err(|_| {
        anyhow::format_err!("Board ID 0x{raw:04X} is not a known Trilo board ID in trilo.rs")
    })
}

fn send_frame(
    sock: &CanFdSocket,
    board_id: BoardId,
    command_id: CommandId,
    request_id: RequestId,
    data: &[u8],
) -> anyhow::Result<()> {
    let id = TriloCanId::new(
        Priority::Low,
        board_id,
        command_id,
        request_id,
        ErrorBit::NoError,
    );
    let ext_id = embedded_can::ExtendedId::new(id.to_raw_id()).ok_or_else(|| {
        anyhow::format_err!(
            "Trilo raw CAN ID 0x{:08X} is not a valid extended ID",
            id.to_raw_id()
        )
    })?;
    let frame = CanFdFrame::new(ext_id, data)
        .ok_or_else(|| anyhow::format_err!("payload is too large for a CAN FD frame"))?;
    sock.write_frame(&frame)?;
    Ok(())
}

fn read_trilo_id<F: EmbeddedFrame>(frame: &F) -> Option<TriloCanId> {
    match frame.id() {
        Id::Extended(ext_id) => TriloCanId::from_raw_id(ext_id.as_raw()).ok(),
        Id::Standard(_) => None,
    }
}

fn is_reply(
    msg_id: TriloCanId,
    board_id: BoardId,
    command_id: CommandId,
    request_id: RequestId,
) -> bool {
    msg_id.board_id == board_id
        && msg_id.command_id == command_id
        && msg_id.request_id == request_id
        && msg_id.err_bit == ErrorBit::NoError
}

fn ping_bootloader(
    sock: &CanFdSocket,
    board_id: BoardId,
    request_id: RequestId,
) -> anyhow::Result<bool> {
    send_frame(sock, board_id, CommandId::Ping, request_id, &[])?;

    match sock.read_frame() {
        Ok(rx_frame) => {
            if let Some(msg_id) = read_trilo_id(&rx_frame) {
                if is_reply(msg_id, board_id, CommandId::Ping, request_id) {
                    let status = rx_frame.data().first().copied().unwrap_or(0xFF);
                    if status == 0 {
                        println!("Device is in bootloader mode.");
                        return Ok(true);
                    }
                    println!("Unexpected ping status byte: 0x{status:02X}");
                }
            }
            Ok(false)
        }
        Err(_) => Ok(false),
    }
}

fn main() -> anyhow::Result<()> {
    let args = Args::parse();

    let sock = match CanFdSocket::open(&args.interface) {
        Ok(sock) => {
            println!("Successfully opened interface on {}", args.interface);
            sock
        }
        Err(e) => {
            return Err(anyhow!(e));
        }
    };

    let resolved_path = if args.binary.exists() {
        args.binary
    } else {
        let fallback = Path::new("./binaries").join(&args.binary);
        if fallback.exists() {
            fallback
        } else {
            args.binary
        }
    };

    println!("Attempting to read: {:?}", resolved_path);
    let binvec: Vec<u8> = match fs::read(&resolved_path) {
        Ok(bytes) => {
            println!("Success! Read {} bytes.", bytes.len());
            bytes
        }
        Err(e) => {
            return Err(anyhow::format_err!(
                "Error reading binary file {:?}: {}",
                resolved_path,
                e
            ));
        }
    };

    let nodeid = parse_board_id(&args.node_id)?;
    println!(
        "Attempting to connect to board ID: 0x{:04X} ({:?})",
        u16::from(nodeid),
        nodeid
    );

    let mut next_request_id = 1u8;
    let mut alloc_request_id = || {
        let request_id = RequestId::new(next_request_id);
        next_request_id = next_request_id.wrapping_add(1);
        request_id
    };

    println!("Pinging device 0x{:04X}...", u16::from(nodeid));
    sock.set_read_timeout(Duration::from_millis(500))?;
    let mut device_in_bootloader = ping_bootloader(&sock, nodeid, alloc_request_id())?;

    if !device_in_bootloader {
        println!("Sending Reboot command...");
        send_frame(&sock, nodeid, CommandId::Reboot, alloc_request_id(), &[])?;

        println!("Waiting for bootloader to respond to Ping (power-cycle the device if needed)...");
        sock.set_read_timeout(Duration::from_millis(250))?;
        let ping_deadline = std::time::Instant::now() + Duration::from_secs(5);
        while std::time::Instant::now() < ping_deadline {
            if ping_bootloader(&sock, nodeid, alloc_request_id())? {
                device_in_bootloader = true;
                break;
            }
            print!(".");
            use std::io::Write;
            std::io::stdout().flush().ok();
        }
        println!();
    }

    if !device_in_bootloader {
        anyhow::bail!("Device did not respond in bootloader mode");
    }

    sock.set_read_timeout(Duration::from_secs(30))?;

    println!("Sending Erase command...");
    let erase_request_id = alloc_request_id();
    send_frame(&sock, nodeid, CommandId::Erase, erase_request_id, &[])?;

    println!("Waiting for device to erase flash...");
    loop {
        let rx_frame = sock.read_frame()?;

        if let Some(msg_id) = read_trilo_id(&rx_frame) {
            if is_reply(msg_id, nodeid, CommandId::EraseOk, erase_request_id) {
                println!("Received EraseOk! Flash is ready.");
                break;
            }
        }
    }

    match write_binary(&binvec, &sock, nodeid, &mut alloc_request_id) {
        Ok(_) => {
            println!("Successfully wrote binary :D");
            println!("Sending jump command...");
            send_frame(&sock, nodeid, CommandId::Jump, alloc_request_id(), &[])?;
        }
        Err(e) => {
            anyhow::bail!(e);
        }
    }

    Ok(())
}

fn write_binary(
    binv: &[u8],
    sock: &CanFdSocket,
    targetid: BoardId,
    alloc_request_id: &mut impl FnMut() -> RequestId,
) -> anyhow::Result<()> {
    let total_chunks = binv.chunks(64).count();
    let pb = ProgressBar::new(total_chunks as u64);
    pb.set_style(
        ProgressStyle::with_template("[{elapsed_precise}] [{bar:40}] {pos}/{len} chunks ({eta})")
            .unwrap()
            .progress_chars("=>-"),
    );

    for (i, chunk) in binv.chunks(64).enumerate() {
        let chunk_address = APP_BASE_ADDRESS + (i * 64) as u32;
        let chunk_size = chunk.len() as u8;
        let request_id = alloc_request_id();

        let addr_bytes = chunk_address.to_be_bytes();
        let payload = [
            addr_bytes[0],
            addr_bytes[1],
            addr_bytes[2],
            addr_bytes[3],
            chunk_size,
        ];

        send_frame(
            sock,
            targetid,
            CommandId::AddressAndSize,
            request_id,
            &payload,
        )?;
        send_frame(sock, targetid, CommandId::Write, request_id, chunk)?;

        loop {
            let rx_frame = sock.read_frame()?;

            if let Some(msg_id) = read_trilo_id(&rx_frame) {
                if is_reply(msg_id, targetid, CommandId::WriteOk, request_id) {
                    let data = rx_frame.data();

                    if data.len() >= 5 {
                        let ack_addr = u32::from_be_bytes([data[0], data[1], data[2], data[3]]);
                        let ack_size = data[4];

                        if ack_addr == chunk_address && ack_size == chunk_size {
                            pb.inc(1);
                            break;
                        } else {
                            return Err(anyhow::format_err!(
                                "WriteOk mismatch! Expected Addr: 0x{:08X}, Size: {}. Got Addr: 0x{:08X}, Size: {}",
                                chunk_address,
                                chunk_size,
                                ack_addr,
                                ack_size
                            ));
                        }
                    }
                }
            }
        }
    }

    pb.finish_with_message("done");
    Ok(())
}
