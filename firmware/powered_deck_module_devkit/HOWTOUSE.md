# Powered Deck Module GPIO walkthrough

Board ID used below: `110`.

There are three separate operations:

- Set GPIO mode with `raspi/scripts/set_pin_mode.py`.
- Read GPIO state with `raspi/scripts/read_pin_state.py`.
- Set a GPIO high/low with `raspi/scripts/set_pin_state.py`.

## 1. Set a pin mode

`set_light_levels()` is repurposed as:

```text
set_light_levels(board_id, port, pin, mode, filler)
```

The fields are sent as four bytes:

```text
[port, pin, mode, filler]
```

Mode values:

```text
0 = input
1 = push-pull output
```

Example: configure PA0 as an input.

```bash
python raspi/scripts/set_pin_mode.py 110 0 0 0
```

Example: configure PA0 as an output.

```bash
python raspi/scripts/set_pin_mode.py 110 0 0 1
```

The response should report `status=0, error_code=1`.

Port and pin values are:

```text
PORT_A = 0       PORT_B = 1
PA0    = (0, 0)  PB0    = (1, 0)
```

The pin number is an index, not a GPIO mask. Use `0` for pin 0, `12` for pin 12, etc.

## 2. Read a pin state

`read_probe()` is repurposed with a packed one-byte selector:

```text
selector = (port << 4) | pin
```

The script prints `HIGH` or `LOW`:

```bash
# PA0: port=0, pin=0
python raspi/scripts/read_pin_state.py 110 0 0
```

More selectors:

```bash
python raspi/scripts/read_pin_state.py 110 1 0   # PB0
python raspi/scripts/read_pin_state.py 110 0 12  # PA12
python raspi/scripts/read_pin_state.py 110 1 12  # PB12
```

For a pin configured as an input, drive it externally before reading it. With no external
pull-up or pull-down, the result can float.

## 3. Set a pin high/low through the arb-message tunnel

`set_pin_state.py` tunnels raw bytes through the legacy `WRITE_ENCODER_CALIBRATION`
command.

The payload format consumed by the firmware is:

```text
byte 0: group
byte 1: pin
byte 2: state
```

Group values:

```text
0 = LED group
1 = PORT_A
2 = PORT_B
```

State values:

```text
0     = LOW
nonzero = HIGH
```

### PA0 high and low

First configure PA0 as an output:

```bash
python raspi/scripts/set_pin_mode.py 110 0 0 1
```

PA0 high:

```bash
python raspi/scripts/set_pin_state.py 110 0 0 1
```

Payload breakdown:

```text
01 = PORT_A
00 = pin 0
01 = HIGH
```

PA0 low:

```bash
python raspi/scripts/set_pin_state.py 110 0 0 0
```

Payload breakdown:

```text
01 = PORT_A
00 = pin 0
00 = LOW
```

Verify either state:

```bash
python raspi/scripts/read_pin_state.py 110 0 0
```

### PB0 high and low

```bash
# PB0 high
python raspi/scripts/set_pin_state.py 110 1 0 1

# PB0 low
python raspi/scripts/set_pin_state.py 110 1 0 0
```

## Complete PA0 test

```bash
python raspi/scripts/set_pin_mode.py 110 0 0 1
python raspi/scripts/set_pin_state.py 110 0 0 1
python raspi/scripts/read_pin_state.py 110 0 0  # HIGH
python raspi/scripts/set_pin_state.py 110 0 0 0
python raspi/scripts/read_pin_state.py 110 0 0  # LOW
```
