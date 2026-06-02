#![no_std]
#![no_main]

// mod protocol;
// use protocol::*;

mod trilo;
use trilo::*;

use core::num::{NonZeroU8, NonZeroU16};
use embassy_executor::Spawner;
use embassy_stm32::can::config::{
    ClockDivider, DataBitTiming, FdCanConfig, FrameTransmissionConfig, NominalBitTiming,
    TxBufferMode,
};
use embassy_stm32::peripherals::*;
use embassy_stm32::{Config, bind_interrupts, can, can::filter::*, flash, rcc};
use embedded_can::Id;
use panic_probe as _;
use rtt_target::{rprintln, rtt_init_print};

bind_interrupts!(struct Irqs {
    FDCAN2_IT0 => can::IT0InterruptHandler<FDCAN2>;
    FDCAN2_IT1 => can::IT1InterruptHandler<FDCAN2>;
});

unsafe fn jump_to_app() -> ! {
    unsafe {
        let mut p = cortex_m::Peripherals::steal();

        cortex_m::interrupt::disable();

        p.SYST.disable_counter();
        p.SYST.disable_interrupt();

        for i in 0..16 {
            p.NVIC.icer[i].write(0xFFFF_FFFF);
            p.NVIC.icpr[i].write(0xFFFF_FFFF);
        }

        p.SCB.invalidate_icache();
        p.SCB.vtor.write(0x0800_8000);
        cortex_m::asm::dsb();
        cortex_m::asm::isb();

        cortex_m::interrupt::enable();
        cortex_m::asm::bootload(0x0800_8000 as *const u32);
    }
}

pub const THIS_NODE: BoardId = BoardId::ToolDevKit;

