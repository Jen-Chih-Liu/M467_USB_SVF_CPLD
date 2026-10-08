# M460 HSUSBD SVF & BMC Firmware - Keil MDK Project

**Project Name:** HSUSBD_HID_Transfer  
**MCU Target:** Nuvoton M467  
**Core Clock:** 200 MHz  
**Build Tool:** Keil MDK-ARM v5 (UV4)

## 📋 Project Overview

This Keil MDK project implements a comprehensive Board Support Package (BSP) for the Nuvoton M460/M467 microcontroller featuring:

- **USB Device (HSUSBD)** communication via HID class interface
- **JTAG/SVF Player** for CPLD and FPGA programming over USB
- **BMC (Baseboard Management Controller)** functionality:
  - Multi-fan control and monitoring (NCT7363Y fan controllers)
  - CPLD status and version monitoring
  - Temperature sensor monitoring
  - NVMe drive basic management interface (NVMe MI)
- **Real-time debug output** via UART3
- **System control** (reset, LED status, bootloader switching)

## 🎯 Key Characteristics

| Property | Value |
| :--- | :--- |
| **Microcontroller** | Nuvoton M467 (ARM Cortex-M4) |
| **Flash Memory** | 512 KB (APROM) + 8 KB (LDROM) |
| **RAM** | 64 KB |
| **USB Speed** | High-Speed USB 2.0 |
| **Core Clock** | 200 MHz (PLL from 12 MHz HXT) |
| **UART Debug** | 115200 bps on UART3 (PB14/PB15) |
| **Compilation Time** | ~10-15 seconds (clean build) |
| **Output Formats** | .hex, .elf, .bin (auto-generated) |

## 🛠 Prerequisites

