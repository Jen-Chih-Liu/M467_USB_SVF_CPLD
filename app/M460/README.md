# M460 USB Management Tool

**Version:** 20260420

A cross-platform command-line utility for managing Nuvoton M460/M463 MCU-based systems via USB HID. Provides firmware updates (ISP/SVF), hardware monitoring (Fans, Sensors, NVMe/HDD), I2C device communication, and JTAG debugging capabilities.

**Target Device:**
- VID: `0x0416`, PID: `0x502a`
- Supports up to 16 concurrent devices with deterministic logical device mapping

## 🚀 Key Features

### Firmware Management
- **MCU ISP (In-System Programming)**
  - Update M460/M463 MCU firmware using `.bin` files
  - Automatic device handshaking and checksum verification
  - Bootloader (LDROM) switching for safe updates
  
- **CPLD SVF Programming**
  - Program CPLD (Complex Programmable Logic Device) using `.svf` files over JTAG-over-USB
  - Real-time status checking and error reporting

### Hardware Monitoring
- **NVMe/SSD Storage**
  - Detect up to 16 NVMe drives with presence status
  - Read drive temperature sensors
  - Manage LED indicators (Standby, Fault, Locate, Rebuild states)
  - Query drive information via passthrough I2C commands
  
- **Fan Control & Monitoring**
  - Monitor individual fan RPM (rotations per minute)
  - Set/get PWM duty cycle (0-100%)
  - Convert duty percentage to 8-bit register values
  
- **Thermal Sensors**
  - Read BPB (Backplane Board) ambient temperature
  - 0.0625°C resolution temperature reporting
  - Temperature offset mapping via CPLD registers

### System Control & Debugging
- **System Reset Management**
  - Perform software resets
  - Get/set reset control variables
  - MCU bootloader jumping (LDROM/APROM switching)
  
- **Global Status Monitoring**
  - Query global LED status
  - Full register dump for system debugging
  - CPLD version, test version, and JTAG ID reading
  
- **I2C Device Communication**
  - Direct I2C passthrough to backend devices
  - CPLD I2C register read/write operations
  - Flexible register address and data length specification
  
- **USB Device Discovery**
  - Automatic scan with deterministic logical device numbering
  - Sort by physical topology (Bus → Port Path)
  - JSON-formatted device listing

## 🛠 Prerequisites

### Hardware
- Nuvoton M460/M463 MCU-based system (USB HID interface at VID `0x0416`/PID `0x502a`)
- USB connection to host computer

### Software
- **CMake** (3.8 or higher) — For cross-platform build configuration
- **C Compiler**:
  - **Windows**: Microsoft Visual Studio 2015 or later (MSVC)
  - **Linux**: GCC 4.8+ or Clang 3.3+
- **libusb-1.0**:
  - **Windows**: Pre-compiled binaries included in `libusb/` folder (MSVC 64-bit)
  - **Linux**: `sudo apt install libusb-1.0-0-dev` (Debian/Ubuntu)

## 🔨 Build Instructions

### Windows (Visual Studio with CMake)

1. **Prerequisites**:
   - Visual Studio 2015 or later with CMake support
   - CMake 3.8+
   - libusb pre-built binaries in `libusb/VS2019/MS64/dll/`

2. **Build Steps**:
   ```powershell
   # Navigate to project root
   cd app/M460
   
   # Open in Visual Studio with CMake support
   # File → Open → Folder (Select M460 folder)
   ```
   
   Or use command line:
   ```powershell
   mkdir build
   cd build
   cmake .. -G "Visual Studio 16 2019" -A x64
   cmake --build . --config Release
   ```

3. **Output**:
   - Executable: `build/Release/M460.exe`
   - DLL (auto-copied): `libusb-1.0.dll` in same directory

4. **CMake Configuration Details**:
   - libusb lib path: `libusb/VS2019/MS64/dll/libusb-1.0.lib`
   - libusb DLL auto-copied to executable directory post-build
   - Include paths: `libusb/include/`

### Linux (GCC/Clang)

