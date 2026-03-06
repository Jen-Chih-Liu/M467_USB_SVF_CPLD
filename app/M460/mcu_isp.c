/**
 * @file mcu_isp.c
 * @brief In-System Programming (ISP) module for Nuvoton MCU firmware update via USB HID.
 * 
 * This module provides functions to program MCU APROM through USB interface using
 * the ISP (In-System Programming) protocol. It handles file loading, USB communication,
 * firmware transfer, and verification.
 * 
 * Key Features:
 * - USB HID communication with MCU bootloader (LDROM)
 * - Binary file loading and validation
 * - APROM programming with packet-based protocol
 * - Checksum verification
 * - Device identification and handshaking
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if defined(_WIN32) || defined(_WIN64)
#include "libusb.h"
#else
// On Linux, pkg-config sometimes automatically points the include path to the libusb-1.0 folder.
// To be safe, you can use __has_include check (C++17) or write it directly.
#include <libusb-1.0/libusb.h>
#endif
#include  "config.h"

// === Global Variables ===

/** @brief Global USB device handle for ISP operations */
libusb_device_handle* g_dev_handle = NULL;

/** @brief Timeout value for USB operations (milliseconds) */
#define Time_Out_Value 1000 // Overall operation timeout (ms)

/** @brief USB IN endpoint address */
uint8_t ENDPOINT_IN = 0;

/** @brief USB OUT endpoint address */
uint8_t ENDPOINT_OUT = 0;

/** @brief Receive buffer for USB packets */
unsigned char buffer[full_Pkg_Size] = { 0 };

/** @brief Packet sequence number for protocol synchronization */
unsigned int PacketNumber;

/** @brief Memory address and size variables */
unsigned int Address, Size;

/** @brief Transmit buffer for USB packets */
unsigned char send_buf[full_Pkg_Size];

/** @brief Command code for APROM update operation */
#define CMD_UPDATE_APROM      0x000000A0

/** @brief Buffer to store APROM binary data */
unsigned char W_APROM_BUFFER[aprom_size_t];

/** @brief Actual size of loaded firmware file */
unsigned int file_size;

/** @brief Checksum of firmware file */
unsigned int file_checksum;

/** @brief Temporary checksum calculation variable */
unsigned int file_checksum_temp;

/** @brief Flag indicating USB connection status (0=closed, 1=open) */
unsigned int USB_OPEN_FLAG;

/** @brief Total APROM size for programming */
unsigned int APROM_SIZE;


// === Utility Functions ===

/**
 * @brief Copies bytes from source to destination buffer.
 * 
 * This is a custom memory copy function used for preparing USB packets.
 * Similar to memcpy but implemented explicitly for clarity.
 * 
 * @param dest Pointer to destination buffer.
 * @param src Pointer to source buffer.
 * @param size Number of bytes to copy.
 */
void WordsCpy(void* dest, void* src, unsigned int size)
{
    unsigned char* pu8Src, * pu8Dest;
    unsigned int i;

    pu8Dest = (unsigned char*)dest;
    pu8Src = (unsigned char*)src;

    // Copy byte by byte
    for (i = 0; i < size; i++)
        pu8Dest[i] = pu8Src[i];
}

// === USB Device Management Functions ===

/**
 * @brief Locates and opens the ISP HID device, then retrieves endpoint addresses.
 * 
 * This function searches for a USB HID device with specific VID/PID (bootloader mode),
 * claims the interface, and discovers the IN/OUT interrupt endpoint addresses.
 * 
 * @param handle Output pointer to store the opened device handle.
 * @param ep_out Output pointer to store the OUT endpoint address.
 * @param ep_in Output pointer to store the IN endpoint address.
 * @return int 0 on success, -1 on failure.
 */
