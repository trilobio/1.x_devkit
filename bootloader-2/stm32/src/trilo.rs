/// Trilo 29-bit extended CAN identifier.
///
/// Bit layout, MSB first:
/// [priority:2][board_id:10][command_id:8][request_id:8][err_bit:1]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TriloCanId {
    pub priority: Priority,
    pub board_id: BoardId,
    pub command_id: CommandId,
    pub request_id: RequestId,
    pub err_bit: ErrorBit,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum Priority {
    Critical = 0,
    High = 1,
    Medium = 2,
    Low = 3,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u16)]
pub enum BoardId {
    AllCall = 0x00,
    ToolDevKit = 0x0101,
    Pdm1 = 0x0102,
    Pdm2 = 0x0103,
    Pds1 = 0x0202,
    Pds2 = 0x0203,
    Unknown = 0x03FF,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum CommandId {
    Ping = 0x40,
    Erase = 0x45,
    EraseOk = 0x46,
    Write = 0x47,
    WriteOk = 0x48,
    AddressAndSize = 0x4A,
    FirmwareUpdateQuery = 0x4B,
    FirmwareUpdateResponse = 0x4C,
    Reboot = 0x4D,
    Jump = 0xAA,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct RequestId(pub u8);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum ErrorBit {
    NoError = 0,
    HasError = 1,
}

impl TriloCanId {
    pub const PRIORITY_BITS: usize = 2;
    pub const BOARD_ID_BITS: usize = 10;
    pub const COMMAND_ID_BITS: usize = 8;
    pub const REQUEST_ID_BITS: usize = 8;
    pub const ERR_BIT_BITS: usize = 1;
    pub const SIZE_BITS: usize = Self::PRIORITY_BITS
        + Self::BOARD_ID_BITS
        + Self::COMMAND_ID_BITS
        + Self::REQUEST_ID_BITS
        + Self::ERR_BIT_BITS;

    pub const PRIORITY_MASK: u8 = (1 << Self::PRIORITY_BITS) - 1;
    pub const BOARD_ID_MASK: u16 = (1 << Self::BOARD_ID_BITS) - 1;
    pub const COMMAND_ID_MASK: u8 = u8::MAX;
    pub const REQUEST_ID_MASK: u8 = u8::MAX;
    pub const ERR_BIT_MASK: u8 = 0x01;
    pub const RAW_ID_MASK: u32 = (1 << Self::SIZE_BITS) - 1;

    pub const PRIORITY_SHIFT: usize =
        Self::BOARD_ID_BITS + Self::COMMAND_ID_BITS + Self::REQUEST_ID_BITS + Self::ERR_BIT_BITS;
    pub const BOARD_ID_SHIFT: usize =
        Self::COMMAND_ID_BITS + Self::REQUEST_ID_BITS + Self::ERR_BIT_BITS;
    pub const COMMAND_ID_SHIFT: usize = Self::REQUEST_ID_BITS + Self::ERR_BIT_BITS;
    pub const REQUEST_ID_SHIFT: usize = Self::ERR_BIT_BITS;
    pub const BOARD_ID_FILTER_MASK: u32 = (Self::BOARD_ID_MASK as u32) << Self::BOARD_ID_SHIFT;

    pub const fn board_id_filter_bits(board_id: BoardId) -> u32 {
        (board_id as u32) << Self::BOARD_ID_SHIFT
    }

    pub const fn new(
        priority: Priority,
        board_id: BoardId,
        command_id: CommandId,
        request_id: RequestId,
        err_bit: ErrorBit,
    ) -> Self {
        Self {
            priority,
            board_id,
            command_id,
            request_id,
            err_bit,
        }
    }

    pub fn to_raw_id(&self) -> u32 {
        ((u8::from(self.priority) as u32) << Self::PRIORITY_SHIFT)
            | ((u16::from(self.board_id) as u32) << Self::BOARD_ID_SHIFT)
            | ((u8::from(self.command_id) as u32) << Self::COMMAND_ID_SHIFT)
            | ((u8::from(self.request_id) as u32) << Self::REQUEST_ID_SHIFT)
            | u8::from(self.err_bit) as u32
    }

    pub fn from_raw_id(raw_id: u32) -> Result<Self, ()> {
        let raw_id = raw_id & Self::RAW_ID_MASK;

        let priority = ((raw_id >> Self::PRIORITY_SHIFT) & Self::PRIORITY_MASK as u32) as u8;
        let board_id = ((raw_id >> Self::BOARD_ID_SHIFT) & Self::BOARD_ID_MASK as u32) as u16;
        let command_id = ((raw_id >> Self::COMMAND_ID_SHIFT) & Self::COMMAND_ID_MASK as u32) as u8;
        let request_id = ((raw_id >> Self::REQUEST_ID_SHIFT) & Self::REQUEST_ID_MASK as u32) as u8;
        let err_bit = (raw_id & Self::ERR_BIT_MASK as u32) as u8;

        Ok(Self {
            priority: Priority::try_from(priority)?,
            board_id: BoardId::try_from(board_id).unwrap_or(BoardId::Unknown),
            command_id: CommandId::try_from(command_id)?,
            request_id: RequestId::new(request_id),
            err_bit: ErrorBit::try_from(err_bit)?,
        })
    }
}

impl TryFrom<u8> for Priority {
    type Error = ();

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            x if x == Priority::Critical as u8 => Ok(Priority::Critical),
            x if x == Priority::High as u8 => Ok(Priority::High),
            x if x == Priority::Medium as u8 => Ok(Priority::Medium),
            x if x == Priority::Low as u8 => Ok(Priority::Low),
            _ => Err(()),
        }
    }
}