```bash
# Install dependencies
sudo apt update
sudo apt install cmake build-essential libusb-1.0-0-dev

# Build
cd app/M460
mkdir build && cd build
cmake ..
cmake --build .

# Output
./M460  # Executable in build/ directory
```

### macOS

```bash
# Install dependencies via Homebrew
brew install cmake libusb

# Build
cd app/M460
mkdir build && cd build
cmake .. -DCMAKE_C_COMPILER=clang
cmake --build .

./M460  # Executable
```

### Development Build (Debug Mode)

```bash
# Windows
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Debug

# Linux/macOS
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

### Clean Build Files (Preserve Executable)

**Windows (PowerShell)**:
```powershell
cd build
Remove-Item -Recurse -Force CMakeFiles,M460.dir
Remove-Item *.vcxproj,*.filters,CMakeCache.txt,cmake_install.cmake
# Executable M460.exe remains in Release/ or Debug/ folder
```

**Linux/macOS**:
```bash
cd build
rm -rf CMakeFiles *.cmake Makefile
find . -name "*.o" -delete
# Executable M460 remains in build/ directory
```

### Full Clean (Remove All Build Artifacts)

```bash
# Windows
rmdir /s /q build

# Linux/macOS
rm -rf build
```

## 💻 Usage

### Command Syntax

```bash
M460.exe <Count> <Command> <SubCommand> [Arguments...]
```

**Parameters:**
- `<Count>`: Logical device index (0-based, up to 15 for 16 devices). Assigned by physical USB topology (Bus/Port).
- `<Command>`: Top-level command category (e.g., `CPLD`, `HDD`, `FAN`, `UPDATE`)
- `<SubCommand>`: Operation within the command category (e.g., `Get`, `Set`, `Info`)
- `[Arguments...]`: Additional parameters specific to the subcommand

**Example:**
```bash
M460.exe 0 HDD Info              # Query HDD info on device 0
M460.exe 1 FAN Duty Set 75       # Set Fan duty to 75% on device 1
M460.exe 2 UPDATE MCU firmware.bin  # Program MCU on device 2
```

### Discovery & Device Management

#### List All Connected Devices

```bash
M460.exe 0 usblist
```

**Output Example:**
```
Device Count: 2
[Device 0] Bus=1, Port=1.2, VID=0x0416, PID=0x502a, Serial=12345678
[Device 1] Bus=2, Port=2.1, VID=0x0416, PID=0x502a, Serial=87654321
```

### Firmware Programming

#### Update MCU Firmware (ISP)

```bash
M460.exe <Count> UPDATE MCU <firmware.bin>
```

**Process:**
1. Reads MCU APROM capacity
2. Jumps to bootloader (LDROM)
3. Programs binary in packets
4. Verifies checksum
5. Returns to APROM

**Example:**
```bash
M460.exe 0 UPDATE MCU mcu_firmware.bin
```

#### Program CPLD Firmware (SVF)

```bash
M460.exe <Count> UPDATE CPLD <logic.svf>
```

**Process:**
1. Parses SVF file
2. Executes JTAG commands over USB
3. Programs CPLD configuration
4. Performs verification

**Example:**
```bash
M460.exe 0 UPDATE CPLD cpld_design.svf
```

### CPLD Operations

#### Read CPLD Information

```bash
M460.exe <Count> CPLD Id          # Read JTAG chip ID
M460.exe <Count> CPLD Version     # Read CPLD firmware version
```

#### CPLD I2C Register Operations

```bash
# Read register
M460.exe <Count> CPLD Get <reg_addr> <byte_count>

# Write register
M460.exe <Count> CPLD Set <reg_addr> <value>
```

**Examples:**
```bash
M460.exe 0 CPLD Get 0x30 1        # Read HDD count from CPLD
M460.exe 0 CPLD Set 0x70 0x50     # Set fan duty control register
```

### Hardware Monitoring

#### NVMe/HDD Operations

```bash
# Query drive information
M460.exe <Count> HDD Info

# Get all drive temperatures
M460.exe <Count> HDD Temp

# Get LED status for specific drive
M460.exe <Count> HDD LED Get <slot_num>

