# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An ESP32 firmware that emulates an Alber e-motion M25 wheelchair wheel over Bluetooth. It speaks the real M25 binary protocol (AES-128 encrypted, CRC-16, byte-stuffed frames) so that the official app and Python tooling (`m25_spp.py`) cannot tell it apart from a real wheel.

Target hardware: ESP32-WROOM-32. Transport: Bluetooth Classic SPP (RFCOMM channel 6) and/or BLE GATT, selectable at compile time.

## Build and flash

```sh
pio run -t upload          # build + flash
pio device monitor         # serial monitor at 115200 baud
pio run                    # build only
```

Change `upload_port` in `platformio.ini` to match your COM port. Do **not** change the `platform =` line — the pioarduino URL is required for Bluetooth Classic support.

## Encryption keys

Keys are injected from a local `.env` file next to `platformio.ini` by `load_env.py` (PlatformIO pre-build script):

```
M25_LEFT_KEY=A1B2C3D4...   # 32 hex chars
M25_RIGHT_KEY=C1D2E3F4...
```

If no `.env` exists, the zero-byte fallback keys in `config.h` are used.

## Runtime wheel-side selection

After flashing, set the side once via the serial CLI:

```
config set left    # or right — saves to ESP32 NVS and reboots
config reset       # clears NVS, falls back to compile-time default
```

The side is stored in ESP32 NVS under namespace `m25wheel`. It survives re-flash until explicitly cleared.

## Architecture

All logic is in header-only modules. `m25_wheel_rfcomm.ino` contains only `setup()` and `loop()` and wires them together.

### Data flow

```
Transport (RFCOMM / BLE)
  -> rfcomm_poll() / ble_poll()          transport_rfcomm.h / transport_ble.h
  -> do_process_frame()                   .ino
  -> packet_decode()                      packet.h
      -> proto_frame_parse()  (CRC, unstuff)   protocol.h
      -> crypto_ecb_decrypt() + crypto_cbc_decrypt() (recover IV, decrypt)  crypto.h
  -> command_apply()                      command.h
      -> mutates WheelState               state.h
  -> do_send_response_for()               .ino
  -> packet_encode_response()             packet.h
      -> _packet_encode_spp()  (pad, encrypt, frame)
  -> _transport_send()                    .ino  -> rfcomm_send() / ble_send()
```

### Module responsibilities

| Module | Responsibility |
|--------|---------------|
| `config.h` | All compile-time constants: pins, timeouts, feature flags, transport selection, BLE UUIDs |
| `nvs_config.h` | `WheelRuntimeConfig` struct; load/save wheel side from NVS; populates per-side keys, device name, UUIDs |
| `state.h` | `WheelState` struct; pure mutation helpers; no hardware access |
| `protocol.h` | CRC-16, byte stuffing/unstuffing, frame parse and build; no crypto, no state |
| `crypto.h` | AES-128 ECB/CBC wrappers around mbedtls |
| `packet.h` | Codec layer: calls protocol.h + crypto.h to go from wire bytes ↔ SPP payload |
| `command.h` | Maps SPP service/param IDs to `WheelState` mutations; returns `CmdResult` (IGNORE / SEND_ACK / SPEED_UPDATE) |
| `transport_rfcomm.h` | BluetoothSerial SPP; event detection, polling, send |
| `transport_ble.h` | BLE GATT server; RX queue, stale-packet tracking, event detection |
| `led.h` | LED state machine (battery level, connection, speed blink animation) |
| `buzzer.h` | Active and passive buzzer helpers |
| `cli.h` | Serial command-line; delegates actions to the `.ino` via `CliActions` callback struct |
| `safety.h` | Command-timeout watchdog, battery drain timer, button debounce + advertise trigger |

### Protocol constants

Defined in `command.h`. Key ones:

- `SPP_SERVICE_APP_MGMT` (0x01) — primary service for speed, assist, drive mode, profile
- `SPP_PARAM_REMOTE_SPEED` (0x30) — high-rate speed command; ACK is intentionally **skipped** to avoid flooding the link
- `SPP_SERVICE_BATT_MGMT` (0x08), `SPP_SERVICE_VERSION_MGMT` (0x0A) — read-only queries from remote during init; always ACK'd

### Transport selection

Controlled by `TRANSPORT_RFCOMM_ENABLED` / `TRANSPORT_BLE_ENABLED` in `config.h`. Both can be on simultaneously. All code guarded by `#if TRANSPORT_*_ENABLED`.

### BLE stale-packet handling

On BLE connect, the first packets can arrive before the app finishes its crypto handshake. `transport_ble.h` tracks a stale count; frames that fail `packet_decode()` are discarded for up to `STALE_TIMEOUT_MS` (15 s). After that, the device disconnects and forces a reconnect.
