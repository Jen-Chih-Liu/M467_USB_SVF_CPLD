# M460 BSP: HSUSBD SVF & Sensor Management Sample

## Overview
This project is a Board Support Package (BSP) sample for the Nuvoton **NuMicro M460** series microcontroller. It demonstrates a multi-functional application that combines High-Speed USB Device (HSUSBD) communication, JTAG/SVF programming, and a Baseboard Management Controller (BMC) style sensor monitoring system.

### Key Features
- **USB HID Interface:** High-speed data transfer between the MCU and a PC using the HID class.
- **SVF Player (JTAG):** Executes Serial Vector Format (SVF) commands for JTAG/CPLD programming over USB.
- **Sensor Management (BMC Functions):**
    - **Fan Management:** Supports up to 16 logical fan channels across two NCT7363Y fan controllers.
    - **CPLD Monitoring:** Reads hardware status and version information from up to two CPLDs via I2C (I2C0 / I2C2).
    - **Temperature Sensing:** Monitors system temperature using an external I2C sensor.
    - **NVMe Management:** Reads basic management information (temperature) from NVMe drives via SMBus. The first CPLD's drives are polled on I2C1; the second CPLD's drives (when present) on UI2C0.
    - **NVMe-MI (Management Interface):** Executes on-demand NVMe-MI commands over MCTP-over-SMBus (USB command `0xe0`) — Basic Management, VPD Read/Write, Configuration Get/Set, Health/SMART logs, Device Identify, Format NVM, etc. Drive slots span both CPLD banks as one flat range and are routed to I2C1 or UI2C0 automatically.
- **System Control:** Supports remote reset, boot-to-LDROM (for ISP), LED status reporting, and firmware version reporting via USB commands.

## Project Structure
```text
m460bsp/
├── Library/                    # Nuvoton Standard Libraries (CMSIS, StdDriver)
└── SampleCode/StdDriver/HSUSBD_svf/
    ├── main.c                  # Application entry point & USB command parser
    ├── g_def.h                 # Global definitions, offsets, and device addresses
    ├── cpld_fan.c              # CPLD and Fan (NCT7363Y) management logic
    ├── descriptors.c           # USB HID descriptors
    ├── hid_transfer.c / .h     # USB HID transfer handling
    ├── svf.c / xsvf.c          # SVF/XSVF player implementation
    ├── xsvftool-esp.c          # SVF packet tool (ESP/USB path)
    ├── play.c / scan.c         # libxsvf support: SVF playback & scan
    ├── tap.c                   # JTAG TAP state machine
    ├── statename.c / memname.c # libxsvf state/memory name helpers
    ├── nvme_mi.c               # NVMe Basic Management Interface logic
    ├── log_dump.c              # Debug log helper
    ├── usb_sn.c                # USB serial number helper
    └── Keil/                   # Keil MDK Project files (.uvprojx)
```

## Hardware Requirements
- **MCU:** Nuvoton M460 series (e.g., M467).
- **Peripherals:**
    - **I2C0:** CPLD1, NCT7363Y fan controllers (0x40 / 0x42 / 0x44 / 0x46), temperature sensor (0x30), EEPROM (0xAE).
    - **I2C1:** I²C MUX (TCA9548 / PCA9848) for NVMe SMBus access.
    - **I2C2:** CPLD2 (PA10=SDA, PA11=SCL), EEPROM2 — used when a second CPLD is detected.
    - **UI2C0 (USCI0):** NVMe MI interface (PB13=DAT0, PB12=CLK).
    - **USB:** High-speed USB port for PC communication.
    - **JTAG Pins:** PA7 (TCK), PA6 (TMS), PC1 (TDI), PC0 (TDO) — configured for fast slew rate and Schmitt trigger input.
    - **LED Pins:** PB3 (Amber), PB2 (Green).
    - **UART3:** Debug console (PB14 RX, PB15 TX) at 115200 bps.

## Getting Started

### 1. Build the Project
1. Open the Keil project located at `SampleCode\StdDriver\HSUSBD_svf\Keil\HSUSBD_HID_Transfer.uvprojx`.
2. Ensure you have the Nuvoton NuMicro M460 Series Device Family Pack (DFP) installed.
3. Click **Build** (F7) to compile the project.

### 2. Programming
1. Connect your debugger (e.g., Nu-Link).
2. Click **Download** (F8) to flash the firmware to the M460 target.

### 3. Usage
- **USB Interface:** Use a PC-side HID tool to send commands (0xa0, 0xa1, 0xc0, etc.) to the device.
- **Debug Console:** Connect a serial terminal to UART3 to see system logs and sensor data.
- **SVF Programming:** Send SVF packets via USB command `0xa1` to program connected JTAG devices.

## USB Command Summary
The device communicates via 1024-byte HID report packets. `[N]` refers to byte index N in the received buffer; `usb_rcvbuf[0]` is always the command byte.
> **I2C address note:** For `0xd0` / `0xd2` / `0xd4`, `[2]` carries the 8-bit I²C address (e.g., 0xAE); the firmware shifts it right by 1 internally before use.