# Set LED status (LED state: 0=Standby, 1=Fault, 2=Locate, 3=Rebuild)
M460.exe <Count> HDD LED Set <slot_num> <state>
```

**Example Output (HDD Info):**
```json
{
  "NVMECount": 6,
  "NVME1": {"Present": true, "Model": "Samsung 980 Pro", "Capacity": "2TB"},
  "NVME2": {"Present": true, "Model": "SK Hynix P41", "Capacity": "1TB"},
  "NVME3": {"Present": false},
  ...
}
```

**Example Output (HDD Temp):**
```json
{
  "NVMECount": 6,
  "NVME1": "42",
  "NVME2": "38",
  "NVME3": "NA",
  ...
}
```

#### Fan Control

```bash
# Get all fan RPM values
M460.exe <Count> FAN Get

# Set all fans to specific duty cycle (0-100%)
M460.exe <Count> FAN Duty Set <duty_percentage>

# Get current duty cycle
M460.exe <Count> FAN Duty Get
```

**Examples:**
```bash
M460.exe 0 FAN Get              # Read RPM for all fans
M460.exe 0 FAN Duty Set 60      # Set all fans to 60% speed
M460.exe 0 FAN Duty Get         # Query current duty cycle
```

**Example Output:**
```json
{
  "FanCount": 4,
  "Fan1_RPM": 3500,
  "Fan2_RPM": 3450,
  "Fan3_RPM": 0,
  "Fan4_RPM": 3480,
  "DutyCycle": "60%"
}
```

#### Thermal Sensors

```bash
# Read backplane board temperature
M460.exe <Count> BPB Sensor Get
```

**Example Output:**
```json
{
  "Temperature_C": "28.5625",
  "Temperature_F": "83.4"
}
```

### System Control

#### MCU Operations

```bash
# Get MCU firmware version
M460.exe <Count> MCU Version

# Perform system reset
M460.exe <Count> MCU Reset

# Jump MCU to bootloader (LDROM)
M460.exe <Count> MCU LDROM
```

#### Reset Variable Management

```bash
# Get reset control variable
M460.exe <Count> Reset Get

# Set reset control variable
M460.exe <Count> Reset Set <value>
```

### I2C Device Communication

#### CPLD I2C Passthrough

```bash
# Read from I2C device
M460.exe <Count> I2CBYPASS Get <i2c_addr> <register> <byte_count>

# Write to I2C device
M460.exe <Count> I2CBYPASS Set <i2c_addr> <register> <value>
```

**Examples:**
```bash
M460.exe 0 I2CBYPASS Get 0x46 0x90 1     # Read 1 byte from NCT7363 at addr 0x46, reg 0x90
M460.exe 0 I2CBYPASS Set 0x46 0x90 0x80  # Write 0x80 to same device
```

### System Debugging

#### Global LED Status

```bash
M460.exe <Count> GLOBAL Get
```

**Output:** LED state bitmask in JSON format

#### Complete System Dump

```bash
M460.exe <Count> dumpall
```

**Output:** Comprehensive JSON dump of:
- CPLD version and JTAG ID
- All hardware register values
- NVMe drive information
- Fan RPM and duty cycle
- Thermal sensor readings
- Global LED status

**Example:**
```json
{
  "Device": 0,
  "CPLD_Version": "1.2",
  "CPLD_JTAG_ID": "0x20b20953",
  "NVMECount": 6,
  "FanCount": 4,
  "Registers": {...},
  "Sensors": {...}
}
```

### EEPROM Management

```bash
# Write binary data to MCU EEPROM (max 256 bytes)
M460.exe <Count> EEPROM WRITE <eeprom.bin>

