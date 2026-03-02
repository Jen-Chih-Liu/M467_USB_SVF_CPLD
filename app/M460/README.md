# M460 USB Management Tool

A cross-platform command-line utility for managing Nuvoton MCU-based systems, providing firmware updates (ISP/SVF), hardware monitoring (Fans, Sensors, HDD), and low-level I2C/JTAG communication via USB HID.

## 🚀 Key Features

- **Firmware Management**:
  - **MCU ISP**: Update MCU firmware using `.bin` files via In-System Programming.
  - **CPLD SVF**: Program CPLD firmware using `.svf` files over JTAG-over-USB.
- **Hardware Monitoring**:
  - **HDD/NVMe**: Detect presence, read temperature, and manage LED status (Standby, Fault, Locate, Rebuild).
  - **Fan Control**: Monitor Fan RPM and set/get PWM Duty cycles.
  - **Thermal**: Read BPB (Backplane Board) temperature sensors.
- **System Control**:
  - Perform system resets and manage reset control variables.
  - Monitor Global LED status.
- **Low-Level Debugging**:
  - **I2C Bypass**: Directly communicate with I2C devices behind the bridge.
  - **Register Dump**: Comprehensive dump of all hardware registers for debugging.
  - **USB Listing**: Deterministic logical device mapping based on physical topology (Bus/Port).

## 🛠 Prerequisites

### Hardware
- Nuvoton M463 based system or compatible target device.

### Software
- **CMake** (3.8 or higher)
- **C Compiler** (MSVC for Windows, GCC/Clang for Linux)
- **libusb-1.0**:
  - **Windows**: Included in the `libusb` folder (MSVC binaries).
  - **Linux**: Install via package manager (e.g., `sudo apt install libusb-1.0-0-dev`).

## 🔨 Build Instructions

### Windows (Visual Studio)
1. Open the project folder in Visual Studio (using CMake support).
2. The `CMakeLists.txt` is configured to automatically link the included libusb libraries and copy the DLL to the output directory.
3. Build the project (Ctrl+Shift+B).

### Linux
```bash
mkdir build && cd build
cmake ..
make
```

## 💻 Usage

The tool uses a logical device index (`<Count>`) to target specific boards in a multi-device environment.

**General Syntax:**
```bash
M460.exe <Count> <Command> <SubCommand> [Arguments...]
```

### Common Commands

| Category | Command Examples | Description |
| :--- | :--- | :--- |
| **Discovery** | `usblist` | List all connected devices with Logical IDs. |
| **MCU Update**| `Update M463 firmware.bin` | Start MCU ISP programming. |
| **CPLD Update**| `Update CPLD logic.svf` | Start CPLD SVF programming. |
| **HDD Info** | `HDD Info` / `HDD Temp` | Show drive details or temperature in JSON. |
| **Fan Control**| `FAN Duty Set 50` | Set all fans to 50% duty cycle. |
| **Thermal** | `BPB Sensor Get` | Read system ambient temperature. |
| **I2C Bypass** | `I2CBYPASS Get 0x46 0x90 1` | Read 1 byte from Addr 0x46, Reg 0x90. |
| **Debug** | `dumpall` | Output full system status for troubleshooting. |

### Output Format
Most commands output data in **JSON format**, making it easy to integrate with higher-level scripts or monitoring UI.

Example `HDD Temp` output:
```json
{
  "NVMECount": 6,
  "NVME1": "42",
  "NVME2": "NA",
  ...
}
```

## 📁 Project Structure

- `M460.c`: Command dispatcher and CLI logic.
- `usb_cmd.c`: USB backend, device mapping, and HID communication.
- `mcu_isp.c`: MCU ISP protocol implementation.
- `cpld_svf.c`: SVF parser and JTAG programming engine.
- `config.h`: Hardware register maps and global constants.
- `cJSON.c/h`: Lightweight JSON library.

## 📝 License
This project is for internal technical management and hardware validation.