| Command | Name | Payload Details | Response Details |
| :--- | :--- | :--- | :--- |
| `0xa0` | **SVF Init** | None | Clears SVF state (`buffer_index`, `total_line`, error flags). Returns 1024 bytes of zeros. |
| `0xa1` | **SVF Exec (Final)** | `[1...1023]`: Final or only SVF command packet. Payload is appended to the internal 8192-byte buffer; command is executed when a `;` terminator is found. | Returns 4-byte error line number (little-endian) or 0 if success. Total response is 1024 bytes. |
| `0xa2` | **SVF Exec (Continuation)** | `[1...1023]`: Continuation packet for long SVF commands. Payload is appended to internal buffer without processing. | No response sent. |
| `0xb0` | **FW Version** | None | Returns 4-byte version: `0x26, 0x04, 0x20, 0x02`. |
| `0xb1` | **LDROM Boot** | `[1]=0x5A, [2]=0xA5` | No response (device reboots to LDROM / ISP mode). |
| `0xb2` | **APROM Reset** | `[1]=0x55, [2]=0xAA` | No response (device performs system reset). |
| `0xb2` | **Set Var** | `[1]=0x5A, [2]=Value` | Stores `Value` into internal `reset_var`. |
| `0xb2` | **Get Var** | `[1]=0x6A` | Returns `[0]=0xb2, [1]=reset_var`. |
| `0xb3` | **Get LED Pins** | None | Returns `[0]=0xb3, [1]=GLED_AMB_N_R (PB3), [2]=GLED_GRN_N_R (PB2)`. |
| `0xc0` | **Read BMC (CPLD1)** | None | Returns full 1024-byte `bmc_report` array (CPLD1 / I2C0). |
| `0xc1` | **Write EEPROM** | `[1...256]`: 256 bytes of data | Writes 256 bytes to EEPROM (I2C0) starting at offset 0x00. |
| `0xc2` | **Read EEPROM** | None | Returns `[0]=0xc2, [1...256]`: 256 bytes read from EEPROM (I2C0). |
| `0xc4` | **Read BMC (CPLD2)** | None | Returns full 1024-byte `bmc_report1` array (CPLD2 / I2C2). |
| `0xc5` | **Write EEPROM2** | `[1...256]`: 256 bytes of data | Writes 256 bytes to EEPROM2 (I2C2 / CPLD2 path). |
| `0xc6` | **Read EEPROM2** | None | Returns `[0]=0xc6, [1...256]`: 256 bytes read from EEPROM2 (I2C2). |
| `0xd0` | **I2C Write** | `[1]=Bus (0=I2C0, 1=I2C1), [2]=Addr (8-bit), [3]=Len, [4+]=Data` | Starts I²C write transaction. |
| `0xd1` | **I2C Write Ack** | None | Returns `[0]=0xd1, [1]=BytesWritten`. |
| `0xd2` | **I2C Read** | `[1]=Bus (0=I2C0, 1=I2C1), [2]=Addr (8-bit), [3]=Len` | Starts I²C read transaction. |
| `0xd3` | **I2C Read Ack** | None | Returns `[0]=0xd3, [1]=BytesRead, [2+]=Data`. |
| `0xd4` | **I2C Write-then-Read** | `[1]=Bus, [2]=Addr (8-bit), [3]=WLen, [4]=RLen, [5+]=WriteData` | Returns `[0]=0xd4, [1]=WriteStat, [2]=ReadStat, [3+]=ReadData`. |
| `0xda` | **Monitor Ctrl** | `[1]=1` (enable) or `0` (disable) | Enables/disables the 500 ms periodic sensor polling task. |
| `0xdb` | **Monitor Stat** | None | Returns `[0]=0xdb, [1]=i2c_monitor_flag`. |
| `0xdc` | **GPIO Write** | `[1]=40`: set PC14=`[2]`; `[1]=48`: set PB6=`[2]` | PC14=0 also triggers `CPLD_read()`/`CPLD_read1()`; PB6 controls the PCA9848 SMBus reset line. |
| `0xdd` | **GPIO Read** | `[1]=40`: read PC14; `[1]=48`: read PB6 | Returns `[0]=0xdd, [1]=PinValue`. |
| `0xe0` | **NVMe-MI Command** | `[1]=NVMe index (0-based)`, `[2]=CommandID (1..19)`, `[3..6]=Dword0 (LE)`, `[7..10]=Dword1 (LE)`. VPD Write (CmdID 8) carries data bytes from `[7]` onward. | Returns `[0]=0xe0, [1]=CommandID, [2]=Status (0=OK, 1=Fail), [3]=Data byte count`, `[4..]=raw data` (success) or null-terminated ASCII reason (fail). See below. |

## NVMe-MI Command (`0xe0`)
Executes a single NVMe-MI Management Interface command against one drive over MCTP-over-SMBus. The host tool's `NVMEMI <CommandID> <NVMEIndex>` command maps to this; the host converts its 1-based `NVMEIndex` to the 0-based `[1]` index used here.