# Read EEPROM contents
M460.exe <Count> EEPROM READ
```

## 📋 Common Command Reference

| Feature | Command | Description |
| :--- | :--- | :--- |
| **Discovery** | `usblist` | List all connected M460 devices |
| **MCU Update** | `UPDATE MCU firmware.bin` | Program MCU APROM via ISP |
| **CPLD Update** | `UPDATE CPLD logic.svf` | Program CPLD via JTAG/SVF |
| **Drive Info** | `HDD Info` | List all NVMe drives and info |
| **Drive Temp** | `HDD Temp` | Read all NVMe temperatures |
| **Fan Control** | `FAN Duty Set 50` | Set all fans to 50% speed |
| **Fan Monitor** | `FAN Get` | Read all fan RPM values |
| **Backplane Temp** | `BPB Sensor Get` | Read ambient temperature |
| **System Status** | `dumpall` | Full system debug dump |
| **I2C Bypass** | `I2CBYPASS Get 0x46 0x90 1` | Read I2C device register |
| **MCU Version** | `MCU Version` | Get MCU firmware version |
| **MCU Reset** | `MCU Reset` | Perform soft reset |
| **CPLD Version** | `CPLD Version` | Get CPLD version |
| **Global LED** | `GLOBAL Get` | Query LED status |

## 🔧 Troubleshooting

### Device Not Found
**Problem**: `libusb error: no such device`

**Solutions**:
1. Verify USB cable connection
2. Check device is powered on
3. Confirm VID/PID in `config.h` matches target device
4. Install correct USB drivers (Windows may need libusb-win32 driver)
5. Run as administrator (Windows)

### libusb-1.0.dll Not Found (Windows)
**Problem**: Runtime error when launching M460.exe

**Solution**:
- Ensure `libusb-1.0.dll` is in the same directory as `M460.exe`
- CMake's post-build step should auto-copy it; if not, manually copy from:
  ```
  libusb/VS2019/MS64/dll/libusb-1.0.dll
  ```

### Firmware Update Fails
**Problem**: ISP or SVF programming returns error

**Troubleshooting**:
1. Verify file exists and is readable: `M460.exe <Count> dumpall`
2. Check MCU is in bootloader mode: `M460.exe <Count> MCU Version`
3. Try resetting device: `M460.exe <Count> MCU Reset`
4. Ensure file format matches:
   - ISP: `.bin` binary file (raw APROM image)
   - CPLD: `.svf` JTAG vector file (IEEE 1149.1 format)

### Multi-Device Issues
**Problem**: Incorrect device assigned to Count index

**Solution**:
```bash
M460.exe 0 usblist  # Always run this first to see device mapping
```

Devices are sorted by:
1. USB Bus number (ascending)
2. Port path (ascending, hierarchical)

Physical USB topology determines the logical ordering.

### Permission Denied (Linux)
**Problem**: libusb error on Linux

**Solution**:
```bash
# Option 1: Run with sudo
sudo ./M460 0 dumpall

# Option 2: Add udev rules (permanent)
sudo nano /etc/udev/rules.d/99-m460.rules
# Add: SUBSYSTEMS=="usb", ATTRS{idVendor}=="0416", ATTRS{idProduct}=="502a", MODE="0666"
sudo udevadm control --reload-rules
```

## 📚 Project Structure

```
app/M460/
├── M460.c              # Main CLI entry point and command dispatch
├── config.h            # Configuration defines (VID/PID, register maps, version)
├── usb_cmd.c           # USB enumeration, device scanning, HID communication
├── mcu_isp.c           # MCU ISP (In-System Programming) implementation
├── cpld_svf.c          # CPLD SVF parser and JTAG programmer
├── cJSON.c/h           # JSON library for output formatting
├── CMakeLists.txt      # CMake build configuration (cross-platform)
├── CMakePresets.json   # CMake preset for IDE integration
├── README.md           # This file
└── libusb/             # Pre-built libusb binaries for Windows
    ├── include/
    │   └── libusb.h
    └── VS2019/MS64/dll/
        ├── libusb-1.0.lib    # Link library
        └── libusb-1.0.dll    # Runtime library
