# M460 BSP: HSUSBD SVF & Sensor Management Sample

## Overview
This project is a Board Support Package (BSP) sample for the Nuvoton **NuMicro M460** series microcontroller. It demonstrates a multi-functional application that combines High-Speed USB Device (HSUSBD) communication, JTAG/SVF programming, and a Baseboard Management Controller (BMC) style sensor monitoring system.

### Key Features
- **USB HID Interface:** High-speed data transfer between the MCU and a PC using the HID class.
- **SVF Player (JTAG):** Executes Serial Vector Format (SVF) commands for JTAG/CPLD programming over USB.
- **Sensor Management (BMC Functions):**
    - **Fan Management:** Supports multiple logical fan zones using the NCT7363Y fan controller.
    - **CPLD Monitoring:** Reads hardware status and version information via I2C.
    - **Temperature Sensing:** Monitors system temperature using an external I2C sensor.
    - **NVMe Management:** Reads basic management information from NVMe drives.
- **System Control:** Supports remote reset, boot-to-LDROM (for ISP), and firmware version reporting via USB commands.

## Project Structure
```text
m460bsp/
├── Library/                    # Nuvoton Standard Libraries (CMSIS, StdDriver)
└── SampleCode/StdDriver/HSUSBD_svf/
    ├── main.c                  # Application entry point & USB command parser
    ├── cpld_fan.c              # CPLD and Fan (NCT7363Y) management logic
    ├── descriptors.c           # USB HID descriptors
    ├── hid_transfer.c          # USB HID transfer handling
    ├── svf.c / xsvf.c          # SVF/XSVF player implementation
    ├── tap.c                   # JTAG TAP state machine
    ├── nvme_mi.c               # NVMe Basic Management Interface logic
    └── Keil/                   # Keil MDK Project files (.uvprojx)
```

## Hardware Requirements
- **MCU:** Nuvoton M460 series (e.g., M467).
- **Peripherals:**
    - **I2C0/I2C1:** Connected to CPLD, NCT7363Y Fan Controller, and Temperature sensors.
    - **USB:** High-speed USB port for PC communication.
    - **JTAG Pins:** PA7 (TDI), PA6 (TDO), PC1 (TCK), PC0 (TMS) — configured for fast slew rate.
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
The device communicates via 1024-byte HID report packets.

| Command | Name | Payload Details | Response Details |
| :--- | :--- | :--- | :--- |
| `0xa0` | **SVF Init** | None | Returns 1024 bytes of zeros (clears error state). |
| `0xa1` | **SVF Exec (Final)** | `[1...1023]`: Final or only SVF command packet. Appends payload to internal buffer, processes complete command when `;` terminator is found. | Returns 4-byte error line number (little-endian) or 0 if success. Total response is 1024 bytes. |
| `0xa2` | **SVF Exec (Continuation)** | `[1...1023]`: Continuation packet for multi-packet SVF commands. Payload is appended to internal buffer (max 8192 bytes) without processing. | No response sent. Host should not read between `0xa2` packets. |
| `0xb0` | **FW Version** | None | Returns 4-byte version: `0x26, 0x01, 0x14, 0x02`. |
| `0xb1` | **LDROM Boot** | `[1]=0x5a, [2]=0xa5` | No response (device reboots to LDROM). |
| `0xb2` | **APROM Reset**| `[1]=0x55, [2]=0xaa` | No response (device reboots to APROM). |
| `0xb2` | **Set Var** | `[1]=0x5a, [2]=Value` | Sets internal `reset_var`. |
| `0xb2` | **Get Var** | `[1]=0x6a` | Returns `[0]=0xb2, [1]=reset_var`. |
| `0xb3` | **Get LED Pins**| None | Returns `[0]=0xb3, [1]=AMB_Pin, [2]=GRN_Pin`. |
| `0xc0` | **Read BMC** | None | Returns full 1024-byte `bmc_report` array. |
| `0xc1` | **Write EEPROM** | `[1...256]`: 256 bytes of data to write | Writes 256 bytes to EEPROM starting at address 0x00. |
| `0xc2` | **Read EEPROM** | None | Returns `[0]=0xc2, [1...256]`: 256 bytes read from EEPROM. |
| `0xd0` | **I2C Write** | `[1]=Bus, [2]=Addr, [3]=Len, [4...]=Data` | Starts I2C write transaction. |
| `0xd1` | **I2C Write Ack**| None | Returns `[0]=0xd1, [1]=BytesWritten`. |
| `0xd2` | **I2C Read** | `[1]=Bus, [2]=Addr, [3]=Len` | Starts I2C read transaction. |
| `0xd3` | **I2C Read Ack** | None | Returns `[0]=0xd3, [1]=BytesRead, [2...]=Data`. |
| `0xd4` | **I2C W-then-R**| `[1]=Bus, [2]=Addr, [3]=WLen, [4]=RLen, [5...]=Data` | Returns `[0]=0xd4, [1]=WStat, [2]=RStat, [3...]=Data`. |
| `0xda` | **Monitor Ctrl**| `[1]=1 (On) / 0 (Off)` | Enables/Disables 500ms periodic sensor task. |
| `0xdb` | **Monitor Stat**| None | Returns `[0]=0xdb, [1]=Status`. |

## BMC Report Memory Map (1024 bytes)
The `bmc_report` array (accessed via USB command `0xc0`) contains the current state of all monitored hardware.

| Offset | Length | Name | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | 1 | `cpld_ver` | CPLD Hardware Version |
| `0x02` | 1 | `cpld_test_ver`| CPLD Test/Sub-version |
| `0x10` | 1 | `temp_high` | Temperature sensor high byte |
| `0x11` | 1 | `temp_low` | Temperature sensor low byte |
| `0x20` | 4 | `jtag_id` | Scanned JTAG ID of the CPLD |
| `0x30` | 1 | `hdd_amount` | Number of detected HDDs |
| `0x40` | 8 | `hdd_port_stat`| Port status for HDD 0-7 |
| `0x50` | 8 | `hdd_status` | General status for HDD 0-7 |
| `0x60` | 8 | `hdd_led` | LED status for HDD 0-7 |
| `0x70` | 20 | `fan_data` | Data for 5 fans (4 bytes each: Duty, RPM_H, RPM_L, Rsvd) |
| `0x100`| 256 | `nvme_info` | NVMe info for up to 8 slots (32 bytes per slot) |

## License
Copyright (C) 2021 Nuvoton Technology Corp. All rights reserved. Licensed under the Apache-2.0 license.