### Required Software
- **Keil MDK-ARM v5** (v5.30 or later)
- **Nuvoton NuMicro Device Pack** for M460 series
  - [Download](https://www.nuvoton.com/tools-and-support/iot-iot-platform-tools/nuvoton-isp-programming-tool/)
  - Install via Keil `Pack Installer` (within Keil IDE)

### Optional but Recommended
- **Serial Terminal** (PuTTY, TeraTerm) for debug console
- **USB Device Driver** for M467 (auto-installed by Windows in most cases)
- **FILESUM utility** (included in `Keil/` folder for binary verification)

### System Requirements
- **Windows 7 or later** (or Windows Subsystem for Linux)
- **USB 2.0 port** (high-speed recommended)
- **Minimum 2 GB RAM** for IDE operation

## 📂 Project Structure

```
HSUSBD_svf/Keil/
├── HSUSBD_HID_Transfer.uvprojx    # Main Keil project file (XML format)
├── HSUSBD_HID_Transfer.uvoptx     # User options (compiler, debugger, settings)
├── HSUSBD_HID_Transfer.uvguix.*   # GUI workspace (user-specific)
├── Nu_Link_Driver.ini             # Nu-Link debugger configuration
├── EventRecorderStub.scvd         # Cortex-M event recording (optional)
├── FILESUM.exe                    # Binary checksum verification utility
├── obj/                           # Build output directory
│   ├── *.o (object files)
│   ├── *.lst (assembly listings)
│   └── HSUSBD_HID_Transfer.elf
├── lst/                           # Assembly output listings
└── [Source files: main.c, cpld_fan.c, svf.c, etc. in parent directory]
```

## 🔧 Project Configuration

### Target Configuration
- **Device:** M467LLIB (ARM Cortex-M4, 512 KB Flash)
- **Optimization:** Level 2 (-O2)
- **FPU:** Enabled (VFP Single Precision)
- **Compiler:** Arm Compiler 5 (ARMCC)

### Memory Layout
| Region | Address | Size | Purpose |
| :--- | :--- | :--- | :--- |
| **APROM** | 0x00000000 | 512 KB | Application code, data |
| **LDROM** | 0x00100000 | 8 KB | ISP bootloader |
| **RAM** | 0x20000000 | 64 KB | Stack, heap, globals |
| **Config0** | 0x00300000 | 4 B | Security/boot config |

### Peripheral Configuration

#### USB (HSUSBD)
- **Port:** USB High-Speed Device
- **Class:** Human Interface Device (HID)
- **Endpoints:**
  - EP0: Control (64 bytes)
  - EP1 IN: Interrupt (64 bytes)
  - EP2 OUT: Bulk (512 bytes)
  - EP3 IN: Bulk (512 bytes)
- **VID/PID:** 0x0416 / 0x502A (Nuvoton)

#### I2C Interfaces

**I2C0 (PA4=SCL, PA5=SDA)** — Main sensor & control bus
- Frequency: 400 kHz (standard mode)
- Devices:
  - CPLD at 0x78 (7-bit address 0x3C)
  - NCT7363Y fan controllers at 0x88, 0x8C (7-bit: 0x44, 0x46)
  - Temperature sensor at 0x60 (7-bit: 0x30)
  - EEPROM at 0x5C (7-bit: 0x2E)

**I2C1** — I2C MUX for NVMe SMBus
- TCA9548 / PCA9548 multiplexer for selectable NVMe channels

**I2C2 (PA10=SDA, PA11=SCL)** — Secondary CPLD
- Used when dual CPLD configuration detected
- Frequency: 400 kHz

**UI2C0 (USCI0 — PB13=DAT0, PB12=CLK)** — NVMe Management Interface
- 3-wire protocol for NVMe Basic Management
- High-speed serial communication

#### UART
- **UART3:** Debug console
  - Pins: PB14 (RX), PB15 (TX)
  - Baud rate: 115200 bps
  - Data: 8 bits, 1 stop, no parity

#### JTAG (for SVF Programming)
- **PA7:** TCK (Test Clock)
- **PA6:** TMS (Test Mode Select)
- **PC1:** TDI (Test Data In)
- **PC0:** TDO (Test Data Out)
- **Slew Rate:** Fast (for CPLD/FPGA compatibility)
- **Input Buffer:** Schmitt trigger

#### GPIO
- **PB3:** LED_AMBER (active low)
- **PB2:** LED_GREEN (active low)
- **PA9:** HWM_SEL (hardware management mode select)

## 🚀 Building the Project

### Method 1: Keil IDE (GUI)

1. **Open project in Keil:**
   ```
   File → Open Project... → HSUSBD_HID_Transfer.uvprojx
   ```

2. **Configure target (if needed):**
   - Right-click on target in Project window
   - Select `Options for Target...`
   - Verify Device: `M467LLIB`
   - Check compiler/debugger settings

3. **Build project:**
   - **Compile single file:** Ctrl+F7
   - **Build project:** Ctrl+F8 (incremental)
   - **Rebuild all:** Ctrl+Shift+F8 (clean build)

4. **Output location:**
   ```
   obj/HSUSBD_HID_Transfer.elf       # ELF executable (debug symbols)
   obj/HSUSBD_HID_Transfer.hex       # Intel HEX format
   obj/HSUSBD_HID_Transfer.bin       # Raw binary
   ```

5. **Verify build:**
   - Check **Build Output** window (should end with "0 Error(s), N Warning(s)")
   - Executable size typically ~80-150 KB

### Method 2: Command-Line Build

**Using batch script (from fw root directory):**

```batch
cd fw
build_keil_projects.bat
```

**Using UV4 directly:**

```batch
"C:\Keil_v5\UV4\UV4.exe" -b "fw\SampleCode\StdDriver\HSUSBD_svf\Keil\HSUSBD_HID_Transfer.uvprojx" -j0 -o build.log
```

**Return codes:**
- 0 = Success
- 1 = Warnings
- \>1 = Build errors

### Method 3: Custom Build (No GUI)

```batch
REM Minimal build using armcc (if Keil installed)
cd fw\SampleCode\StdDriver\HSUSBD_svf
armcc --c99 -c main.c -o obj/main.o
armlink obj/*.o -o obj/HSUSBD_HID_Transfer.elf
fromelf obj/HSUSBD_HID_Transfer.elf --bin -o obj/HSUSBD_HID_Transfer.bin
```

## 📋 Build Output Files

### Primary Artifacts

| File | Format | Purpose | Size |
| :--- | :--- | :--- | :--- |
| `.elf` | ELF executable | Debug symbols, full info | ~180 KB |
| `.hex` | Intel HEX | Programming into APROM | ~150 KB (text) |
| `.bin` | Raw binary | Direct flash write, compact | ~80-100 KB |

### Secondary Artifacts

| File | Purpose |
| :--- | :--- |
| `.lst` | Assembly listing with source lines |
| `.map` | Linker map file (symbol table, memory layout) |
| `_build.log` | Detailed build output (warnings, errors) |

### Using FILESUM for Verification

```bash
cd Keil
FILESUM.exe obj/HSUSBD_HID_Transfer.bin
```

Output:
```
SHA256: 1a2b3c4d5e6f7g8h...
MD5: 9i8j7k6l5m4n3o2p...
```

## 🐛 Debugging in Keil

### 1. Setting Up Debugger

**Using Nu-Link Debugger:**
1. Connect Nu-Link USB debugger to M467 board
2. Project → Options for Target → **Debugger** tab
3. Select: `Nu-Link Debugger`
4. Click `Settings` → Verify connection

**Alternative: J-Link Debugger**
1. Select: `J-Link / J-Trace Cortex`
2. Configure device ID and interface (SWD recommended)

### 2. Debug Breakpoints

```c
// In main.c, set breakpoint at critical sections:
int main(void) {
    SYS_Init();           // ← Breakpoint to verify clock setup
    USBD_Open(...);       // ← Check USB initialization
    while (1) {
        // ← Breakpoint in main loop
    }
}
```

**Keil keyboard shortcuts:**
- **F5:** Start debug session
- **Ctrl+F5:** Stop debug session
- **F10:** Step over
- **F11:** Step into
- **Ctrl+F11:** Step out
- **F9:** Toggle breakpoint

### 3. Watch Window

Monitor variables in real-time:
1. **Debug → Watch Windows → Watch 1**
2. Add expressions:
   ```
   usb_rcvbuf[0]         (USB receive buffer first byte)
   buffer_index          (Current USB packet position)
   timer0_count          (System timer for monitoring)
   i2c_read_report[0]    (I2C sensor reading)
   ```

### 4. Serial Debug Output

Since UART3 is configured for 115200 bps debug output:

**PuTTY/TeraTerm setup:**
- Port: COM (identified in Device Manager)
- Speed: 115200 bps
- Data bits: 8, Stop bits: 1, Parity: None
- Open terminal window to see real-time debug messages

**Sample debug output:**
```
[SYS] Core Clock: 200000000 Hz
[USB] HSUSBD Initialized
[I2C0] CPLD detected at 0x3C
[I2C0] Temperature: 28.5°C
[FAN] NCT7363Y at 0x44 configured
[NVMe] MI channel 0 ready
```

## 🔗 Linker Script Configuration

**Linker Script:** Automatically configured via device pack

**Key sections:**
```
Region      Base Address     Size         Purpose
IROM1       0x00000000       512 KB       APROM (application)
IRAM1       0x20000000       64 KB        RAM (stack, heap)
IROM2       0x00100000       8 KB         LDROM (bootloader)
```

**Adjusting memory usage:**
1. Project → Options for Target → **Linker** tab
2. Modify `--ro-base`, `--rw-base`, `--first` to relocate sections
3. Recommended: Use defaults unless specific memory requirements

## 🔐 Security & Config

### Config Bytes (Non-Volatile)

Located at address 0x00300000, controls:

| Byte | Setting | Default | Purpose |
| :--- | :--- | :--- | :--- |
| 0 | User config0 | 0xFF | Boot from APROM, flash access enabled |
| 1 | User config1 | 0xFF | ISP enable, watchdog disable |
| 2 | User config2 | 0xFF | Reserved |
| 3 | User config3 | 0xFF | Reserved |

**Warning:** Incorrect config can make device unbrick-able. Only modify if you understand the consequences.

### ISP Bootloader (LDROM)

- **Base:** 0x00100000
- **Size:** 8 KB
- **Purpose:** In-System Programming via ISP protocol
- **Activation:** Set LDROM boot bit in config bytes or GPIO trigger

For ISP updates via M460.exe app tool, ensure bootloader is present and boot config is set to LDROM.

## 🔄 Project Workflow

### Clean Build (Remove All Intermediates)

```batch
rem Windows: Clean from Keil GUI
Project → Rebuild All (Ctrl+Shift+F8)

rem Or manually delete:
rmdir /s /q obj lst
```

### Incremental Build (Faster)

```batch
rem Only recompile changed files
Project → Build (Ctrl+F8)
```

### Binary Output Only (Skip Debug Info)

```batch
rem Convert to compact binary
fromelf obj/HSUSBD_HID_Transfer.elf --bin -o HSUSBD_HID_Transfer.bin
```

### Verify Build Success

- **No errors**: "0 Error(s), N Warning(s)"
- **Output present**: `obj/HSUSBD_HID_Transfer.elf` exists and is >100 KB
- **Buildlog clean**: No "undefined reference" messages

## 📤 Flashing Firmware

### Option 1: Using M460.exe CLI Tool

After building, copy `.bin` output to app folder and program MCU:

```batch
cd ..\..\..\..\..\app
M460.exe 0 UPDATE MCU ..\fw\SampleCode\StdDriver\HSUSBD_svf\obj\HSUSBD_HID_Transfer.bin
```

### Option 2: Keil Download

In Keil IDE:
1. Project → Options for Target → **Utilities** tab
2. Enable: `Use External Tool for File Download`
3. Configure **Nuvoton Programmer** if available
4. Debug → Download (Ctrl+F10)

### Option 3: Nu-Link Debugger

Connected via USB, Keil automatically downloads during debug session start.

### Verification

After flashing:
```batch
M460.exe 0 MCU Version
M460.exe 0 dumpall
```

Verify output shows updated firmware version.

## ⚠️ Common Issues & Solutions

### Issue 1: "Device Not Found"

**Problem:** Keil can't connect to M467 debugger

**Solutions:**
1. Verify USB cable connection to Nu-Link
2. Check Device Manager for "M467" device (should appear)
3. Update Nu-Link driver:
   - Delete `Nu_Link_Driver.ini`
   - Restart Keil (will auto-reinstall driver)
4. Try different USB port (USB 3.0 sometimes has issues)
5. Install latest Keil MDK update

### Issue 2: "Compile Error: undefined reference to SYS_Init"

**Problem:** Missing library files

**Solutions:**
1. Verify Device Pack installed:
   - Tools → Package Manager → Check M460 pack listed
2. Project → Options → Device tab → verify `M467LLIB` selected
3. Check Include Paths:
   - Verify `Library/CMSIS/Include/` and `Library/StdDriver/inc/` present
4. Rebuild all: Ctrl+Shift+F8

### Issue 3: USB HID Not Recognized on Windows

**Problem:** Device not appearing in Device Manager

**Solutions:**
1. First-time setup: Windows auto-install drivers (can take 30 seconds)
2. If fails, manually install:
   - Extract `libusb-win32` drivers
   - Device Manager → right-click unknown device → Update driver → Browse → Select driver
3. Check VID/PID matches (0x0416/0x502A)
4. Try different USB port

### Issue 4: Intermittent I2C Bus Hangs

**Problem:** I2C0 sometimes locks up, requiring reset

**Causes:** Slave device holds SDA low (bus stuck)

**Solutions:**
1. **Hardware:** Add 47Ω pull-up resistors to SDA/SCL if not present
2. **Software:** Implement I2C bus recovery:
   - Toggle SCL 9 times while monitoring SDA
   - Generate STOP condition when SDA goes high
3. **CPLD:** Verify CPLD I2C firmware is correct version

### Issue 5: Build Takes >30 Seconds

**Problem:** Slow compilation

**Solutions:**
1. Use **incremental build** (Ctrl+F8) instead of rebuild
2. Check Optimization level: use `-O2` (not `-O3` for compile speed)
3. Disable **Event Recorder** if not needed:
   - Project → Options → Code Generation → uncheck "Enable Event Recording"
4. Use RAM disk for `obj/` folder (if SSD is slow)

### Issue 6: "Flash size exceeds 512 KB"

**Problem:** Binary too large for M467

**Solutions:**
1. Check code size: Review linker `.map` file
2. Disable unused features in `main.c`
3. Use `-O2` or `-O3` optimization to reduce code size
4. Check for duplicate object files in project

## 📚 Key Source Files Reference

| File | Purpose | Key Functions |
| :--- | :--- | :--- |
| **main.c** | Entry point, command dispatch | `main()`, USB command handler |
| **cpld_fan.c** | CPLD & fan control logic | `read_cpld()`, `set_fan_duty()` |
| **svf.c / xsvf.c** | SVF/JTAG player | `xsvftool_esp_svf_packet()` |
| **nvme_mi.c** | NVMe management interface | `nvme_mi_read()`, `nvme_mi_write()` |
| **hid_transfer.c** | USB HID packet handling | `USBD_HID_DataReceived()` |
| **descriptors.c** | USB device descriptors | USB HID report descriptor |
| **g_def.h** | Global definitions | I2C addresses, register maps |

## 🔗 References

### Nuvoton Documentation
- M460 Series Datasheet
- M460 BSP User Manual
- M460 Standard Driver Documentation (included in device pack)

### External Standards
- USB HID specification (www.usb.org)
- JTAG/SVF Standards (IEEE 1149.1)
- I2C Specification (NXP)

### Keil MDK Resources
- Keil Online Documentation: www.keil.com/support/
- ARM CMSIS Documentation
- Cortex-M4 Programming Guide

## ✅ Build Checklist

Before distributing firmware:

- [ ] **Compilation**: Build succeeds with 0 errors
- [ ] **Warnings**: All warnings reviewed (error-level ones fixed)
- [ ] **Binary size**: Reasonable (<200 KB for this application)
- [ ] **Debug test**: Debugger connection works
- [ ] **Flash verification**: Firmware flashes without errors
- [ ] **Functional test**: Device responds to `M460.exe 0 MCU Version`
- [ ] **UART debug**: Debug messages output correctly on serial terminal
- [ ] **Performance**: No obvious delays or hangs in main loop

## 📝 Version History

| Date | Version | Changes |
| :--- | :--- | :--- |
| 2026-06-03 | 3.00 | M467 MCU, dual CPLD support, NVMe MI interface |
| 2025-11-15 | 2.50 | Enhanced JTAG/SVF, fan duty cycle control |
| 2024-06-10 | 2.00 | Multi-device support, I2C1 mux for NVMe |

## 📞 Support & Troubleshooting

For issues or questions:

1. **Check Keil documentation**: www.keil.com/support
2. **Review debug output**: Connect UART3 to serial terminal
3. **Inspect `.map` file**: Understand memory usage and undefined symbols
4. **Use debugger**: Set breakpoints and inspect variables
5. **Check device power**: Verify MCU has stable power supply

---

**Last Updated:** June 3, 2026  
**Project:** M460 HSUSBD SVF & BMC Firmware  
**Maintainer:** Nuvoton Technology