int find_hid_device_and_endpoints_isp(libusb_device_handle** handle, uint8_t* ep_out, uint8_t* ep_in) {
    // Attempt to open device with specified VID/PID (LDROM bootloader)
    *handle = libusb_open_device_with_vid_pid(NULL, USB_VID_LD, USB_PID_LD);
    if (*handle == NULL) {
        fprintf(stderr, "error find Vendor ID: 0x%04x, Product ID: 0x%04x\n\r", USB_VID_LD, USB_PID_LD);
        return -1;
    }

    // Detach kernel driver (if currently attached) - Required on Linux
    libusb_detach_kernel_driver(*handle, INTERFACE_NUMBER);

    // Claim the interface for exclusive access
    int r = libusb_claim_interface(*handle, INTERFACE_NUMBER);
    if (r < 0) {
        fprintf(stderr, "Can't claim usb: %s\n", libusb_error_name(r));
        libusb_close(*handle);
        return -1;
    }

    // Retrieve configuration descriptor to find endpoints
    struct libusb_config_descriptor* config;
    libusb_get_active_config_descriptor(libusb_get_device(*handle), &config);
    const struct libusb_interface* inter = &config->interface[INTERFACE_NUMBER];
    const struct libusb_interface_descriptor* inter_desc = &inter->altsetting[0];

    // Initialize endpoint addresses
    *ep_out = 0;
    *ep_in = 0;

    // Iterate through all endpoints to find IN and OUT interrupt endpoints
    for (int i = 0; i < inter_desc->bNumEndpoints; i++) {
        const struct libusb_endpoint_descriptor* ep_desc = &inter_desc->endpoint[i];
        if ((ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT) {
            *ep_out = ep_desc->bEndpointAddress;
        }
        else if ((ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
            *ep_in = ep_desc->bEndpointAddress;
        }
    }

    // Free configuration descriptor
    libusb_free_config_descriptor(config);

    // Validate that both endpoints were found
    if (*ep_out == 0 || *ep_in == 0) {
        fprintf(stderr, "Can't find IN/OUT\n\r");
        libusb_release_interface(*handle, INTERFACE_NUMBER);
        libusb_close(*handle);
        return -1;
    }

    return 0;
}

/**
 * @brief Opens USB connection to the ISP device in bootloader mode.
 * 
 * Initializes libusb library, locates the target device, and establishes
 * communication endpoints. Returns immediately if already connected.
 * 
 * @return ISP_STATE RES_CONNECT on success, RES_CONNECT_FALSE on failure.
 */
ISP_STATE OPEN_USBPORT(void)
{
    // Check if already connected
    if (g_dev_handle != NULL) {
        return RES_CONNECT;
    }
    
    // Initialize libusb library
    int r = libusb_init(NULL);
    if (r < 0) {
        fprintf(stderr, "libusb init false, %s\n\r ", libusb_error_name(r));
        return RES_CONNECT_FALSE;
    }

    // Find and open the ISP device, retrieve endpoints
    r = find_hid_device_and_endpoints_isp(&g_dev_handle, &ENDPOINT_OUT, &ENDPOINT_IN);
    if (r != 0) {
        libusb_exit(NULL);
        return RES_CONNECT_FALSE;
    }
    return RES_CONNECT;

}

/**
 * @brief Closes the USB connection and releases resources.
 * 
 * Releases the claimed interface, closes the device handle, and resets
 * connection flags.
 * 
 * @return ISP_STATE Always returns RES_DISCONNECT.
 */
ISP_STATE CLOSE_USB_PORT(void)
{
    // Release USB resources if connected
    if (USB_OPEN_FLAG == 1 && g_dev_handle != NULL) {
        libusb_release_interface(g_dev_handle, 0);
        libusb_close(g_dev_handle);
        //libusb_exit(g_ctx);
        g_dev_handle = NULL;
        //g_ctx = NULL;
        USB_OPEN_FLAG = 0;
    }
    return RES_DISCONNECT;
}

// === ISP Protocol Command Functions ===

/**
 * @brief Reads the firmware version from the bootloader.
 * 
 * Sends command 0xA6 to query the firmware version and waits for response.
 * 
 * @return unsigned int Firmware version on success, 0 on timeout or error.
 */
unsigned int READFW_VERSION_USB(void)
{
    int r;
    int transferred;
    
    // Prepare command packet: 0xA6 = Read FW Version
    unsigned char cmd[full_Pkg_Size] = { 0xa6,0,0,0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    // Send command to device
    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd, sizeof(cmd), &transferred, 2000);
    if (r != 0) {
        printf("Write error in READFW_VERSION_USB: %s\n", libusb_error_name(r));
        return 0;
    }

    // Wait for response with timeout
    clock_t start_time = clock();
    while ((clock() - start_time) < Time_Out_Value)
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 2000);
        if (r == 0 && transferred > 0) {
            dbg_printf("package: 0x%x\n\r", buffer[4]);
            // Check if packet number matches expected response
            if ((buffer[4] | ((buffer[5] << 8) & 0xff00) | ((buffer[6] << 16) & 0xff0000) | ((buffer[7] << 24) & 0xff000000)) == (PacketNumber + 1)) {
                PacketNumber += 2;
                // Extract and return firmware version from bytes 8-11
                return (buffer[8] | ((buffer[9] << 8) & 0xff00) | ((buffer[10] << 16) & 0xff0000) | ((buffer[11] << 24) & 0xff000000));
            }
        }
        else if (r != LIBUSB_ERROR_TIMEOUT) {
            printf("Read error in READFW_VERSION_USB: %s\n", libusb_error_name(r));
            return 0;
        }
    }
    dbg_printf("Timeout in READFW_VERSION_USB\n");
    return 0;
}

