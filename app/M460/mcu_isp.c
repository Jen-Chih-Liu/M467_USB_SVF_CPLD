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

libusb_device_handle* g_dev_handle = NULL;

#define Time_Out_Value 1000 // Overall operation timeout (ms)
uint8_t ENDPOINT_IN = 0;
uint8_t ENDPOINT_OUT = 0;
unsigned char buffer[full_Pkg_Size] = { 0 };
unsigned int PacketNumber;
unsigned int Address, Size;
unsigned char send_buf[full_Pkg_Size];
#define CMD_UPDATE_APROM      0x000000A0
unsigned char W_APROM_BUFFER[aprom_size_t];
unsigned int file_size;
unsigned int file_checksum;
unsigned int file_checksum_temp;
unsigned int USB_OPEN_FLAG;
unsigned int APROM_SIZE;



void WordsCpy(void* dest, void* src, unsigned int size)
{
    unsigned char* pu8Src, * pu8Dest;
    unsigned int i;

    pu8Dest = (unsigned char*)dest;
    pu8Src = (unsigned char*)src;

    for (i = 0; i < size; i++)
        pu8Dest[i] = pu8Src[i];
}


int find_hid_device_and_endpoints_isp(libusb_device_handle** handle, uint8_t* ep_out, uint8_t* ep_in) {
    *handle = libusb_open_device_with_vid_pid(NULL, USB_VID_LD, USB_PID_LD);
    if (*handle == NULL) {
        fprintf(stderr, "error find Vendor ID: 0x%04x, Product ID: 0x%04x\n\r", USB_VID_LD, USB_PID_LD);
        return -1;
    }

    // Detach kernel driver (if currently attached)
    libusb_detach_kernel_driver(*handle, INTERFACE_NUMBER);

    int r = libusb_claim_interface(*handle, INTERFACE_NUMBER);
    if (r < 0) {
        fprintf(stderr, "Can't claim usb: %s\n", libusb_error_name(r));
        libusb_close(*handle);
        return -1;
    }

    // Find endpoints
    struct libusb_config_descriptor* config;
    libusb_get_active_config_descriptor(libusb_get_device(*handle), &config);
    const struct libusb_interface* inter = &config->interface[INTERFACE_NUMBER];
    const struct libusb_interface_descriptor* inter_desc = &inter->altsetting[0];

    *ep_out = 0;
    *ep_in = 0;

    for (int i = 0; i < inter_desc->bNumEndpoints; i++) {
        const struct libusb_endpoint_descriptor* ep_desc = &inter_desc->endpoint[i];
        if ((ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT) {
            *ep_out = ep_desc->bEndpointAddress;
        }
        else if ((ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
            *ep_in = ep_desc->bEndpointAddress;
        }
    }

    libusb_free_config_descriptor(config);

    if (*ep_out == 0 || *ep_in == 0) {
        fprintf(stderr, "Can't find IN/OUT\n\r");
        libusb_release_interface(*handle, INTERFACE_NUMBER);
        libusb_close(*handle);
        return -1;
    }

    return 0;
}

ISP_STATE OPEN_USBPORT(void)
{
    if (g_dev_handle != NULL) {
        return RES_CONNECT;
    }
    int r = libusb_init(NULL);
    if (r < 0) {
        fprintf(stderr, "libusb init false, %s\n\r ", libusb_error_name(r));
        return RES_CONNECT_FALSE;
    }

    r = find_hid_device_and_endpoints_isp(&g_dev_handle, &ENDPOINT_OUT, &ENDPOINT_IN);
    if (r != 0) {
        libusb_exit(NULL);
        return RES_CONNECT_FALSE;
    }
    return RES_CONNECT;

}

ISP_STATE CLOSE_USB_PORT(void)
{
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

unsigned int READFW_VERSION_USB(void)
{
    int r;
    int transferred;
    unsigned char cmd[full_Pkg_Size] = { 0xa6,0,0,0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd, sizeof(cmd), &transferred, 2000);
    if (r != 0) {
        printf("Write error in READFW_VERSION_USB: %s\n", libusb_error_name(r));
        return 0;
    }

    clock_t start_time = clock();
    while ((clock() - start_time) < Time_Out_Value)
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 2000);
        if (r == 0 && transferred > 0) {
            dbg_printf("package: 0x%x\n\r", buffer[4]);
            if ((buffer[4] | ((buffer[5] << 8) & 0xff00) | ((buffer[6] << 16) & 0xff0000) | ((buffer[7] << 24) & 0xff000000)) == (PacketNumber + 1)) {
                PacketNumber += 2;
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

unsigned int CHECK_BOOT_USB(void)
{
    int r;
    int transferred;
    unsigned char cmd[full_Pkg_Size] = { 0xca,0,0,0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd, sizeof(cmd), &transferred, 2000);
    if (r != 0) {
        printf("Write error in CHECK_BOOT_USB: %s\n", libusb_error_name(r));
        return 0;
    }

    clock_t start_time = clock();
    while ((clock() - start_time) < Time_Out_Value)
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 200);
        if (r == 0 && transferred > 0) {
            dbg_printf("package: 0x%x\n\r", buffer[4]);
            if ((buffer[4] | ((buffer[5] << 8) & 0xff00) | ((buffer[6] << 16) & 0xff0000) | ((buffer[7] << 24) & 0xff000000)) == (PacketNumber + 1)) {
                PacketNumber += 2;
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

void RUN_TO_APROM_USB(void)
{
    int transferred;
    unsigned char cmd[full_Pkg_Size] = { 0xab,0,0,0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

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
ISP_STATE READ_PID_USB(void)
{
    int r;
    int transferred;
    unsigned char cmd[full_Pkg_Size] = { 0xB1,0,0,0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd, sizeof(cmd), &transferred, 2000);
    if (r != 0) {
        printf("Write error in READ_PID_USB: %s\n", libusb_error_name(r));
        return RES_TIME_OUT;
    }

    clock_t start_time = clock();
    while ((clock() - start_time) < Time_Out_Value)
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 2000);
        if (r == 0 && transferred > 0) {
            dbg_printf("package: 0x%x\n\r", buffer[4]);
            if ((buffer[4] | ((buffer[5] << 8) & 0xff00) | ((buffer[6] << 16) & 0xff0000) | ((buffer[7] << 24) & 0xff000000)) == (PacketNumber + 1))
            {
                PacketNumber += 2;
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

ISP_STATE SN_PACKAGE_USB(void)
{
    int r;
    int transferred=64;
    unsigned char cmd1[full_Pkg_Size] = { 0xa4,0,0,0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff),
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd1, full_Pkg_Size, &transferred, 2000);
    if (r != 0) {
        printf("Write error in SN_PACKAGE_USB: %s\n", libusb_error_name(r));
        return RES_TIME_OUT;
    }

    clock_t start_time = clock();
    while ((clock() - start_time) < Time_Out_Value)
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 2000);
        if (r == 0 && transferred > 0) {
            dbg_printf("package: 0x%x\n\r", buffer[4]);
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

void READ_CONFIG_USB(void)
{
    int r;
    int transferred;
    unsigned char cmd[full_Pkg_Size] = { 0xa2,0, 0, 0,
        (PacketNumber & 0xff),((PacketNumber >> 8) & 0xff),((PacketNumber >> 16) & 0xff),((PacketNumber >> 24) & 0xff) };

    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, cmd, sizeof(cmd), &transferred, 2000);
    if (r != 0) {
        dbg_printf("Write error in READ_CONFIG_USB: %s\n", libusb_error_name(r));
        return;
    }

    clock_t start_time = clock();
    while ((clock() - start_time) < Time_Out_Value)
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 200);
        if (r == 0 && transferred > 0) {
            dbg_printf("package: 0x%x\n\r", buffer[4]);
            if ((buffer[4] | ((buffer[5] << 8) & 0xff00) | ((buffer[6] << 16) & 0xff0000) | ((buffer[7] << 24) & 0xff000000)) == (PacketNumber + 1))
            {
                dbg_printf("config0: 0x%x\n\r", (buffer[8] | ((buffer[9] << 8) & 0xff00) | ((buffer[10] << 16) & 0xff0000) | ((buffer[11] << 24) & 0xff000000)));
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

ISP_STATE UPDATE_APROM_USB(void)
{
    int r;
    int transferred;
    unsigned short get_cksum;
    file_size = aprom_size_t;

    // --- First packet (Erase command) ---
    memset(send_buf, 0, full_Pkg_Size);
    unsigned long cmdData = CMD_UPDATE_APROM;
    unsigned long startaddr = 0;
    WordsCpy(send_buf + 0, &cmdData, 4);
    WordsCpy(send_buf + 4, &PacketNumber, 4);
    WordsCpy(send_buf + 8, &startaddr, 4);
    WordsCpy(send_buf + 12, &file_size, 4);
    WordsCpy(send_buf + 16, W_APROM_BUFFER + 0, 48);

    r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, send_buf, sizeof(send_buf), &transferred, 00);
    if (r != 0) {
        dbg_printf("Write error at start of UPDATE_APROM_USB: %s\n", libusb_error_name(r));
        return RES_FALSE;
    }

    BOOL ack_received = FALSE;
    clock_t start_time = clock();
    while ((clock() - start_time) < 10000) // Erase can take longer, use 10s timeout
    {
        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 500);
        if (r == 0 && transferred > 0) {
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


    int last_progress = -1;
    for (unsigned int i = 48; i < file_size; i = i + 56)
    {

        dbg_printf("Process=%.2f \r", (float)((float)i / (float)file_size) * 100);

        memset(send_buf, 0, 64);
        WordsCpy(send_buf + 4, &PacketNumber, 4);

        unsigned int chunk_size = (file_size - i > 56) ? 56 : (file_size - i);
        WordsCpy(send_buf + 8, W_APROM_BUFFER + i, chunk_size);

        r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_OUT, send_buf, sizeof(send_buf), &transferred, 2000);
        if (r != 0) {
            dbg_printf("Write error during APROM update: %s\n", libusb_error_name(r));
            goto out1;
        }

        ack_received = FALSE;
        start_time = clock();
        while ((clock() - start_time) < Time_Out_Value)
        {
            r = libusb_interrupt_transfer(g_dev_handle, ENDPOINT_IN, buffer, full_Pkg_Size, &transferred, 200);
            if (r == 0 && transferred > 0) {
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

        if (i + 56 >= file_size) { // Last packet
            WordsCpy(&get_cksum, buffer + 8, 2);
            dbg_printf("get_cksum: 0x%x\n\r", get_cksum);
        }

        PacketNumber = PacketNumber + 2;
    }

    dbg_printf("\r                                        ");
    dbg_printf("\r program progress: 100%% \n\r");

    return RES_PASS;

out1:
    dbg_printf("\r program false!! \n\r");
    return RES_FALSE;
}


ISP_STATE File_Open_APROM(const char* temp)
{
    FILE* fp;
    file_size = 0;
    for (unsigned int i = 0; i < aprom_size_t; i++)
    {
        W_APROM_BUFFER[i] = 0xff;
    }
    if ((fp = fopen(temp, "rb")) == NULL)
    {
        dbg_printf("APROM FILE OPEN FALSE\n\r");
        return RES_FILE_NO_FOUND;
    }
    if (fp != NULL)
    {
        // Fix for reading file size correctly
        fseek(fp, 0, SEEK_END);
        file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (file_size > aprom_size_t) {
            fclose(fp);
            return RES_FILE_SIZE_OVER;
        }
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

void APROM_AND_CHECKSUM(void)
{
    APROM_SIZE = aprom_size_t;
    W_APROM_BUFFER[(APROM_SIZE)-8] = ((aprom_size_t - 8) >> 0) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-7] = ((aprom_size_t - 8) >> 8) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-6] = ((aprom_size_t - 8) >> 16) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-5] = ((aprom_size_t - 8) >> 24) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-4] = (file_checksum >> 0) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-3] = (file_checksum >> 8) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-2] = (file_checksum >> 16) & 0xff;
    W_APROM_BUFFER[(APROM_SIZE)-1] = (file_checksum >> 24) & 0xff;
    file_checksum_temp = 0;
    for (unsigned int i = 0; i < APROM_SIZE; i++)
    {
        file_checksum_temp += W_APROM_BUFFER[i];
    }
    file_checksum_temp = file_checksum_temp & 0xffff;
    file_size = APROM_SIZE;
    file_checksum = file_checksum_temp; //total array checksum
}

void ISP_COMMAND_init(void)
{

    USB_OPEN_FLAG = 0;
    Address = 0;
    PacketNumber = 1;
    APROM_SIZE = 0;
}

int find_hid_device_and_endpoints_ldrom(libusb_device_handle** handle, uint8_t* ep_out, uint8_t* ep_in) {
    *handle = libusb_open_device_with_vid_pid(NULL, USB_VID_LD, USB_PID_LD);
    if (*handle == NULL) {
        fprintf(stderr, "NO FIND (Vendor ID: 0x%04x, Product ID: 0x%04x)\n", USB_VID_LD, USB_PID_LD);
        return -1;
    }

    // Detach kernel driver (if currently attached)
    libusb_detach_kernel_driver(*handle, INTERFACE_NUMBER);

    int r = libusb_claim_interface(*handle, INTERFACE_NUMBER);
    if (r < 0) {
        fprintf(stderr, "INTERFACE ERROR: %s\n", libusb_error_name(r));
        libusb_close(*handle);
        return -1;
    }

    // Find endpoints
    struct libusb_config_descriptor* config;
    libusb_get_active_config_descriptor(libusb_get_device(*handle), &config);
    const struct libusb_interface* inter = &config->interface[INTERFACE_NUMBER];
    const struct libusb_interface_descriptor* inter_desc = &inter->altsetting[0];

    *ep_out = 0;
    *ep_in = 0;

    for (int i = 0; i < inter_desc->bNumEndpoints; i++) {
        const struct libusb_endpoint_descriptor* ep_desc = &inter_desc->endpoint[i];
        if ((ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT) {
            *ep_out = ep_desc->bEndpointAddress;
        }
        else if ((ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
            *ep_in = ep_desc->bEndpointAddress;
        }
    }

    libusb_free_config_descriptor(config);

    if (*ep_out == 0 || *ep_in == 0) {
        fprintf(stderr, "Cann't find IN/OUT endpoint\n\r");
        libusb_release_interface(*handle, INTERFACE_NUMBER);
        libusb_close(*handle);
        return -1;
    }

    return 0;
}


int is_bin_file(const char* filename)
{
    if (filename == NULL) {
        return -1;
    }

    size_t len = strlen(filename);

    // Check if length is sufficient to contain ".bin" (length 4)
    if (len < 4) {
        return -1;
    }

    // Case-insensitive comparison, but .bin is usually lowercase
     if (_stricmp(&filename[len - 4], ".bin") == 0) {
         return 0; 
     }

    return -1; 
}

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

/* * FAE Note:
 * Change return value to int, mapping ISP_STATE out, so upper UI or Script can determine results.
 * Unified Single Exit Point to ensure USB resource release.
 */
int isp_mcu(char* filename) {
    ISP_STATE status = RES_FALSE;
    time_t pstart_time, pend_time;

    // 0. Parameter validation
    if (filename == NULL) {
        dbg_printf("Error: Filename is NULL\n\r");
        return RES_FILE_NO_FOUND;
    }

    sleep_seconds(2);
    dbg_printf("--- ISP Start ---\n\r");
    pstart_time = time(NULL);

    // 1. Initialize global variables (PacketNumber, Address...)
    ISP_COMMAND_init();

    // 2. Load file (Blocking Check)
    // Must check if status is RES_FILE_LOAD; if it fails, exit directly without closing USB (since it's not open yet)
    status = File_Open_APROM(filename);
    if (status != RES_FILE_LOAD) {
        dbg_printf("Error: Load bin file failed (Code: %d)\n\r", status);
        return status; // Return error code directly
    }
    dbg_printf("File loaded. Size: %d bytes\n\r", file_size);

    // 3. Open USB connection
    status = OPEN_USBPORT();
    if (status != RES_CONNECT) {
        dbg_printf("Error: USB Connection failed or Device not found (Code: %d)\n\r", status);
        // USB not opened successfully, no need to call CLOSE_USB_PORT
        return status;
    }

    // --- Enter Critical Section (Must ensure CLOSE_USB_PORT is called) ---

    // 4. Handshake & Check
    // Check SN Package
    status = SN_PACKAGE_USB();
    if (status != RES_SN_OK) {
        dbg_printf("Error: SN Package handshake failed (Code: %d)\n\r", status);
        goto cleanup;
    }

    // Read PID
    status = READ_PID_USB();
    if (status != RES_PASS) {
        dbg_printf("Error: Read PID failed (Code: %d)\n\r", status);
        goto cleanup;
    }

    // 5. Perform Programming
    // This is the most time-consuming operation; status will be updated by UPDATE_APROM_USB
    status = UPDATE_APROM_USB();
    if (status != RES_PASS) {
        dbg_printf("Error: APROM Update failed (Code: %d)\n\r", status);
        goto cleanup;
    }

    // 6. Programming complete, execute jump
    // Usually after the Jump command is sent, the MCU will Reset and USB will disconnect, so an ACK may not be received here
    RUN_TO_APROM_USB();
    dbg_printf("Command: Run to APROM sent.\n\r");

    // Mark final success state
    status = RES_PASS;

cleanup:
    // Unified resource release
    CLOSE_USB_PORT();

    pend_time = time(NULL);
    if (status == RES_PASS) {
        dbg_printf("ISP Success! Total time: %d sec\n\r", (int)(pend_time - pstart_time));
    }
    else {
        dbg_printf("ISP Failed! Total time: %d sec\n\r", (int)(pend_time - pstart_time));
    }

    return status;
}