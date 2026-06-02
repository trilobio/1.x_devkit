use embedded_can::{Frame, Id};
use socketcan::{CanAnyFrame, CanFdFrame};

pub struct TriloBootloaderMessage {
    pub bl_command: BootloaderCommandId,
    pub bl_data: Vec<u8>,
}

impl TryInto<CanFdFrame> for TriloBootloaderMessage {
    type Error = anyhow::Error;

    fn try_into(self) -> Result<CanFdFrame, anyhow::Error> {
        let ext_id = embedded_can::ExtendedId::new(self.bl_command as u32)
            .ok_or_else(|| anyhow::anyhow!("Invalid extended CAN ID"))?;
        CanFdFrame::new(ext_id, &self.bl_data)
            .ok_or_else(|| anyhow::anyhow!("payload is too large for a CAN FD frame"))
    }
}

impl TryFrom<CanAnyFrame> for TriloBootloaderMessage {
    type Error = anyhow::Error;

    fn try_from(frame: CanAnyFrame) -> Result<Self, anyhow::Error> {
        Self::try_from(&frame)
    }
}

impl TryFrom<&CanAnyFrame> for TriloBootloaderMessage {
    type Error = anyhow::Error;

    fn try_from(frame: &CanAnyFrame) -> Result<Self, anyhow::Error> {
        match frame {
            CanAnyFrame::Fd(frame) => Self::try_from(frame),
            _ => Err(anyhow::anyhow!("Unsupported frame type")),
        }
    }
}

impl TryFrom<CanFdFrame> for TriloBootloaderMessage {
    type Error = anyhow::Error;

    fn try_from(frame: CanFdFrame) -> Result<Self, anyhow::Error> {
        Self::try_from(&frame)
    }
}

impl TryFrom<&CanFdFrame> for TriloBootloaderMessage {
    type Error = anyhow::Error;

    fn try_from(frame: &CanFdFrame) -> Result<Self, anyhow::Error> {
        let raw_id = match frame.id() {
            Id::Standard(_) => return Err(anyhow::anyhow!("Standard CAN ID not supported")),
            Id::Extended(ext_id) => ext_id.as_raw(),
        };
        let bl_command = BootloaderCommandId::try_from(raw_id as u8)
            .map_err(|()| anyhow::anyhow!("Invalid bootloader command ID: 0x{raw_id:08X}"))?;
        Ok(Self {
            bl_command,
            bl_data: frame.data().to_vec(),
        })
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum BootloaderCommandId {
    BlPing = 0xF5,
    BlPingOk = 0xF6,
    BlErase = 0xF7,
    BlEraseOk = 0xF8,
    BlWrite = 0xF9,
    BlWriteOk = 0xFA,
    BlAddressAndSize = 0xFB,
    BlJump = 0xFF,
}

impl TryFrom<u8> for BootloaderCommandId {
    type Error = ();

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            x if x == BootloaderCommandId::BlPing as u8 => Ok(BootloaderCommandId::BlPing),
            x if x == BootloaderCommandId::BlPingOk as u8 => Ok(BootloaderCommandId::BlPingOk),
            x if x == BootloaderCommandId::BlErase as u8 => Ok(BootloaderCommandId::BlErase),
            x if x == BootloaderCommandId::BlEraseOk as u8 => Ok(BootloaderCommandId::BlEraseOk),
            x if x == BootloaderCommandId::BlWrite as u8 => Ok(BootloaderCommandId::BlWrite),
            x if x == BootloaderCommandId::BlWriteOk as u8 => Ok(BootloaderCommandId::BlWriteOk),
            x if x == BootloaderCommandId::BlAddressAndSize as u8 => {
                Ok(BootloaderCommandId::BlAddressAndSize)
            }
            x if x == BootloaderCommandId::BlJump as u8 => Ok(BootloaderCommandId::BlJump),
            _ => Err(()),
        }
    }
}

impl From<BootloaderCommandId> for u8 {
    fn from(bl_id: BootloaderCommandId) -> Self {
        bl_id as u8
    }
}