/**
 * @brief Checks the boot status of the MCU.
 * 
 * Sends command 0xCA to query which firmware bank (APROM/LDROM) the MCU booted from.
 * 
 * @return unsigned int Boot status byte on success, 0 on timeout or error.
 */
unsigned int CHECK_BOOT_USB(void)
{
    int r;
    int transferred;
    
    // Prepare command packet: 0xCA = Check Boot Status
    unsigned char cmd[full_Pkg_Size] = { 0xca,0,0,0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    // Send command to device
    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd, sizeof(cmd), &transferred, 2000);
    if (r != 0) {
        printf("Write error in CHECK_BOOT_USB: %s\n", libusb_error_name(r));
        return 0;
    }

    // Wait for response with timeout
    clock_t start_time = clock();
    while ((clock() - start_time) < Time_Out_Value)
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 200);
        if (r == 0 && transferred > 0) {
            dbg_printf("package: 0x%x\n\r", buffer[4]);
            // Check if packet number matches expected response
            if ((buffer[4] | ((buffer[5] << 8) & 0xff00) | ((buffer[6] << 16) & 0xff0000) | ((buffer[7] << 24) & 0xff000000)) == (PacketNumber + 1)) {
                PacketNumber += 2;
                // Return boot status from byte 8
                return (buffer[8]);
            }
        }
        else if (r != LIBUSB_ERROR_TIMEOUT) {
            dbg_printf("Read error in CHECK_BOOT_USB: %s\n", libusb_error_name(r));
            return 0;
        }
    }
    dbg_printf("Timeout in CHECK_BOOT_USB\n");
    return 0;
}

/**
 * @brief Commands the MCU to jump to APROM (main application firmware).
 * 
 * Sends command 0xAB to instruct the bootloader to execute the main firmware.
 * No response is expected as the MCU will reset and disconnect from USB.
 */
void RUN_TO_APROM_USB(void)
{
    int transferred;
    
    // Prepare command packet: 0xAB = Run to APROM
    unsigned char cmd[full_Pkg_Size] = { 0xab,0,0,0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    // Send command (MCU will reset, no ACK expected)
    int r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd, sizeof(cmd), &transferred, 2000);
    if (r != 0) {
        dbg_printf("Write error in RUN_TO_APROM_USB: %s\n", libusb_error_name(r));
    }
    PacketNumber += 2;
}
#if 0
void RUN_TO_LDROM_USB(void)
{
    int transferred;
    unsigned char cmd[Package_Size] = { 0xac,0,0,0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    int r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd, sizeof(cmd), &transferred, 2000);
    if (r != 0) {
        printf("Write error in RUN_TO_LDROM_USB: %s\n", libusb_error_name(r));
    }
    PacketNumber += 2;
}
#endif
/**
 * @brief Reads the Product ID (PID) from the target MCU.
 * 
 * Sends command 0xB1 to read the MCU's unique product identification number.
 * This verifies the target device type.
 * 
 * @return ISP_STATE RES_PASS on success, RES_TIME_OUT on failure.
 */