### CommandID map
| ID | Command | ID | Command |
| :-- | :--- | :-- | :--- |
| 1 | Basic Management | 11 | Ctrl Health Poll |
| 2 | Get Endpoint UUID | 12 | Health Info + Alerts |
| 3 | Get Msg Type Support | 13 | Power Management |
| 4 | Read MI Data Struct | 14 | FW Revision |
| 5 | Configuration Set | 15 | Health Alerts |
| 6 | Configuration Get | 16 | Health Info |
| 7 | VPD Read | 17 | SMART Log |
| 8 | VPD Write | 18 | Device Identify |
| 9 | Reset (unsupported) | 19 | Format NVM (destructive) |
| 10 | Health Status Poll | | |

- **CommandID 1 (Basic Management)** returns the 32-byte Basic Management structure cached in `bmc_report[]` (no live bus transaction).
- **CommandID 2..19** are live MCTP/NVMe-MI over-SMBus exchanges (`nvme_mi.c`): the firmware routes the mux, briefly reconfigures the bus as an MCTP slave (address 0x10), transmits the request, receives and reassembles the response (verifying PEC + MIC), then restores the bus for normal master use.

### Dual-CPLD flat slot indexing
The `[1]` NVMe index is a **flat, 0-based** number across both CPLD banks:
- Slots `0 .. (bmc_report[0x30]-1)` → first CPLD's drives on **I2C1**.
- Slots `bmc_report[0x30] ..` → second CPLD's drives on **UI2C0** (only when a second CPLD is detected, i.e. `g_u32clpd_lost == 0`; otherwise the command fails).

Within each bank, `localSlot / 8` selects the PCA9848/TCA9548 mux and `localSlot % 8` the channel. `SelectNvmeSlotChannel()` performs this split and selects the bus for the following MCTP exchange. Each bank has **3 muxes × 8 channels = up to 24 drives** (`s_au8_PCA9848_Addr[]` = 0xB2/0xB4/0xB6), so a single CPLD supports up to 24 drives.

## BMC Report Memory Map (1024 bytes)
The `bmc_report` array (CPLD1, accessed via `0xc0`) and `bmc_report1` (CPLD2, accessed via `0xc4`) share the same layout.

| Offset | Length | Name | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | 1 | `cpld_ver` | CPLD Hardware Version |
| `0x02` | 1 | `cpld_test_ver` | CPLD Test/Sub-version |
| `0x10` | 1 | `temp_high` | Temperature sensor high byte |
| `0x11` | 1 | `temp_low` | Temperature sensor low byte |
| `0x20` | 4 | `cpld_jtag_id` | Scanned JTAG ID of the CPLD (4 bytes) |
| `0x30` | 1 | `hdd_amount` | Number of detected HDDs (max 24 per CPLD) |
| `0x40` | 8 | `hdd_port_stat` | Port status, **2 bits per slot** (`0b11` = NVMe present), 4 slots per byte → covers up to 24 slots |
| `0x50` | 8 | `hdd_status` | General status for HDD slots 0–7 (1 byte per slot; layout limited to 8) |
| `0x60` | 8 | `hdd_led` | LED status for HDD slots 0–7 (1 byte per slot; layout limited to 8) |
| `0x70` | 64 | `fan_data` | Data for 16 fans × 4 bytes each: `[Duty, RPM_H, RPM_L, Reserved]` |
| `0x100` | 768 | `nvme_info` | NVMe info for up to 24 slots (32 bytes per slot, `nvme_slot_0..23`) |

### Fan Data Detail (offset `0x70`)
Each fan occupies 4 bytes. RPM is computed as: $\text{RPM} = \frac{1350000}{\text{count}_{13\text{-bit}}}$, where the 13-bit counter is reconstructed from `RPM_H` and `RPM_L` bytes read from the NCT7363Y.

### NCT7363Y Fan Controller Map
Four NCT7363Y ICs (addresses `0x40`, `0x42`, `0x44`, `0x46`) manage up to 32 logical fan channels (4 PWM outputs × 8 TACH inputs per IC). Each IC is independently detected at startup (`fan_address_0x4?_d`); the MCU mirrors their register maps and also presents them as I²C4 slaves so the BMC can read fan data. On the reference board, `0x40`/`0x42` sit on CBP20OB and `0x44`/`0x46` on CFB26.

| Fans | IC I²C Addr | PWM Channels | TACH Channels |
| :--- | :--- | :--- | :--- |
| FAN 1–4 | `0x40` | PWM 0–3 | TACH 0–3 |
| FAN 5–8 | `0x40` | PWM 0–3 (shared) | TACH 4–7 |
| FAN 9–12 | `0x42` | PWM 0–3 | TACH 0–3 |
| FAN 13–16 | `0x42` | PWM 0–3 (shared) | TACH 4–7 |
| FAN 17–20 | `0x44` | PWM 0–3 | TACH 0–3 |
| FAN 21–24 | `0x44` | PWM 0–3 (shared) | TACH 4–7 |
| FAN 25–28 | `0x46` | PWM 0–3 | TACH 0–3 |
| FAN 29–32 | `0x46` | PWM 0–3 (shared) | TACH 4–7 |

## License
Copyright (C) 2021 Nuvoton Technology Corp. All rights reserved. Licensed under the Apache-2.0 license.
