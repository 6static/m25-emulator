# m25-emulator

Protocol-accurate Alber e-motion M25 wheelchair wheel emulator running on
ESP32-WROOM-32. Speaks the real M25 binary protocol (AES-128-CBC encrypted
frames, CRC-16, byte stuffing) over BLE GATT and/or Bluetooth Classic SPP
so that the official app and Python tooling cannot distinguish it from a
real wheel.

---

## Design

- One module per concern. All logic lives in header files; `m25-emulator.ino` contains only `setup()` and `loop()`
- State passed explicitly with no hidden globals; each module exposes a minimal API
- Runtime wheel-side selection -- left/right stored in ESP32 NVS, survives
  re-flash, no recompile needed
- Keys never in source -- injected from a local `.env` at build time via
  `load_env.py`

---

## File structure

| File | Responsibility |
|------|----------------|
| `config.h` | All compile-time settings (name, key, pins, timeouts, feature flags) |
| `nvs_config.h` | `WheelRuntimeConfig`: load/save wheel side from ESP32 NVS |
| `state.h` | `WheelState` struct and pure mutation helpers |
| `protocol.h` | CRC-16, byte stuffing/unstuffing, frame encode/decode |
| `crypto.h` | AES-128 ECB/CBC wrappers (mbedtls) |
| `packet.h` | Full M25 packet decode (incoming) and response encode (outgoing) |
| `command.h` | Map SPP service/param IDs to `WheelState` updates |
| `transport_rfcomm.h` | Bluetooth Classic SPP via `BluetoothSerial` |
| `transport_ble.h` | BLE GATT (controlled by `TRANSPORT_BLE_ENABLED`) |
| `led.h` | LED visual feedback (battery level, connection, speed blink) |
| `buzzer.h` | Active and passive buzzer audio feedback |
| `cli.h` | Serial command-line interface; delegates transport actions via callbacks |
| `safety.h` | Command timeout watchdog, battery drain timer, button debounce |
| `load_env.py` | PlatformIO pre-build script: inject AES keys and git metadata |

---

## Quick start

### 1. Set encryption keys

Create a `.env` file next to `platformio.ini`:

```
M25_LEFT_KEY=A1B2C3D4E5F6...   # 32 hex characters
M25_RIGHT_KEY=C1D2E3F4A5B6...
```

`load_env.py` picks these up at compile time. Without a `.env`, the
zero-byte fallback keys from `config.h` are used.

### 2. Set upload port

In `platformio.ini`:

```ini
upload_port = COM8   ; change to your actual port
```

### 3. Build and upload

```
pio run -t upload
pio device monitor
```

### 4. Set wheel side (NVS)

Run once after flashing via the serial monitor:

```
config set left    ; or: config set right
```

Stored in ESP32 NVS under namespace `m25wheel`. Survives reboot and
re-upload until you explicitly change it:

```
config show        ; show active side and whether it came from NVS or build default
config reset       ; clear NVS and reboot
```

No saved side means the firmware falls back to `WHEEL_SIDE_DEFAULT` in
`config.h` (left by default).

---

## Transport selection

Two flags in `config.h`:

```cpp
#define TRANSPORT_RFCOMM_ENABLED  0   // Bluetooth Classic SPP -- M25v1
#define TRANSPORT_BLE_ENABLED     1   // BLE GATT              -- M25v2
```

Both can run at the same time. The emulator decodes frames from either
transport the same way and sends ACKs back on whichever is connected.

| Transport | Wheel generation | Default |
|-----------|-----------------|---------|
| BLE GATT | M25v2 | enabled |
| RFCOMM (SPP) | M25v1 | disabled |

### RFCOMM channel note (M25v1)

M25v1 wheels advertise on RFCOMM channel 6 (confirmed). `BluetoothSerial`
assigns channels automatically. Check the serial monitor at startup before
connecting:

```
[RFCOMM] NOTE: Check actual RFCOMM channel in BT stack output.
[RFCOMM] m25_spp.py expects channel 6; update if needed.
```

---

## Serial CLI commands

| Command | Description |
|---------|-------------|
| `help` | List all commands |
| `status` | Show current wheel state |
| `mac` | Show Bluetooth and base MAC |
| `whoami` | Show device identity, wheel side, transports, MAC |
| `version` | Show firmware version, git hash, build timestamp |
| `uptime` | Show uptime and last reset reason |
| `stats` | Show runtime counters and diagnostics |
| `key` | Show active encryption key |
| `config [show]` | Show active wheel-side configuration |
| `config set left\|right` | Persist wheel side and reboot |
| `config reset` | Clear saved side and reboot |
| `hardware` | Show pin assignments |
| `battery [0-100]` | Get or set battery level |
| `speed <val>` | Get or set raw speed |
| `assist [0-2]` | Get or set assist level |
| `profile [0-5]` | Get or set drive profile |
| `hillhold [on\|off]` | Get or set hill hold |
| `rotate [n]` | Simulate n rotations |
| `reset` | Reset rotation counter |
| `debug [flag\|all\|none]` | Toggle debug flags |
| `audio [on\|off]` | Toggle audio feedback |
| `visual [on\|off]` | Toggle speed LED |
| `beep [1-10]` | Play beeps |
| `tone <freq>` | Play tone (Hz) |
| `send` | Send ACK now |
| `disconnect` | Disconnect client |
| `advertise` | Restart advertising |
| `restart` | Reboot ESP32 |
| `power off` | Enter deep sleep |

### Debug flags

```
debug protocol   Frame parsing details
debug crypto     Encrypt/decrypt steps
debug crc        CRC check details
debug commands   Decoded command details
debug raw        Raw hex dumps
debug all        Enable all
debug none       Disable all
```

---

## Hardware wiring

Battery is fully simulated. Use the CLI to read or set it. For a bench
setup, two signals are worth wiring:

| Signal | Pin | Notes |
|--------|-----|-------|
| LED (connection status) | 32 | on = connected, blinking = advertising |
| Button (GND when pressed) | 33 | restarts advertising without the serial monitor |

Optional:

| Signal | Pin | Notes |
|--------|-----|-------|
| LED (speed activity) | 14 | blinks when REMOTE_SPEED commands arrive |
| Active buzzer | 22 | beeps on connect/disconnect |
| Passive buzzer | 23 | tone follows speed value |
| LED red | 25 | low battery threshold |
| LED yellow | 26 | mid battery threshold |
| LED green | 27 | full battery threshold |

All pins are set in `config.h`.

---

## Related

| Repo | Role |
|------|------|
| [MPZ-00/m25-remote](https://github.com/MPZ-00/m25-remote) | ESP32 BLE remote controller (sends commands to this emulator) |
| [MPZ-00/m5squared](https://github.com/MPZ-00/m5squared) | Python toolkit for protocol analysis, GUI, and code generators |