ISP_STATE READ_PID_USB(void)
{
    int r;
    int transferred;
    
    // Prepare command packet: 0xB1 = Read PID
    unsigned char cmd[full_Pkg_Size] = { 0xB1,0,0,0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    // Send command to device
    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd, sizeof(cmd), &transferred, 2000);
    if (r != 0) {
        printf("Write error in READ_PID_USB: %s\n", libusb_error_name(r));
        return RES_TIME_OUT;
    }

    // Wait for response with timeout
    clock_t start_time = clock();
    while ((clock() - start_time) < Time_Out_Value)
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 2000);
        if (r == 0 && transferred > 0) {
            dbg_printf("package: 0x%x\n\r", buffer[4]);
            // Check if packet number matches expected response
            if ((buffer[4] | ((buffer[5] << 8) & 0xff00) | ((buffer[6] << 16) & 0xff0000) | ((buffer[7] << 24) & 0xff000000)) == (PacketNumber + 1))
            {
                PacketNumber += 2;
                // Extract and display Product ID
                unsigned int temp_PDID = buffer[8] | ((buffer[9] << 8) & 0xff00) | ((buffer[10] << 16) & 0xff0000) | ((buffer[11] << 24) & 0xff000000);
                dbg_printf("Pid=x%x\n\r", temp_PDID);
                return RES_PASS;
            }
        }
        else if (r != LIBUSB_ERROR_TIMEOUT) {
            dbg_printf("Read error in READ_PID_USB: %s\n", libusb_error_name(r));
            return RES_TIME_OUT;
        }
    }

    dbg_printf("Timeout in READ_PID_USB\n");
    return RES_TIME_OUT;
}

/**
 * @brief Performs initial handshake with bootloader using SN (Serial Number) packet.
 * 
 * Sends command 0xA4 to establish communication and verify bootloader readiness.
 * This is typically the first command sent after connecting.
 * 
 * @return ISP_STATE RES_SN_OK on success, RES_TIME_OUT on failure.
 */
ISP_STATE SN_PACKAGE_USB(void)
{
    int r;
    int transferred=64;
    
    // Prepare command packet: 0xA4 = SN Package (Handshake)
    unsigned char cmd1[full_Pkg_Size] = { 0xa4,0,0,0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff),
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    // Send handshake command to device
    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd1, full_Pkg_Size, &transferred, 2000);
    if (r != 0) {
        printf("Write error in SN_PACKAGE_USB: %s\n", libusb_error_name(r));
        return RES_TIME_OUT;
    }

    // Wait for acknowledgment with timeout
    clock_t start_time = clock();
    while ((clock() - start_time) < Time_Out_Value)
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 2000);
        if (r == 0 && transferred > 0) {
            dbg_printf("package: 0x%x\n\r", buffer[4]);
            // Check if packet number matches expected response
            if ((buffer[4] | ((buffer[5] << 8) & 0xff00) | ((buffer[6] << 16) & 0xff0000) | ((buffer[7] << 24) & 0xff000000)) == (PacketNumber + 1))
            {
                PacketNumber += 2;
                return RES_SN_OK;
            }
        }
        else if (r != LIBUSB_ERROR_TIMEOUT) {
            dbg_printf("Read error in SN_PACKAGE_USB: %s\n", libusb_error_name(r));
            return RES_TIME_OUT;
        }
    }

    dbg_printf("Timeout in SN_PACKAGE_USB\n");
    return RES_TIME_OUT;
}

/**
 * @brief Reads configuration registers from the MCU.
 * 
 * Sends command 0xA2 to retrieve MCU configuration settings (CONFIG0, CONFIG1).
 * These registers control boot options, security settings, and other MCU parameters.
 */
void READ_CONFIG_USB(void)
{
    int r;
    int transferred;
    
    // Prepare command packet: 0xA2 = Read Config
    unsigned char cmd[full_Pkg_Size] = { 0xa2,0, 0, 0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    // Send command to device
    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd, sizeof(cmd), &transferred, 2000);
    if (r != 0) {
        dbg_printf("Write error in READ_CONFIG_USB: %s\n", libusb_error_name(r));
        return;
    }

    // Wait for response with timeout
    clock_t start_time = clock();
    while ((clock() - start_time) < Time_Out_Value)
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 200);
        if (r == 0 && transferred > 0) {
            dbg_printf("package: 0x%x\n\r", buffer[4]);
            // Check if packet number matches expected response
            if ((buffer[4] | ((buffer[5] << 8) & 0xff00) | ((buffer[6] << 16) & 0xff0000) | ((buffer[7] << 24) & 0xff000000)) == (PacketNumber + 1))
            {
                // Display CONFIG0 register (bytes 8-11)
                dbg_printf("config0: 0x%x\n\r", (buffer[8] | ((buffer[9] << 8) & 0xff00) | ((buffer[10] << 16) & 0xff0000) | ((buffer[11] << 24) & 0xff000000)));
                // Display CONFIG1 register (bytes 12-15)
                dbg_printf("config1: 0x%x\n\r", (buffer[12] | ((buffer[13] << 8) & 0xff00) | ((buffer[14] << 16) & 0xff0000) | ((buffer[15] << 24) & 0xff000000)));
                PacketNumber += 2;
                return;
            }
        }
        else if (r != LIBUSB_ERROR_TIMEOUT) {
            dbg_printf("Read error in READ_CONFIG_USB: %s\n", libusb_error_name(r));
            return;
        }
    }
    dbg_printf("Time out in READ_CONFIG_USB\n\r");
}

