use embassy_stm32::can::frame::FdFrame;
use embedded_can::Id;

pub struct TriloBootloaderMessage<'a> {
    pub bl_command: BootloaderCommandId,
    pub bl_data: &'a [u8],
}

impl<'a> TryInto<FdFrame> for TriloBootloaderMessage<'a> {
    type Error = ();

    fn try_into(self) -> Result<FdFrame, ()> {
        FdFrame::new_extended(self.bl_command as u32, self.bl_data).map_err(|_| ())
    }
}

impl<'a> TryFrom<&'a FdFrame> for TriloBootloaderMessage<'a> {
    type Error = ();

    fn try_from(frame: &'a FdFrame) -> Result<Self, ()> {
        let raw_id = match frame.id() {
            Id::Standard(_) => return Err(()),
            Id::Extended(ext_id) => ext_id.as_raw(),
        };
        let bl_command = BootloaderCommandId::try_from(raw_id as u8)?;
        Ok(Self {
            bl_command,
            bl_data: frame.data(),
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
