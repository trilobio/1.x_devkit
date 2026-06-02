use anyhow::anyhow;
use clap::Parser;
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

    #[arg(short, long)]
    node_id: u16,
}

fn send_message(sock: &CanFdSocket, msg: TriloBootloaderMessage) -> anyhow::Result<()> {
    let frame: CanFdFrame = msg.try_into()?;
    sock.write_frame(&frame)?;
    Ok(())
}

fn read_message(sock: &CanFdSocket) -> anyhow::Result<TriloBootloaderMessage> {
    let frame = sock.read_frame()?;
    TriloBootloaderMessage::try_from(&frame)
}

fn ping_bootloader(sock: &CanFdSocket) -> anyhow::Result<bool> {
    send_message(
        sock,
        TriloBootloaderMessage {
            bl_command: BootloaderCommandId::BlPing,
            bl_data: vec![],
        },
    )?;

    match read_message(sock) {
        Ok(msg) if msg.bl_command == BootloaderCommandId::BlPingOk => {
            let status = msg.bl_data.first().copied().unwrap_or(0xFF);
            if status == 0 {
                println!("Device is in bootloader mode.");
                Ok(true)
            } else {
                println!("Unexpected ping status byte: 0x{status:02X}");
                Ok(false)
            }
        }
        Ok(_) | Err(_) => Ok(false),
    }
}

fn main() -> anyhow::Result<()> {
    let args = Args::parse();

    let sock = match CanFdSocket::open(&args.interface) {
        Ok(sock) => {
            println!("Successfully opened interface on {}", args.interface);
            sock
        }
        Err(e) => return Err(anyhow!(e)),
    };

    println!("Target node ID: 0x{:04X}", args.node_id);

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
    let binvec = fs::read(&resolved_path)
        .map_err(|e| anyhow::format_err!("Error reading binary file {:?}: {}", resolved_path, e))?;
    println!("Success! Read {} bytes.", binvec.len());

    println!("Pinging device...");
    sock.set_read_timeout(Duration::from_millis(500))?;
    if !ping_bootloader(&sock)? {
        anyhow::bail!("Device did not respond in bootloader mode");
    }

    sock.set_read_timeout(Duration::from_secs(30))?;

    println!("Sending Erase command...");
    send_message(
        &sock,
        TriloBootloaderMessage {
            bl_command: BootloaderCommandId::BlErase,
            bl_data: vec![],
        },
    )?;

    println!("Waiting for device to erase flash...");
    loop {
        let msg = read_message(&sock)?;
        if msg.bl_command == BootloaderCommandId::BlEraseOk {
            println!("Received EraseOk! Flash is ready.");
            break;
        }
    }

    write_binary(&binvec, &sock)?;

    println!("Successfully wrote binary :D");
    println!("Sending jump command...");
    send_message(
        &sock,
        TriloBootloaderMessage {
            bl_command: BootloaderCommandId::BlJump,
            bl_data: vec![],
        },
    )?;

    Ok(())
}

fn write_binary(binv: &[u8], sock: &CanFdSocket) -> anyhow::Result<()> {
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

        let mut payload = Vec::with_capacity(5);
        payload.extend_from_slice(&chunk_address.to_be_bytes());
        payload.push(chunk_size);

        send_message(
            sock,
            TriloBootloaderMessage {
                bl_command: BootloaderCommandId::BlAddressAndSize,
                bl_data: payload,
            },
        )?;
        send_message(
            sock,
            TriloBootloaderMessage {
                bl_command: BootloaderCommandId::BlWrite,
                bl_data: chunk.to_vec(),
            },
        )?;

        loop {
            let msg = read_message(sock)?;
            if msg.bl_command != BootloaderCommandId::BlWriteOk {
                continue;
            }

            let data = msg.bl_data;
            if data.len() < 5 {
                continue;
            }

            let ack_addr = u32::from_be_bytes([data[0], data[1], data[2], data[3]]);
            let ack_size = data[4];

            if ack_addr == chunk_address && ack_size == chunk_size {
                pb.inc(1);
                break;
            }

            return Err(anyhow::format_err!(
                "WriteOk mismatch! Expected Addr: 0x{:08X}, Size: {}. Got Addr: 0x{:08X}, Size: {}",
                chunk_address,
                chunk_size,
                ack_addr,
                ack_size
            ));
        }
    }

    pb.finish_with_message("done");
    Ok(())
}