/**
 * @brief Programs the APROM (Application ROM) with firmware from buffer.
 * 
 * This is the main firmware update function. It:
 * 1. Sends erase command with first 48 bytes of data
 * 2. Waits for erase completion (can take up to 10 seconds)
 * 3. Sends remaining firmware data in 56-byte chunks
 * 4. Verifies checksum after transfer completes
 * 
 * @return ISP_STATE RES_PASS on success, RES_FALSE or RES_TIME_OUT on failure.
 */
ISP_STATE UPDATE_APROM_USB(void)
{
    int r;
    int transferred;
    unsigned short get_cksum;
    file_size = aprom_size_t;

    // --- Phase 1: Send erase command with initial data ---
    memset(send_buf, 0, full_Pkg_Size);
    unsigned long cmdData = CMD_UPDATE_APROM;
    unsigned long startaddr = 0;
    
    // Prepare first packet: Command(4) + PacketNum(4) + StartAddr(4) + Size(4) + FirstData(48)
    WordsCpy(send_buf + 0, &cmdData, 4);
    WordsCpy(send_buf + 4, &PacketNumber, 4);
    WordsCpy(send_buf + 8, &startaddr, 4);
    WordsCpy(send_buf + 12, &file_size, 4);
    WordsCpy(send_buf + 16, W_APROM_BUFFER + 0, 48);

    // Send erase/program start command
    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, send_buf, sizeof(send_buf), &transferred, 00);
    if (r != 0) {
        dbg_printf("Write error at start of UPDATE_APROM_USB: %s\n", libusb_error_name(r));
        return RES_FALSE;
    }

    // Wait for erase completion (may take up to 10 seconds)
    BOOL ack_received = FALSE;
    clock_t start_time = clock();
    while ((clock() - start_time) < 10000) // Erase can take longer, use 10s timeout
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 500);
        if (r == 0 && transferred > 0) {
            // Check if packet number matches expected ACK
            if ((buffer[4] | ((buffer[5] << 8) & 0xff00) | ((buffer[6] << 16) & 0xff0000) | ((buffer[7] << 24) & 0xff000000)) == (PacketNumber + 1)) {
                ack_received = TRUE;
                break;
            }
        }
        else if (r != LIBUSB_ERROR_TIMEOUT) {
            dbg_printf("Read error at start of UPDATE_APROM_USB: %s\n", libusb_error_name(r));
            return RES_FALSE;
        }
    }

    if (!ack_received) {
        dbg_printf("Timeout waiting for erase ACK\n\r");
        return RES_TIME_OUT;
    }

    PacketNumber = PacketNumber + 2;
    dbg_printf("erase down\n\r");


    // --- Phase 2: Transfer remaining firmware data in chunks ---
    int last_progress = -1;
    for (unsigned int i = 48; i < file_size; i = i + 56)
    {
        // Display programming progress
        dbg_printf("Process=%.2f \r", (float)((float)i / (float)file_size) * 100);

        // Prepare data packet
        memset(send_buf, 0, 64);
        WordsCpy(send_buf + 4, &PacketNumber, 4);

        // Calculate chunk size (56 bytes per packet, last packet may be smaller)
        unsigned int chunk_size = (file_size - i > 56) ? 56 : (file_size - i);
        WordsCpy(send_buf + 8, W_APROM_BUFFER + i, chunk_size);

        // Send data packet
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, send_buf, sizeof(send_buf), &transferred, 2000);
        if (r != 0) {
            dbg_printf("Write error during APROM update: %s\n", libusb_error_name(r));
            goto out1;
        }

        // Wait for acknowledgment
        ack_received = FALSE;
        start_time = clock();
        while ((clock() - start_time) < Time_Out_Value)
        {
            r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 200);
            if (r == 0 && transferred > 0) {
                // Check if packet number matches expected ACK
                if ((buffer[4] | ((buffer[5] << 8) & 0xff00) | ((buffer[6] << 16) & 0xff0000) | ((buffer[7] << 24) & 0xff000000)) == (PacketNumber + 1)) {
                    ack_received = TRUE;
                    break;
                }
            }
            else if (r != LIBUSB_ERROR_TIMEOUT) {
                dbg_printf("Read error during APROM update: %s\n", libusb_error_name(r));
                goto out1;
            }
        }

        if (!ack_received) {
            dbg_printf("Timeout waiting for data packet ACK\n\r");
            goto out1;
        }

        // For the last packet, extract and display checksum from response
        if (i + 56 >= file_size) { // Last packet
            WordsCpy(&get_cksum, buffer + 8, 2);
            dbg_printf("get_cksum: 0x%x\n\r", get_cksum);
        }

        PacketNumber = PacketNumber + 2;
    }

    // Programming completed successfully
    dbg_printf("\r                                        ");
    dbg_printf("\r program progress: 100%% \n\r");

    return RES_PASS;