#[embassy_executor::main]
async fn main(_spawner: Spawner) {
    let mut config = Config::default();

    // 1. Configure HSI (64MHz) as the PLL source
    config.rcc.hsi = Some(rcc::HSIPrescaler::DIV1);

    // 2. Configure PLL1: HSI(64)/M(4) * N(30) = 480MHz VCO
    config.rcc.pll1 = Some(rcc::Pll {
        source: rcc::PllSource::HSI,
        prediv: rcc::PllPreDiv::DIV4,   // 64 / 4 = 16MHz input
        mul: rcc::PllMul::MUL30,        // 16 * 30 = 480MHz VCO
        divp: Some(rcc::PllDiv::DIV2),  // 480 / 2 = 240MHz (System Core)
        divq: Some(rcc::PllDiv::DIV12), // 480 / 12 = 40MHz (FDCAN)
        divr: None,
    });

    // 3. Set System Clock and FDCAN Mux
    config.rcc.sys = rcc::Sysclk::PLL1_P;
    config.rcc.mux.fdcan12sel = rcc::mux::Fdcansel::PLL1_Q;

    let peripherals = embassy_stm32::init(config);

    rtt_init_print!();

    match THIS_NODE {
        BoardId::ToolDevKit => rprintln!("Tool Devkit"),
        _ => {}
    }

    let filter_all = ExtendedFilter {
        filter: FilterType::BitMask {
            filter: TriloCanId::board_id_filter_bits(BoardId::AllCall),
            mask: TriloCanId::BOARD_ID_FILTER_MASK,
        },
        action: can::filter::Action::StoreInFifo0,
    };
    let filter_this_node = ExtendedFilter {
        filter: FilterType::BitMask {
            filter: TriloCanId::board_id_filter_bits(THIS_NODE),
            mask: TriloCanId::BOARD_ID_FILTER_MASK,
        },
        action: can::filter::Action::StoreInFifo0,
    };

    let mut can =
        can::CanConfigurator::new(peripherals.FDCAN2, peripherals.PB5, peripherals.PB13, Irqs);
    can.properties()
        .set_extended_filter(ExtendedFilterSlot::_0, filter_all);
    can.properties()
        .set_extended_filter(ExtendedFilterSlot::_1, filter_this_node);
    let config = FdCanConfig::default()
        .set_clock_divider(ClockDivider::_1)
        .set_frame_transmit(FrameTransmissionConfig::AllowFdCanAndBRS)
        .set_automatic_retransmit(false)
        .set_transmit_pause(false)
        .set_protocol_exception_handling(false)
        .set_tx_buffer_mode(TxBufferMode::Fifo)
        .set_nominal_bit_timing(NominalBitTiming {
            prescaler: NonZeroU16::new(1).unwrap(),
            sync_jump_width: NonZeroU8::new(10).unwrap(),
            seg1: NonZeroU8::new(139).unwrap(),
            seg2: NonZeroU8::new(20).unwrap(),
        })
        .set_data_bit_timing(DataBitTiming {
            transceiver_delay_compensation: true,
            prescaler: NonZeroU16::new(1).unwrap(),
            seg1: NonZeroU8::new(29).unwrap(),
            seg2: NonZeroU8::new(10).unwrap(),
            sync_jump_width: NonZeroU8::new(5).unwrap(),
        });

    can.set_config(config);
    rprintln!("Initialized CAN");
    #[allow(unused_mut)]
    let mut can = can.into_normal_mode();
    let (mut tx, mut rx, _props) = can.split();

    #[allow(unused_mut)]
    let mut flash = flash::Flash::new_blocking(peripherals.FLASH).into_blocking_regions();
    let mut chunk_address: u32 = 0;
    let mut chunk_size: u8 = 0;
    let mut f = flash.bank1_region;

    loop {
        match rx.read_fd().await {
            Ok(message) => {
                let (rx_frame, _ts) = message.parts();
                rprintln!("{:?}", rx_frame.id());
                if let Id::Extended(id) = rx_frame.id() {
                    let raw_id = id.as_raw();
                    let Ok(can_msg) = TriloCanId::from_raw_id(raw_id) else {
                        continue;
                    };
                    if can_msg.board_id == THIS_NODE || can_msg.board_id == BoardId::AllCall {
                        let data = rx_frame.data();

                        match can_msg.command_id {
                            CommandId::Ping => {
                                let reply_id = TriloCanId::new(
                                    Priority::Low,
                                    THIS_NODE,
                                    CommandId::Ping,
                                    can_msg.request_id,
                                    ErrorBit::NoError,
                                );
                                let tx_frame = embassy_stm32::can::frame::FdFrame::new_extended(
                                    reply_id.to_raw_id(),
                                    &[0u8],
                                )
                                .unwrap();
                                tx.write_fd(&tx_frame).await;
                            }
                            CommandId::Erase => {
                                f.blocking_erase(0x8000, 0x80000).unwrap();
                                let reply_id = TriloCanId::new(
                                    Priority::Low,
                                    THIS_NODE,
                                    CommandId::EraseOk,
                                    can_msg.request_id,
                                    ErrorBit::NoError,
                                );
                                let tx_frame = embassy_stm32::can::frame::FdFrame::new_extended(
                                    reply_id.to_raw_id(),
                                    &[],
                                )
                                .unwrap();
                                tx.write_fd(&tx_frame).await;
                            }
                            CommandId::AddressAndSize => {
                                if data.len() >= 5 {
                                    chunk_address =
                                        u32::from_be_bytes([data[0], data[1], data[2], data[3]]);
                                    chunk_size = data[4];
                                }
                            }
                            CommandId::Write => {
                                if chunk_address >= 0x0800_8000 {
                                    let offset = chunk_address - 0x0800_0000;

                                    let mut write_buf = [0xFF; 64];

                                    let aligned_len = (data.len() + 15) & !15;

                                    if aligned_len <= 64 {
                                        write_buf[..data.len()].copy_from_slice(data);

                                        f.blocking_write(offset, &write_buf[..aligned_len])
                                            .unwrap();

                                        let mut payload = [0u8; 5];
                                        payload[0..4].copy_from_slice(&chunk_address.to_be_bytes());
                                        payload[4] = chunk_size;

                                        let reply_id = TriloCanId::new(
                                            Priority::Low,
                                            THIS_NODE,
                                            CommandId::WriteOk,
                                            can_msg.request_id,
                                            ErrorBit::NoError,
                                        );

                                        let tx_frame =
                                            embassy_stm32::can::frame::FdFrame::new_extended(
                                                reply_id.to_raw_id(),
                                                &payload,
                                            )
                                            .unwrap();
                                        tx.write_fd(&tx_frame).await;

                                        chunk_address += data.len() as u32;
                                    }
                                }
                            }
                            CommandId::Jump => unsafe {
                                jump_to_app();
                            },
                            _ => {}
                        }
                    }
                }
            }
            Err(_e) => {}
        }
    }
}