```

### Key Source Files

#### **M460.c** (Main Application)
- Command-line argument parsing
- Command dispatch routing
- Output formatting and JSON generation
- Device selection and error handling

#### **config.h** (Configuration)
- USB vendor/product IDs
- CPLD register address maps
- MCU memory configuration
- Device and feature limits
- Build timestamps

#### **usb_cmd.c** (USB Communication)
- libusb device enumeration
- Deterministic device sorting (Bus/Port topology)
- USB HID read/write operations
- Device map management

#### **mcu_isp.c** (Firmware Programming)
- Binary file loading
- APROM programming protocol
- Bootloader switching
- Checksum calculation and verification
- USB packet sequencing

#### **cpld_svf.c** (CPLD Programming)
- SVF file parsing
- JTAG command execution
- State machine transitions (Shift-IR, Shift-DR)
- Device status checking and error handling

## 🔌 Hardware Integration

### USB HID Interface
- **Class**: Human Interface Device (HID)
- **Subclass**: Custom
- **Protocol**: Vendor-specific
- **Endpoint**: Bulk transfer (IN/OUT)
- **Packet Size**: 1024 bytes (Windows), 1024 bytes (Linux)
- **Timeout**: 1000 ms default

### CPLD Register Interface
Accessed via USB HID, memory-mapped at addresses `0x00-0x2FF`:
- **0x00-0x1F**: Board info (version, JTAG ID, sensor data)
- **0x30**: HDD count
- **0x40-0x4F**: HDD port status
- **0x50-0x5F**: HDD state registers
- **0x60-0x6F**: HDD LED control
- **0x70**: Fan PWM duty cycle
- **0x80-0x81**: Fan RPM (16-bit)
- **0x100-0x2FF**: NVMe slot information (8 × 32-byte slots)

### I2C Backend Devices
Connected behind CPLD I2C bridge:
- **CPLD at 0xF0** (JTAG access, I2C register interface)
- **NCT7363 at 0x46** (Fan/thermal controller)
- **Other devices**: Temperature sensors, LED controllers, etc.

## 📝 Example Scripts

### PowerShell: Monitor System Health

```powershell
$device = 0

Write-Host "=== System Health Check ===" -ForegroundColor Cyan

# Temperature
$temp = & ".\M460.exe" $device "BPB" "Sensor" "Get" | ConvertFrom-Json
Write-Host "Backplane Temp: $($temp.Temperature_C)°C"

# Fan Status
$fans = & ".\M460.exe" $device "FAN" "Get" | ConvertFrom-Json
Write-Host "Fan Count: $($fans.FanCount)"
$fans.PSObject.Properties | Where-Object Name -like "Fan*RPM" | ForEach-Object {
    Write-Host "  $($_.Name): $($_.Value) RPM"
}

# Drive Status
$drives = & ".\M460.exe" $device "HDD" "Info" | ConvertFrom-Json
Write-Host "NVMe Drives: $($drives.NVMECount)"
```

### Bash: Batch Firmware Update

```bash
#!/bin/bash

DEVICE=0
MCU_FW="mcu_firmware.bin"
CPLD_FW="cpld_design.svf"

echo "Updating firmware on device $DEVICE..."

# Update CPLD first
echo "Programming CPLD..."
./M460 $DEVICE UPDATE CPLD "$CPLD_FW"
if [ $? -ne 0 ]; then
    echo "CPLD update failed!"
    exit 1
fi

# Update MCU
echo "Programming MCU..."
./M460 $DEVICE UPDATE MCU "$MCU_FW"
if [ $? -ne 0 ]; then
    echo "MCU update failed!"
    exit 1
fi

echo "Firmware update complete!"
./M460 $DEVICE dumpall | jq '.CPLD_Version, .MCU_Version'
```

### Python: Multi-Device Monitoring

```python
import subprocess
import json
import time

def run_command(device, *args):
    cmd = ["./M460.exe", str(device)] + list(args)
    result = subprocess.run(cmd, capture_output=True, text=True)
    return json.loads(result.stdout) if result.returncode == 0 else None