out1:
    // Error exit label
    dbg_printf("\r program false!! \n\r");
    return RES_FALSE;
}

// === File Operations ===

/**
 * @brief Loads a binary firmware file into the APROM buffer.
 * 
 * Opens the specified file, validates its size, and loads it into W_APROM_BUFFER.
 * Buffer is pre-filled with 0xFF (erased state).
 * 
 * @param temp Path to the binary firmware file.
 * @return ISP_STATE RES_FILE_LOAD on success, RES_FILE_NO_FOUND if file not found,
 *         RES_FILE_SIZE_OVER if file exceeds maximum size.
 */
ISP_STATE File_Open_APROM(const char* temp)
{
    FILE* fp;
    file_size = 0;
    
    // Initialize buffer to 0xFF (erased flash state)
    for (unsigned int i = 0; i < aprom_size_t; i++)
    {
        W_APROM_BUFFER[i] = 0xff;
    }
    
    // Attempt to open binary file
    if ((fp = fopen(temp, "rb")) == NULL)
    {
        dbg_printf("APROM FILE OPEN FALSE\n\r");
        return RES_FILE_NO_FOUND;
    }
    if (fp != NULL)
    {
        // Get file size by seeking to end
        // Fix for reading file size correctly
        fseek(fp, 0, SEEK_END);
        file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        // Validate file size doesn't exceed APROM capacity
        if (file_size > aprom_size_t) {
            fclose(fp);
            return RES_FILE_SIZE_OVER;
        }
        
        // Read entire file into buffer
        fread(W_APROM_BUFFER, 1, file_size, fp);
        fclose(fp);
    }

    //file_checksum = 0;
    //for (unsigned int i = 0; i < aprom_size_t - 8; i++)
    //{
      //  file_checksum = file_checksum + W_APROM_BUFFER[i];
    //}
    return RES_FILE_LOAD;
}

/**
 * @brief Appends checksum information to the end of APROM buffer.
 * 
 * The last 8 bytes of APROM are reserved for:
 * - Bytes [SIZE-8 to SIZE-5]: Data length (4 bytes)
 * - Bytes [SIZE-4 to SIZE-1]: Checksum (4 bytes, 16-bit truncated)
 * 
 * This function calculates the total checksum of the entire buffer including
 * these appended bytes.
 */
void APROM_AND_CHECKSUM(void)
{
    APROM_SIZE = aprom_size_t;
    
    // Store data length in last 8 bytes (little-endian)
    W_APROM_BUFFER[(APROM_SIZE)-8] = ((aprom_size_t - 8) >> 0) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-7] = ((aprom_size_t - 8) >> 8) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-6] = ((aprom_size_t - 8) >> 16) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-5] = ((aprom_size_t - 8) >> 24) & 0xff;
    
    // Store original checksum in last 4 bytes
    W_APROM_BUFFER[(APROM_SIZE)-4] = (file_checksum >> 0) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-3] = (file_checksum >> 8) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-2] = (file_checksum >> 16) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-1] = (file_checksum >> 24) & 0xff;
    
    // Calculate total checksum of entire buffer (including appended data)
    file_checksum_temp = 0;
    for (unsigned int i = 0; i < APROM_SIZE; i++)
    {
        file_checksum_temp += W_APROM_BUFFER[i];
    }
    
    // Truncate to 16-bit checksum
    file_checksum_temp = file_checksum_temp & 0xffff;
    file_size = APROM_SIZE;
    file_checksum = file_checksum_temp; //total array checksum
}