impl From<Priority> for u8 {
    fn from(priority: Priority) -> Self {
        priority as u8
    }
}

impl TryFrom<u16> for BoardId {
    type Error = ();

    fn try_from(value: u16) -> Result<Self, Self::Error> {
        match value {
            x if x == BoardId::AllCall as u16 => Ok(BoardId::AllCall),
            x if x == BoardId::ToolDevKit as u16 => Ok(BoardId::ToolDevKit),
            x if x == BoardId::Pdm1 as u16 => Ok(BoardId::Pdm1),
            x if x == BoardId::Pdm2 as u16 => Ok(BoardId::Pdm2),
            x if x == BoardId::Pds1 as u16 => Ok(BoardId::Pds1),
            x if x == BoardId::Pds2 as u16 => Ok(BoardId::Pds2),
            x if x == BoardId::Unknown as u16 => Ok(BoardId::Unknown),
            _ => Err(()),
        }
    }
}

impl From<BoardId> for u16 {
    fn from(board_id: BoardId) -> Self {
        board_id as u16
    }
}

impl TryFrom<u8> for CommandId {
    type Error = ();

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            x if x == CommandId::Ping as u8 => Ok(CommandId::Ping),
            x if x == CommandId::Erase as u8 => Ok(CommandId::Erase),
            x if x == CommandId::EraseOk as u8 => Ok(CommandId::EraseOk),
            x if x == CommandId::Write as u8 => Ok(CommandId::Write),
            x if x == CommandId::WriteOk as u8 => Ok(CommandId::WriteOk),
            x if x == CommandId::AddressAndSize as u8 => Ok(CommandId::AddressAndSize),
            x if x == CommandId::FirmwareUpdateQuery as u8 => Ok(CommandId::FirmwareUpdateQuery),
            x if x == CommandId::FirmwareUpdateResponse as u8 => {
                Ok(CommandId::FirmwareUpdateResponse)
            }
            x if x == CommandId::Reboot as u8 => Ok(CommandId::Reboot),
            x if x == CommandId::Jump as u8 => Ok(CommandId::Jump),
            _ => Err(()),
        }
    }
}

impl From<CommandId> for u8 {
    fn from(command_id: CommandId) -> Self {
        command_id as u8
    }
}

impl RequestId {
    pub const MIN: u8 = u8::MIN;
    pub const MAX: u8 = u8::MAX;

    pub const fn new(value: u8) -> Self {
        Self(value)
    }
}

impl From<RequestId> for u8 {
    fn from(request_id: RequestId) -> Self {
        request_id.0
    }
}

impl TryFrom<u8> for ErrorBit {
    type Error = ();

    fn try_from(value: u8) -> Result<Self, <Self as TryFrom<u8>>::Error> {
        match value {
            x if x == ErrorBit::NoError as u8 => Ok(ErrorBit::NoError),
            x if x == ErrorBit::HasError as u8 => Ok(ErrorBit::HasError),
            _ => Err(()),
        }
    }
}

impl From<ErrorBit> for u8 {
    fn from(error: ErrorBit) -> Self {
        error as u8
    }
}