def monitor_devices():
    # Get device list
    list_output = subprocess.run(["./M460.exe", "0", "usblist"], 
                                  capture_output=True, text=True)
    device_count = 2  # Example: 2 devices
    
    for device_id in range(device_count):
        print(f"\n=== Device {device_id} ===")
        
        # Get temperatures
        temp = run_command(device_id, "BPB", "Sensor", "Get")
        if temp:
            print(f"Temperature: {temp['Temperature_C']}°C")
        
        # Get fan info
        fans = run_command(device_id, "FAN", "Get")
        if fans:
            print(f"Fan Count: {fans['FanCount']}")
        
        # Get drive info
        drives = run_command(device_id, "HDD", "Info")
        if drives:
            print(f"NVMe Drives: {drives['NVMECount']}")

if __name__ == "__main__":
    while True:
        monitor_devices()
        time.sleep(5)  # Update every 5 seconds
```

## 📄 Command Protocol Details

### JSON Response Structure

All commands return JSON with this base structure:

```json
{
  "Status": "OK" or "ERROR",
  "Command": "COMMAND_NAME",
  "ErrorCode": 0,
  "ErrorMessage": "Description if Status is ERROR",
  "Data": { ... }
}
```

### Error Codes

| Code | Meaning | Action |
| :--- | :--- | :--- |
| 0 | Success | Operation completed normally |
| 1 | No device found | Verify device connection and Count index |
| 2 | USB communication error | Check cable, retry operation |
| 3 | Invalid parameter | Review command syntax |
| 4 | Device not ready | Try reset: `MCU Reset` |
| 5 | Checksum error | Retry firmware update |
| 6 | File not found | Verify file path and permissions |

## 🔄 Device Lifecycle

1. **Discovery**: `M460.exe 0 usblist`
2. **Identification**: `M460.exe <Count> CPLD Version`
3. **Monitoring**: `M460.exe <Count> dumpall`
4. **Maintenance**: 
   - Firmware: `UPDATE MCU/CPLD`
   - Temperature: `BPB Sensor Get`
   - Fan Control: `FAN Duty Set`
5. **Debugging**: `M460.exe <Count> I2CBYPASS Get`
6. **Reset**: `M460.exe <Count> MCU Reset`

## 📞 Support & Debugging

- **Enable debug output**: Compile with `-DDEBUG` flag
- **USB trace**: Use Wireshark USB capture for protocol analysis
- **Device registers**: Use `dumpall` for comprehensive state dump
- **Linux libusb debug**: `export LIBUSB_DEBUG=3` before running

## 📋 License & Attribution

- **cJSON**: Dual-licensed (MIT/custom) — See `cJSON.c` header
- **libusb**: GNU Lesser General Public License (LGPL) v2.1+
- **Project**: Compatible with embedded system management workflows

## 📌 Version History

- **20260420**: Current stable release
  - Multi-device support (up to 16 concurrent devices)
  - Deterministic device mapping by topology
  - Full JSON output formatting
  - Cross-platform (Windows/Linux) support
  - MCU ISP and CPLD SVF programming
  - Complete hardware monitoring (fans, temps, drives)

## ⚙️ Advanced Configuration

### USB HID Timeout
Default timeout for USB operations: **1000 milliseconds** (configurable in `config.h`)

### Maximum Devices
Default support: **16 concurrent M460 devices** (configurable in `config.h`)

### CPLD Register Mapping
Key register addresses (from `config.h`):
- `0x00`: CPLD Version
- `0x20`: JTAG ID
- `0x30`: HDD Count
- `0x40-0x4F`: HDD Port Status (8 slots)
- `0x50-0x5F`: HDD Status
- `0x60-0x6F`: HDD LED Control
- `0x70`: Fan Duty Cycle
- `0x80-0x81`: Fan RPM (High/Low bytes)
- `0x10-0x11`: Temperature Sensor (High/Low bytes)
- `0x100-0x1BF`: NVMe Slot Info (32 bytes each, 8 slots)

## 📁 Project Structure

- `M460.c`: Command dispatcher and CLI logic.
- `usb_cmd.c`: USB backend, device mapping, and HID communication.
- `mcu_isp.c`: MCU ISP protocol implementation.
- `cpld_svf.c`: SVF parser and JTAG programming engine.
- `config.h`: Hardware register maps and global constants.
- `cJSON.c/h`: Lightweight JSON library.

## 📝 License
This project is for internal technical management and hardware validation.