/**
 * @brief Initializes global variables for ISP operations.
 * 
 * Resets packet counter, address pointer, and connection flags to their
 * initial state before starting a new ISP session.
 */
void ISP_COMMAND_init(void)
{

    USB_OPEN_FLAG = 0;
    Address = 0;
    PacketNumber = 1;
    APROM_SIZE = 0;
}

/**
 * @brief Locates and opens the LDROM bootloader device, then retrieves endpoints.
 * 
 * Similar to find_hid_device_and_endpoints_isp, but specifically targets
 * devices in LDROM (bootloader) mode.
 * 
 * @param handle Output pointer to store the opened device handle.
 * @param ep_out Output pointer to store the OUT endpoint address.
 * @param ep_in Output pointer to store the IN endpoint address.
 * @return int 0 on success, -1 on failure.
 */
int find_hid_device_and_endpoints_ldrom(libusb_device_handle** handle, uint8_t* ep_out, uint8_t* ep_in) {
    // Attempt to open device with LDROM VID/PID
    *handle = libusb_open_device_with_vid_pid(NULL, USB_VID_LD, USB_PID_LD);
    if (*handle == NULL) {
        fprintf(stderr, "NO FIND (Vendor ID: 0x%04x, Product ID: 0x%04x)\n", USB_VID_LD, USB_PID_LD);
        return -1;
    }

    // Detach kernel driver if attached (Linux requirement)
    libusb_detach_kernel_driver(*handle, INTERFACE_NUMBER);

    // Claim interface for exclusive access
    int r = libusb_claim_interface(*handle, INTERFACE_NUMBER);
    if (r < 0) {
        fprintf(stderr, "INTERFACE ERROR: %s\n", libusb_error_name(r));
        libusb_close(*handle);
        return -1;
    }

    // Retrieve configuration descriptor to find endpoints
    struct libusb_config_descriptor* config;
    libusb_get_active_config_descriptor(libusb_get_device(*handle), &config);
    const struct libusb_interface* inter = &config->interface[INTERFACE_NUMBER];
    const struct libusb_interface_descriptor* inter_desc = &inter->altsetting[0];

    // Initialize endpoint addresses
    *ep_out = 0;
    *ep_in = 0;

    // Find IN and OUT interrupt endpoints
    for (int i = 0; i < inter_desc->bNumEndpoints; i++) {
        const struct libusb_endpoint_descriptor* ep_desc = &inter_desc->endpoint[i];
        if ((ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT) {
            *ep_out = ep_desc->bEndpointAddress;
        }
        else if ((ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
            *ep_in = ep_desc->bEndpointAddress;
        }
    }

    // Free descriptor
    libusb_free_config_descriptor(config);

    // Validate endpoints were found
    if (*ep_out == 0 || *ep_in == 0) {
        fprintf(stderr, "Cann't find IN/OUT endpoint\n\r");
        libusb_release_interface(*handle, INTERFACE_NUMBER);
        libusb_close(*handle);
        return -1;
    }

    return 0;
}

/**
 * @brief Validates if a filename has a .bin extension.
 * 
 * Performs case-insensitive comparison of the file extension.
 * 
 * @param filename The filename string to check.
 * @return int 0 if file has .bin extension, -1 otherwise.
 */
int is_bin_file(const char* filename)
{
    // Validate input
    if (filename == NULL) {
        return -1;
    }

    size_t len = strlen(filename);

    // Check if length is sufficient to contain ".bin" (length 4)
    if (len < 4) {
        return -1;
    }

    // Case-insensitive comparison of extension
     if (_stricmp(&filename[len - 4], ".bin") == 0) {
         return 0; 
     }

    return -1; 
}

/**
 * @brief Cross-platform sleep function with sub-second precision.
 * 
 * Suspends execution for the specified duration. Uses platform-specific
 * implementations: Sleep() on Windows, nanosleep() on POSIX systems.
 * 
 * @param seconds Duration to sleep (can be fractional, e.g., 0.5 for 500ms).
 */
void sleep_seconds(double seconds) {
    if (seconds <= 0.0) {
        return;
    }

#ifdef _WIN32
    // On Windows platforms: Use Sleep() in milliseconds.
    Sleep((DWORD)(seconds * 1000));
#else
    // On POSIX platforms: Use nanosleep().
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
#endif
}

// === Main ISP Entry Point ===

/**
 * @brief Main ISP (In-System Programming) function to update MCU firmware.
 * 
 * This is the top-level function that orchestrates the entire firmware update process:
 * 1. Validates input and initializes variables
 * 2. Loads binary file into memory
 * 3. Opens USB connection to bootloader
 * 4. Performs handshake and device identification
 * 5. Programs APROM with firmware data
 * 6. Commands MCU to jump to new firmware
 * 7. Cleans up resources
 * 
 * The function uses a unified exit point (goto cleanup) to ensure USB resources
 * are always properly released regardless of success or failure.
 * 
 * @param filename Path to the binary firmware file (.bin).
 * @return int ISP_STATE code: RES_PASS on success, various error codes on failure.
 *         - RES_FILE_NO_FOUND: File not found or NULL parameter
 *         - RES_FILE_SIZE_OVER: File exceeds maximum size
 *         - RES_CONNECT_FALSE: USB connection failed
 *         - RES_SN_OK/RES_TIME_OUT: Handshake or communication error
 *         - RES_FALSE: Programming error
 * 
 * @note FAE Note:
 * Changed return value to int, mapping ISP_STATE out, so upper UI or Script can determine results.
 * Unified Single Exit Point to ensure USB resource release.
 */
int isp_mcu(char* filename) {
    ISP_STATE status = RES_FALSE;
    time_t pstart_time, pend_time;

    // === Phase 0: Parameter validation ===
    if (filename == NULL) {
        dbg_printf("Error: Filename is NULL\n\r");
        return RES_FILE_NO_FOUND;
    }

    // Brief delay to ensure device stability
    sleep_seconds(2);
    dbg_printf("--- ISP Start ---\n\r");
    pstart_time = time(NULL);

    // === Phase 1: Initialize global variables (PacketNumber, Address...) ===
    ISP_COMMAND_init();

    // === Phase 2: Load firmware file (Blocking Check) ===
    // Must check if status is RES_FILE_LOAD; if it fails, exit directly without closing USB (since it's not open yet)
    status = File_Open_APROM(filename);
    if (status != RES_FILE_LOAD) {
        dbg_printf("Error: Load bin file failed (Code: %d)\n\r", status);
        return status; // Return error code directly
    }
    dbg_printf("File loaded. Size: %d bytes\n\r", file_size);

    // === Phase 3: Open USB connection ===
    status = OPEN_USBPORT();
    if (status != RES_CONNECT) {
        dbg_printf("Error: USB Connection failed or Device not found (Code: %d)\n\r", status);
        // USB not opened successfully, no need to call CLOSE_USB_PORT
        return status;
    }

    // --- Enter Critical Section (Must ensure CLOSE_USB_PORT is called) ---

    // === Phase 4: Handshake & Device Identification ===
    // Check SN Package (Handshake)
    status = SN_PACKAGE_USB();
    if (status != RES_SN_OK) {
        dbg_printf("Error: SN Package handshake failed (Code: %d)\n\r", status);
        goto cleanup;
    }

    // Read Product ID
    status = READ_PID_USB();
    if (status != RES_PASS) {
        dbg_printf("Error: Read PID failed (Code: %d)\n\r", status);
        goto cleanup;
    }

    // === Phase 5: Perform APROM Programming ===
    // This is the most time-consuming operation; status will be updated by UPDATE_APROM_USB
    status = UPDATE_APROM_USB();
    if (status != RES_PASS) {
        dbg_printf("Error: APROM Update failed (Code: %d)\n\r", status);
        goto cleanup;
    }

    // === Phase 6: Programming complete, execute jump to APROM ===
    // Usually after the Jump command is sent, the MCU will Reset and USB will disconnect, so an ACK may not be received here
    RUN_TO_APROM_USB();
    dbg_printf("Command: Run to APROM sent.\n\r");

    // Mark final success state
    status = RES_PASS;

cleanup:
    // === Phase 7: Unified resource release ===
    CLOSE_USB_PORT();

    // Calculate and display total execution time
    pend_time = time(NULL);
    if (status == RES_PASS) {
        dbg_printf("ISP Success! Total time: %d sec\n\r", (int)(pend_time - pstart_time));
    }
    else {
        dbg_printf("ISP Failed! Total time: %d sec\n\r", (int)(pend_time - pstart_time));
    }

    return status;
}