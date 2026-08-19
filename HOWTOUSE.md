# Powered Deck Module GPIO walkthrough

Board ID used below: `110`.

There are three separate operations:

- Set GPIO mode with `uv run scripts/set_pin_mode.py`.
- Read GPIO state with `uv run scripts/read_pin_state.py`.
- Set a GPIO high/low with `uv run scripts/set_pin_state.py`.

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
# PA0: port=0, pin=0, mode=input
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_mode.py --host 192.168.8.200 --client-port 8084 110 0 0 0
```

Example: configure PA0 as an output.

```bash
# PA0: port=0, pin=0, mode=output
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_mode.py --host 192.168.8.200 --client-port 8084 110 0 0 1
```

The response should report `status=0, error_code=1`.

Port and pin values are:

```text
PORT_A = 0       PORT_B = 1       LEDS = 2
PA0    = (0, 0)  PB0    = (1, 0)  LED1 = (2, 0)
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
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/read_pin_state.py --host 192.168.8.200 --client-port 8084 110 0 0
```

More selectors:

```bash
# PB0: port=1, pin=0
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/read_pin_state.py --host 192.168.8.200 --client-port 8084 110 1 0

# PA12: port=0, pin=12
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/read_pin_state.py --host 192.168.8.200 --client-port 8084 110 0 12

# PB12: port=1, pin=12
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/read_pin_state.py --host 192.168.8.200 --client-port 8084 110 1 12
```

For a pin configured as an input, drive it externally before reading it. With no external
pull-up or pull-down, the result can float.

## 3. Set a pin high/low through the arb-message tunnel

`uv run scripts/set_pin_state.py` tunnels raw bytes through the legacy
`WRITE_ENCODER_CALIBRATION` command.

The payload format consumed by the firmware is:

```text
byte 0: port
byte 1: pin
byte 2: state
```

Port values:

```text
0 = GPIO port A
1 = GPIO port B
2 = LEDs
```

LED pin values are zero-based: `0 = LED1`, `1 = LED2`, `2 = LED3`.

State values:

```text
0     = LOW
nonzero = HIGH
```

### PA0 high and low

First configure PA0 as an output:

```bash
# PA0: port=0, pin=0, mode=output
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_mode.py --host 192.168.8.200 --client-port 8084 110 0 0 1
```

PA0 high:

```bash
# PA0: port=0, pin=0, state=HIGH
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_state.py --host 192.168.8.200 --client-port 8084 110 0 0 1
```

Payload breakdown:

```text
00 = GPIO port A
00 = pin 0
01 = HIGH
```

PA0 low:

```bash
# PA0: port=0, pin=0, state=LOW
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_state.py --host 192.168.8.200 --client-port 8084 110 0 0 0
```

Payload breakdown:

```text
00 = GPIO port A
00 = pin 0
00 = LOW
```

Verify either state:

```bash
# PA0: port=0, pin=0
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/read_pin_state.py --host 192.168.8.200 --client-port 8084 110 0 0
```

### PB0 high and low

```bash
# PB0: port=1, pin=0, state=HIGH
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_state.py --host 192.168.8.200 --client-port 8084 110 1 0 1

# PB0: port=1, pin=0, state=LOW
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_state.py --host 192.168.8.200 --client-port 8084 110 1 0 0
```

### LEDs

LEDs use port `2` and zero-based LED pins:

```bash
# LED1: port=2, pin=0, state=HIGH
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_state.py --host 192.168.8.200 --client-port 8084 110 2 0 1

# LED2: port=2, pin=1, state=HIGH
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_state.py --host 192.168.8.200 --client-port 8084 110 2 1 1

# LED3: port=2, pin=2, state=LOW
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_state.py --host 192.168.8.200 --client-port 8084 110 2 2 0
```

## Complete PA0 test

```bash
# PA0: port=0, pin=0, mode=output
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_mode.py --host 192.168.8.200 --client-port 8084 110 0 0 1

# PA0: port=0, pin=0, state=HIGH
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_state.py --host 192.168.8.200 --client-port 8084 110 0 0 1

# PA0: port=0, pin=0; expected output HIGH
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/read_pin_state.py --host 192.168.8.200 --client-port 8084 110 0 0

# PA0: port=0, pin=0, state=LOW
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/set_pin_state.py --host 192.168.8.200 --client-port 8084 110 0 0 0

# PA0: port=0, pin=0; expected output LOW
# On fleet controller/laptop          Trilobot ip vv      Grpc port vv
uv run scripts/read_pin_state.py --host 192.168.8.200 --client-port 8084 110 0 0
```
