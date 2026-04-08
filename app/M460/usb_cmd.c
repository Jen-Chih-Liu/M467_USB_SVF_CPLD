#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if defined(_WIN32) || defined(_WIN64)
#include "libusb.h"
#else
// On Linux, pkg-config will automatically add libusb-1.0 to the include path.
// For compatibility, you can use __has_include check (C++17) or manual path.
#include <libusb-1.0/libusb.h>
#endif
#include  "config.h"
#include "cJSON.h"

// Define a structure to store scanned device information.

// Global variables: Dynamic Map
/**
 * @brief Global array to store entries of detected USB devices.
 */
DeviceEntry MyDeviceMap[MAX_DEVICES];
DeviceEntry MyDeviceMap1[MAX_DEVICES];
/**
 * @brief Global counter for the number of devices currently in MyDeviceMap.
 */
int g_device_count = 0;
int g_device_count1 = 0;
// --- Comparison function for sorting (for qsort) ---
/**
 * @brief Compares two devices for sorting.
 * 
 * Rules: Sort by Bus first, then by Port Path if Bus is the same.
 * 
 * @param a Pointer to the first DeviceEntry.
 * @param b Pointer to the second DeviceEntry.
 * @return int Difference used for sorting.
 */
int compare_devices(const void* a, const void* b) {
    DeviceEntry* devA = (DeviceEntry*)a;
    DeviceEntry* devB = (DeviceEntry*)b;

    // 1. Compare Bus numbers
    if (devA->bus != devB->bus) {
        return devA->bus - devB->bus;
    }

    // 2. If Bus is the same, compare Port Paths
    // Compare based on the shorter path length
    int min_len = (devA->port_len < devB->port_len) ? devA->port_len : devB->port_len;
    for (int i = 0; i < min_len; i++) {
        if (devA->port_path[i] != devB->port_path[i]) {
            return devA->port_path[i] - devB->port_path[i];
        }
    }

    // 3. If prefixes are the same, shorter path comes first (e.g., 1.2 before 1.2.1)
    return devA->port_len - devB->port_len;
}

// --- Core function for scanning and updating the Map ---
/**
 * @brief Scans for target devices and updates the global device map.
 * 
 * Note: Ensure libusb context is initialized; dev_list lifecycle is managed by the caller.
 * 
 * @param ctx libusb context.
 * @param devs Array of libusb devices.
 * @param cnt Number of devices in the array.
 */
void scan_and_update_map(libusb_context* ctx, libusb_device** devs, ssize_t cnt) {
    // Reset global device count
    g_device_count = 0;

    // 1. Iterate and filter
    for (ssize_t i = 0; i < cnt; i++) {
        libusb_device* dev = devs[i];
        struct libusb_device_descriptor desc;

        // Fetch descriptor to check VID/PID
        if (libusb_get_device_descriptor(dev, &desc) < 0) continue;

        // Match against target Vendor and Product IDs
        if (desc.idVendor == TARGET_VID && desc.idProduct == TARGET_PID) {
            if (g_device_count >= MAX_DEVICES) break;

            // Fill in details for the map entry
            MyDeviceMap[g_device_count].device = dev;
            MyDeviceMap[g_device_count].bus = libusb_get_bus_number(dev);
            MyDeviceMap[g_device_count].port_len = libusb_get_port_numbers(dev, MyDeviceMap[g_device_count].port_path, 8);

            // Important: Need to increment reference count since we store it in the map.
            libusb_ref_device(dev);

            g_device_count++;
        }
    }

    // 2. Sort the detected devices for consistent indexing
    // This ensures MyDeviceMap[0] is always the first physical device.
    qsort(MyDeviceMap, g_device_count, sizeof(DeviceEntry), compare_devices);

    dbg_printf("updated usb table, find %d devices \n", g_device_count);
}

// --- Clear Map ---
/**
 * @brief Releases all device references in the map and resets the count.
 */
void clear_map(void) {
    for (int i = 0; i < g_device_count; i++) {
        // Release the libusb reference for each stored device
        libusb_unref_device(MyDeviceMap[i].device); 
    }
    g_device_count = 0;
}

#if 0
/**
 * @brief Legacy function to list Nuvoton devices using debug prints.
 */
int  list_nuvoton_devices(void)
{

    libusb_context* ctx = NULL;
    libusb_device** devs;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1;

    // Get all device list
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1;
    }

    // --- Core scan and sort ---
    scan_and_update_map(ctx, devs, cnt);

    // --- Now you can operate via index 0, 1, 2 ---
    dbg_printf("----------------------------------------\n");
    for (int i = 0; i < g_device_count; i++) {
        dbg_printf("[Logical ID: %d] -> Bus %d, Port Path: ", i, MyDeviceMap[i].bus);
        for (int j = 0; j < MyDeviceMap[i].port_len; j++) {
            dbg_printf("%d%s", MyDeviceMap[i].port_path[j], (j == MyDeviceMap[i].port_len - 1) ? "" : ".");
        }
        dbg_printf("\n");

        // You can perform operations here:
        // libusb_device_handle *handle;
        // libusb_open(MyDeviceMap[i].device, &handle);
        // ...
    }
    dbg_printf("----------------------------------------\n");

    // Clean up resources
    clear_map(); // Release our stored references
    libusb_free_device_list(devs, 1); // Free the device list
    libusb_exit(ctx);
    return 0;
}
#endif

/**
 * @brief Lists detected devices and outputs information in JSON format.
 * 
 * @return int 0 on success, 1 on failure.
 */
int list_nuvoton_devices(void)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    int r;

    // Initialize libusb
    r = libusb_init(&ctx);
    if (r < 0) return 1; // Return failure if init fails

    // Retrieve global device list
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; // Return failure if list fetch fails
    }

    // Scan and sort devices matching VID/PID
    scan_and_update_map(ctx, devs, cnt);

    // =========================================================
    // [NEW] Use cJSON to create output
    // =========================================================
    cJSON* root = cJSON_CreateObject();

    // 1. Add device count (USBCount)
    cJSON_AddNumberToObject(root, "USBCount", g_device_count);

    // 2. Iterate through sorted map, add detailed location info
    // Format example: "USB1": "Bus 3, Port 2.1"
    char key_buf[16];      // Stores "USB1", "USB2", etc.
    char path_buf[32];     // Stores port path string like "2.1.4"
    char val_buf[64];      // Stores the full Value string

    for (int i = 0; i < g_device_count; i++) {
        // A. Construct Port Path string (e.g., "1.2.3")
        path_buf[0] = '\0'; // Clear buffer
        for (int j = 0; j < MyDeviceMap[i].port_len; j++) {
            char temp[8];
            // Append "." if not the last number in the path
            snprintf(temp, sizeof(temp), "%d%s",
                MyDeviceMap[i].port_path[j],
                (j == MyDeviceMap[i].port_len - 1) ? "" : ".");
            strcat(path_buf, temp);
        }

        // B. Construct Key (USB0, USB1...)
        snprintf(key_buf, sizeof(key_buf), "USB%d", i);

        // C. Construct Value (Bus X, Port Y.Y)
        snprintf(val_buf, sizeof(val_buf), "Bus %d, Port %s", MyDeviceMap[i].bus, path_buf);

        // D. Add the entry to the JSON root object
        cJSON_AddStringToObject(root, key_buf, val_buf);
    }

    // 3. Convert JSON object to string and print
    char* out = cJSON_Print(root);
    json_printf("%s\n", out);

    // 4. Free JSON string and object memory
    free(out);
    cJSON_Delete(root);
    // =========================================================

    // Clean up libusb resources
    clear_map();
    libusb_free_device_list(devs, 1);
    libusb_exit(ctx);
    return 0; // Return success
}



/**
 * @brief Sends string data via HID Interrupt endpoint (padded to a multiple of PACKET_SIZE).
 * 
 * @param handle The device handle.
 * @param ep_out The OUT endpoint address.
 * @param data_string The string to send.
 * @return 0 on success, -1 on failure.
 */
#if 0
int send_string_hid_interrupt(libusb_device_handle* handle, uint8_t ep_out, const char* data_string) {
    size_t data_length = strlen(data_string);
    size_t padded_length = ((data_length / PACKET_SIZE) + 1) * PACKET_SIZE;
    if (data_length % PACKET_SIZE == 0) {
        padded_length = data_length;
    }
    if (data_length == 0)
        return 0; // Do not send empty strings

    unsigned char* padded_data = (unsigned char*)malloc(padded_length);
    if (padded_data == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return -1;
    }

    memcpy(padded_data, data_string, data_length);
    memset(padded_data + data_length, 0, padded_length - data_length);

    int actual_length;
    for (size_t i = 0; i < padded_length; i += PACKET_SIZE) {
        int r = libusb_interrupt_transfer(handle, ep_out, padded_data + i, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
        if (r < 0) {
            fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
            free(padded_data);
            return -1;
        }
    }

    free(padded_data);
    return 0;
}
#endif 

/**
 * @brief Reads the firmware version from the device.
 * 
 * [Modified] Function declaration: returns int (status), added uint32_t* out_version (output)
 * 
 * @param handle USB device handle.
 * @param ep_out OUT endpoint.
 * @param ep_in IN endpoint.
 * @param out_version Output pointer for the version.
 * @return int 0 on success, -1 on failure.
 */
int read_device_version(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in, uint32_t* out_version) {
    // Prepare and send the command
    int actual_length = 0;
    char cmd_version[PACKET_SIZE] = { 0 };
    cmd_version[0] = (char)0xb0; // read version command prefix


    // Perform interrupt transfer to send command
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_version, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return -1; // [Modified] Send failed -> return -1
    }

#if 0
    cmd_version[0] = 0xb0; // read version command

    if (send_string_hid_interrupt(handle, ep_out, cmd_version) != 0) {
        fprintf(stderr, "Failed to send read version command.\n");
        return -1; // [Modified] Send failed -> return -1
    }
#endif
    // Prepare the receive buffer
    unsigned char in_data[transfer_size] = { 0 };


    // Read the returned data from the device via interrupt transfer
    int r_in = libusb_interrupt_transfer(handle, ep_in, in_data, sizeof(in_data), &actual_length, 1000); // 1 second timeout

    if (r_in != 0) {
        fprintf(stderr, "Error reading version from device: %s\n", libusb_error_name(r_in));
        return -1; // [Modified] Read failed -> return -1
    }

    // Check if the returned data length is sufficient
    // Based on logic, indices 0~3 are needed, so at least 4 bytes must be read.
    if (actual_length < 4) {
        fprintf(stderr, "Version response was too short (%d bytes).\n", actual_length);
        return -1; // [Modified] Insufficient data -> return -1
    }

    // Convert the received 4 bytes (Big-Endian) to a uint32_t
    // [Modified] Pass the calculation result back through the pointer.
    if (out_version != NULL) {
        *out_version = (uint32_t)((in_data[0] << 24) | (in_data[1] << 16) | (in_data[2] << 8) | in_data[3]);
    }

    return 0; // [Modified] Pass -> return 0
}



// [Modified] Function declaration: returns int (status)
/**
 * @brief Sends a reset command to the target device.
 * 
 * @param handle USB device handle.
 * @param ep_out OUT endpoint.
 * @param ep_in IN endpoint (not used).
 * @return int 0 on success, -1 on failure.
 */
int set_device_reset(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in) {
    // Prepare and send the command

    char cmd_reset[PACKET_SIZE] = { 0 };
    cmd_reset[0] = (char)0xb2; // Reset command prefix
    cmd_reset[1] = 0x55;       // Magic byte 1
    cmd_reset[2] = (char)0xaa; // Magic byte 2
    int actual_length;
    // Execute interrupt transfer
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_reset, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return -1; // [Modified] Send failed -> return -1
    }

    return 0; // [Modified] Pass -> return 0
}


// [Modified] Function declaration: returns int (status), added uint8_t set_var
/**
 * @brief Sets a specific reset variable on the device.
 * 
 * @param handle USB handle.
 * @param ep_out OUT endpoint.
 * @param ep_in IN endpoint (not used).
 * @param set_var Byte value to set.
 * @return int 0 on success, -1 on failure.
 */
int set_device_reset_var(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in, uint8_t set_var) {
    // Prepare and send the command

    char cmd_reset[PACKET_SIZE] = { 0 };
    cmd_reset[0] = (char)0xb2; // Command group
    cmd_reset[1] = 0x5a;       // Sub-command: Set Variable
    cmd_reset[2] = set_var;    // Data byte
    int actual_length;
    // Perform transfer
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_reset, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return -1; // [Modified] Send failed -> return -1
    }

    return 0; // [Modified] Pass -> return 0
}

/**
 * @brief Retrieves the current value of the reset variable from the device.
 * 
 * @param handle USB handle.
 * @param ep_out OUT endpoint.
 * @param ep_in IN endpoint.
 * @param out_version Pointer to store the result.
 * @return int 0 on success, -1 on failure.
 */
int get_device_reset_var(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in, uint8_t* out_version) {
    // Prepare and send the command

    char cmd_reset[PACKET_SIZE] = { 0 };
    cmd_reset[0] = (char)0xb2; // Command group
    cmd_reset[1] = 0x6a;       // Sub-command: Get Variable

    int actual_length;
    // Send command
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_reset, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return -1; // [Modified] Send failed -> return -1
    }

    // Prepare the receive buffer
    unsigned char in_data[transfer_size] = { 0 };


    // Read the returned data from the device
    int r_in = libusb_interrupt_transfer(handle, ep_in, in_data, sizeof(in_data), &actual_length, 1000); // 1 second timeout

    if (r_in != 0) {
        fprintf(stderr, "Error reading version from device: %s\n", libusb_error_name(r_in));
        return -1; // [Modified] Read failed -> return -1
    }

    // Check if the returned data length is sufficient
    if (actual_length < 4) {
        fprintf(stderr, "Version response was too short (%d bytes).\n", actual_length);
        return -1; // [Modified] Insufficient data -> return -1
    }

    // Convert the received bytes
    if (out_version != NULL) {
        if (in_data[0] != (unsigned char)0xb2)
        {
            return -1;
        }
        *out_version =  in_data[1];
    }
    return 0; // [Modified] Pass -> return 0
}



/**
 * @brief Open a specific USB device and retrieve its Endpoints.
 * 
 * @param dev The specific device pointer obtained from the Map (Input).
 * @param handle Pointer to store the opened device handle (Output).
 * @param ep_out Pointer to store the OUT endpoint (Output).
 * @param ep_in Pointer to store the IN endpoint (Output).
 * @return 0 on success, -1 on failure.
 */
int open_specific_device_and_endpoints(libusb_device* dev, libusb_device_handle** handle, uint8_t* ep_out, uint8_t* ep_in) {
    int r;

    // 1. Change: Instead of searching by VID/PID, directly open the "specific device" passed in.
    r = libusb_open(dev, handle);
    if (r < 0) {
        fprintf(stderr, "Error opening device: %s\n", libusb_error_name(r));
        *handle = NULL;
        return -1;
    }

    // 2. Detach kernel driver (On Linux, if HID driver is already loaded, it must be detached).
    if (libusb_kernel_driver_active(*handle, INTERFACE_NUMBER) == 1) {
        r = libusb_detach_kernel_driver(*handle, INTERFACE_NUMBER);
        if (r < 0) {
            fprintf(stderr, "Failed to detach kernel driver: %s\n", libusb_error_name(r));
        }
    }

    // 3. Claim interface for exclusive use
    r = libusb_claim_interface(*handle, INTERFACE_NUMBER);
    if (r < 0) {
        fprintf(stderr, "Cannot claim interface: %s\n", libusb_error_name(r));
        libusb_close(*handle);
        *handle = NULL;
        return -1;
    }

    // 4. Find endpoints by iterating through the configuration descriptor.
    struct libusb_config_descriptor* config;
    r = libusb_get_active_config_descriptor(dev, &config);
    if (r < 0) {
        fprintf(stderr, "Failed to get config descriptor: %s\n", libusb_error_name(r));
        libusb_release_interface(*handle, INTERFACE_NUMBER);
        libusb_close(*handle);
        return -1;
    }

    const struct libusb_interface* inter = &config->interface[INTERFACE_NUMBER];
    const struct libusb_interface_descriptor* inter_desc = &inter->altsetting[0];

    *ep_out = 0;
    *ep_in = 0;

    // Iterate through endpoints to find IN and OUT addresses
    for (int i = 0; i < inter_desc->bNumEndpoints; i++) {
        const struct libusb_endpoint_descriptor* ep_desc = &inter_desc->endpoint[i];

        if ((ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT) {
            if (*ep_out == 0) *ep_out = ep_desc->bEndpointAddress; // The first encountered OUT endpoint
        }
        else if ((ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
            if (*ep_in == 0) *ep_in = ep_desc->bEndpointAddress; // The first encountered IN endpoint
        }
    }

    libusb_free_config_descriptor(config);

    // Ensure both endpoints were found
    if (*ep_out == 0 || *ep_in == 0) {
        fprintf(stderr, "Could not find IN/OUT interrupt endpoints\n");
        libusb_release_interface(*handle, INTERFACE_NUMBER);
        libusb_close(*handle);
        *handle = NULL; // Ensure no invalid handle is returned
        return -1;
    }

    return 0;
}

/**
 * @brief Converts raw sensor bytes to temperature in Celsius.
 * 
 * @param h_bytem Sensor High byte.
 * @param l_byte Sensor Low byte.
 * @return float Temperature in Celsius.
 */
float show_temperature(uint8_t h_bytem, uint8_t l_byte)
{
    float final_celsius;
    int16_t signed_temp_val;
    uint16_t raw_temp;
    raw_temp = ((uint16_t)h_bytem << 8) | l_byte;
    //  Mask Flags & Handle Sign
        // Bit 15, 14, 13 are flags (TCRIT, HIGH, LOW). Bit 12 is Sign. 
        // We only care about Bits 12 down to 0 (13 bits total).
    uint16_t temp_13bit = raw_temp & 0x1FFF;

    // Check Bit 12 for sign 
    if (temp_13bit & 0x1000) {
        // Negative temperature: Convert via manual 2's complement logic for 13-bit
        // Subtract 2^13 (8192) to get the negative integer value
        signed_temp_val = (int16_t)(temp_13bit - 8192);
    }
    else {
        // Positive temperature
        signed_temp_val = (int16_t)temp_13bit;
    }

    // Convert to Celsius using the resolution defined in config
    final_celsius = (float)signed_temp_val * TEMP_RESOLUTION;

    return final_celsius;
}


/**
 * @brief Outputs CPLD internal version and register info to standard output.
 * 
 * @param p_buf Buffer containing raw register data.
 */
void show_cpld_information(uint8_t* p_buf)
{

    printf("cpld version: 0x%x\n\r", p_buf[map_cpld_ver]);
    printf("cpld test version: 0x%x\n\r", p_buf[map_cpld_test_ver]);
    printf("cpld hdd amount: 0x%x\n\r", p_buf[map_cpld_hdd_amount]);
    // Loop through port status registers
    for (uint8_t loc = 0; loc < cpld_hdd_max_cnt; loc++)
    {
        printf("cpld port status register [0x%x]: 0x%x\n\r", cpld_hdd_port_status + loc, p_buf[cpld_hdd_port_status + loc]);
    }
    // Loop through status registers
    for (uint8_t loc = 0; loc < cpld_hdd_max_cnt; loc++)
    {
        printf("cpld status register [0x%x]: 0x%x\n\r", cpld_hdd_status + loc, p_buf[cpld_hdd_status + loc]);
    }
    // Loop through LED registers
    for (uint8_t loc = 0; loc < cpld_hdd_max_cnt; loc++)
    {
        printf("cpld led register [0x%x]: 0x%x\n\r", cpld_hdd_led + loc, p_buf[cpld_hdd_led + loc]);
    }

}


#if 0
/**
 * @brief Reads entire board information from the device.
 */
uint32_t read_board(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in) {
    // Prepare and send the command
    char cmd_version[2] = { 0 }; // Use an array and initialize it
    cmd_version[0] = 0xc0; // read board command
    if (send_string_hid_interrupt(handle, ep_out, cmd_version) != 0) {
        fprintf(stderr, "Failed to send read version command.\n");
        return -1; // Send failed
    }

    // Prepare the receive buffer
    unsigned char in_data[transfer_size] = { 0 };
    int actual_length = 0;

    // Read the returned data from the device
    int r_in = libusb_interrupt_transfer(handle, ep_in, in_data, sizeof(in_data), &actual_length, 1000); // 1 second timeout

    if (r_in != 0) {
        fprintf(stderr, "Error reading version from device: %s\n", libusb_error_name(r_in));
        return -1; // Read failed
    }

    // Check if the returned data length is sufficient
    if (actual_length < 1000) {
        fprintf(stderr, "Version response was too short (%d bytes).\n", actual_length);
        return -1; // Insufficient data
    }
#if 0
    printf("temperature sensor read %f celsius\n\r",
        show_temperature(in_data[map_tempersensor_high], in_data[map_tempersensor_low]));
    show_cpld_information(&in_data[0]);
    print_nvme_basic_management_info(&in_data[nvme_slot_0]);
    print_nvme_basic_management_info(&in_data[nvme_slot_1]);
    print_nvme_basic_management_info(&in_data[nvme_slot_2]);
    print_nvme_basic_management_info(&in_data[nvme_slot_3]);
    print_nvme_basic_management_info(&in_data[nvme_slot_4]);
    print_nvme_basic_management_info(&in_data[nvme_slot_5]);
#endif
    return 0; //pass
}
#endif


#if 0
void show_cpld_information(uint8_t* p_buf)
{

    printf("cpld version: 0x%x\n\r", p_buf[cpld_ver]);
    printf("cpld test version: 0x%x\n\r", p_buf[cpld_test_ver]);
    printf("cpld hdd amount: 0x%x\n\r", p_buf[cpld_hdd_amount]);

    for (uint8_t loc = 0; loc < cpld_hdd_max_cnt; loc++)
    {
        printf("cpld port status register [0x%x]: 0x%x\n\r", cpld_hdd_port_status + loc, p_buf[cpld_hdd_port_status + loc]);
    }

    for (uint8_t loc = 0; loc < cpld_hdd_max_cnt; loc++)
    {
        printf("cpld status register [0x%x]: 0x%x\n\r", cpld_hdd_status + loc, p_buf[cpld_hdd_status + loc]);
    }

    for (uint8_t loc = 0; loc < cpld_hdd_max_cnt; loc++)
    {
        printf("cpld led register [0x%x]: 0x%x\n\r", cpld_hdd_led + loc, p_buf[cpld_hdd_led + loc]);
    }

}
#endif 
//#define nvme_slot_0 0x100
//#define nvme_slot_1 0x120
//#define nvme_slot_2 0x140
//#define nvme_slot_3 0x160
//#define nvme_slot_4 0x180
//#define nvme_slot_5 0x1a0

/**
 * @brief Prints decoded NVMe Basic Management information.
 * 
 * @param data Data pointer to the NVMe status buffer.
 */
void print_nvme_basic_management_info(uint8_t* data)
{

    // --- Block 1: Status Data (Offset 00h - 07h) ---

    // Byte 0: Length of Status (Should be 6)
    uint8_t status_len = data[0];

    // If length is incorrect, stop processing
    if (status_len != 6)
        return;

    dumpall_printf("=== NVMe Basic Management Command Data Structure (32 Bytes) ===\n");

    dumpall_printf("Status Length: %d (Expected: 6)\n", status_len);

    // Byte 1: Status Flags (SFLGS)
    uint8_t sflgs = data[1];
    dumpall_printf("Status Flags (0x%02X):\n", sflgs);
    dumpall_printf("  - SMBus Arbitration: %s\n", (sflgs & 0x80) ? "No Contention" : "Contention/Reset");
    dumpall_printf("  - Powered Up:        %s\n", (sflgs & 0x40) ? "Not Ready (Powering Up)" : "Ready"); // Note: 1=Cannot process, 0=Ready
    dumpall_printf("  - Drive Functional:  %s\n", (sflgs & 0x20) ? "Functional" : "Failure");
    dumpall_printf("  - Reset Not Required:%s\n", (sflgs & 0x10) ? "Yes" : "Reset Required");
    dumpall_printf("  - PCIe Link Active:  %s\n", (sflgs & 0x08) ? "Active" : "Down");

    // Byte 2: SMART Warnings
    uint8_t smart_warn = data[2];
    dumpall_printf("SMART Critical Warnings (0=Warning, 1=OK):\n");
    dumpall_printf("  - Bits: 0x%02X\n", smart_warn);


    // Byte 3: Composite Temperature (CTemp)
    uint8_t temp_raw = data[3];
    dumpall_printf("Composite Temperature: ");

    if (temp_raw <= 0x7E)
    {
        dumpall_printf("%d C\n", temp_raw); // 0 to 126C
    }
    else if (temp_raw == 0x7F)
    {
        dumpall_printf(">= 127 C\n");       // 127C or higher
    }
    else if (temp_raw == 0x80)
    {
        dumpall_printf("No Data / Old Data\n");
    }
    else if (temp_raw == 0x81)
    {
        dumpall_printf("Sensor Failure\n");
    }
    else if (temp_raw >= (unsigned char)0xC4)
    {
        // 0xC5-0xFF represents -1 to -59C (Twos complement)
        // Cast to int8_t directly works for 2's complement logic in C
        dumpall_printf("%d C\n", (int8_t)temp_raw);
    }
    else
    {
        dumpall_printf("Reserved (0x%02X)\n", temp_raw);
    }

    // Byte 4: Percentage Drive Life Used (PDLU)
    uint8_t life_used = data[4];
    dumpall_printf("Drive Life Used: %d%%\n", (life_used == 255) ? 255 : life_used);

    // Byte 5-6: Reserved
    // Byte 7: PEC for Status Block
    dumpall_printf("PEC (Status Block): 0x%02X\n", data[7]);

    dumpall_printf("---------------------------------------------------\n");

    // --- Block 2: Identification Data (Offset 08h - 1Fh) ---

    // Byte 8: Length of Identification (Should be 22)
    uint8_t id_len = data[8];
    dumpall_printf("ID Length: %d (Expected: 22)\n", id_len);

    // Byte 9-10: Vendor ID (VID) - MSB first
    uint16_t vid = (data[9] << 8) | data[10];
    dumpall_printf("Vendor ID: 0x%04X\n", vid);

    // Byte 11-30: Serial Number (20 chars)
    // First character is transmitted first
    char serial[21];

    for (int i = 0; i < 20; i++)
    {
        serial[i] = (char)data[11 + i];
    }

    serial[20] = '\0'; // Null-terminate for printing
    dumpall_printf("Serial Number: %s\n", serial);

    // Byte 31: PEC for ID Block
    dumpall_printf("PEC (ID Block): 0x%02X\n", data[31]);
}




/**
 * @brief Reads specific board information into the M463_BoardData_t structure.
 * 
 * @param handle libusb handle.
 * @param ep_out OUT endpoint.
 * @param ep_in IN endpoint.
 * @param board_dat Data structure to populate.
 * @return uint32_t 0 on success, -1 on failure.
 */
uint32_t read_board(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in, M463_BoardData_t* board_dat) {
    // Prepare and send the command
    char cmd_board[PACKET_SIZE] = { 0 };
    cmd_board[0] = (char)0xc0; // read board command prefix

    int actual_length = 0;;
    // Send command
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_board, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return (uint32_t)-1; // [Modified] Send failed -> return -1
    }

    // Prepare the receive buffer
    unsigned char in_data[transfer_size] = { 0 };

     actual_length = 0;;
    // Read the returned data from the device
    int r_in = libusb_interrupt_transfer(handle, ep_in, in_data, sizeof(in_data), &actual_length, 1000); // 1 second timeout

    if (r_in != 0) {
        fprintf(stderr, "Error reading version from device: %s\n", libusb_error_name(r_in));
        return (uint32_t)-1; // Read failed
    }

    // Check if the returned data length is sufficient
    if (actual_length < 1000) {
        fprintf(stderr, "Version response was too short (%d bytes).\n", actual_length);
        return (uint32_t)-1; // Insufficient data
    }

    // Map received data to structure members
    board_dat->temp_sensor_high = in_data[map_tempersensor_high];
    board_dat->temp_sensor_low = in_data[map_tempersensor_low];
    board_dat->cpld_version = in_data[map_cpld_ver];
    board_dat->cpld_test_ver = in_data[map_cpld_test_ver];
    board_dat->fan_duty= in_data[map_fan_duty];
    // RPM is calculated from two bytes
    board_dat->fan_rpm = (uint16_t)(in_data[map_fan_rpm_high]<<5| (in_data[map_fan_rpm_low]&0x1f));
    board_dat->hdd_amount = in_data[map_cpld_hdd_amount];

    // Read JTAG ID
    board_dat->jtag_id= (uint32_t)(in_data[cpld_chip_jtag_id]<<24| in_data[cpld_chip_jtag_id+1] << 16
        |in_data[cpld_chip_jtag_id+2] << 8 | in_data[cpld_chip_jtag_id + 3]);
    
    // Copy register ranges
    memcpy(board_dat->cpld_reg_40_4f, &in_data[0x40], 16);
    memcpy(board_dat->cpld_reg_50_5f, &in_data[0x50], 16);
    memcpy(board_dat->cpld_reg_60_6f, &in_data[0x60], 16);


    // Copy NVMe slot information
    memcpy(board_dat->nvme_slot_0, &in_data[0x100], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_1, &in_data[0x120], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_2, &in_data[0x140], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_3, &in_data[0x160], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_4, &in_data[0x180], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_5, &in_data[0x1A0], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_6, &in_data[0x1C0], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_7, &in_data[0x1E0], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_8, &in_data[0x200], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_9, &in_data[0x220], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_10, &in_data[0x240], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_11, &in_data[0x260], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_12, &in_data[0x280], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_13, &in_data[0x2A0], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_14, &in_data[0x2C0], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_15, &in_data[0x2E0], NVME_INFO_LEN);

    return 0; // Success
}


uint32_t read_board_cpld1(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in, M463_BoardData_t* board_dat) {
    // Prepare and send the command
    char cmd_board[PACKET_SIZE] = { 0 };
    cmd_board[0] = (char)0xc4; // read board command prefix

    int actual_length = 0;;
    // Send command
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_board, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return (uint32_t)-1; // [Modified] Send failed -> return -1
    }

    // Prepare the receive buffer
    unsigned char in_data[transfer_size] = { 0 };

     actual_length = 0;;
    // Read the returned data from the device
    int r_in = libusb_interrupt_transfer(handle, ep_in, in_data, sizeof(in_data), &actual_length, 1000); // 1 second timeout

    if (r_in != 0) {
        fprintf(stderr, "Error reading version from device: %s\n", libusb_error_name(r_in));
        return (uint32_t)-1; // Read failed
    }

    // Check if the returned data length is sufficient
    if (actual_length < 1000) {
        fprintf(stderr, "Version response was too short (%d bytes).\n", actual_length);
        return (uint32_t)-1; // Insufficient data
    }

    // Map received data to structure members
    board_dat->temp_sensor_high = in_data[map_tempersensor_high];
    board_dat->temp_sensor_low = in_data[map_tempersensor_low];
    board_dat->cpld_version = in_data[map_cpld_ver];
    board_dat->cpld_test_ver = in_data[map_cpld_test_ver];
    board_dat->fan_duty= in_data[map_fan_duty];
    // RPM is calculated from two bytes
    board_dat->fan_rpm = (uint16_t)(in_data[map_fan_rpm_high]<<5| (in_data[map_fan_rpm_low]&0x1f));
    board_dat->hdd_amount = in_data[map_cpld_hdd_amount];

    // Read JTAG ID
    board_dat->jtag_id= (uint32_t)(in_data[cpld_chip_jtag_id]<<24| in_data[cpld_chip_jtag_id+1] << 16
        |in_data[cpld_chip_jtag_id+2] << 8 | in_data[cpld_chip_jtag_id + 3]);
    
    // Copy register ranges
    memcpy(board_dat->cpld_reg_40_4f, &in_data[0x40], 16);
    memcpy(board_dat->cpld_reg_50_5f, &in_data[0x50], 16);
    memcpy(board_dat->cpld_reg_60_6f, &in_data[0x60], 16);


    // Copy NVMe slot information
    memcpy(board_dat->nvme_slot_0, &in_data[0x100], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_1, &in_data[0x120], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_2, &in_data[0x140], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_3, &in_data[0x160], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_4, &in_data[0x180], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_5, &in_data[0x1A0], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_6, &in_data[0x1C0], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_7, &in_data[0x1E0], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_8, &in_data[0x200], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_9, &in_data[0x220], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_10, &in_data[0x240], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_11, &in_data[0x260], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_12, &in_data[0x280], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_13, &in_data[0x2A0], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_14, &in_data[0x2C0], NVME_INFO_LEN);
    memcpy(board_dat->nvme_slot_15, &in_data[0x2E0], NVME_INFO_LEN);

    return 0; // Success
}



/**
 * @brief Reads BMC data from a specific USB logical device index.
 * 
 * @param usb_cnt Logical device index.
 * @param pBoardData Pointer to result structure.
 * @return int 0 on success, 1 on failure.
 */
int usb_read_multi_bmc(unsigned char usb_cnt, M463_BoardData_t* pBoardData)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    // Init and fetch list
    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    // Refresh internal device map
    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);
    if (g_device_count == 0)
    {
         libusb_exit(ctx);
        return 1;
    }
    r = -1;
    // Check if logical index is valid
    if (g_device_count > usb_cnt)
    {       
        // Open the specific device and discover endpoints
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            // Perform actual board data read
            r = (int)read_board(handle, ep_out, ep_in, pBoardData);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }


    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}


int usb_read_multi_bmc_cpld1(unsigned char usb_cnt, M463_BoardData_t* pBoardData)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    // Init and fetch list
    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    // Refresh internal device map
    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);
    if (g_device_count == 0)
        {
             libusb_exit(ctx);
        return 1;
        }
    r = -1;
    // Check if logical index is valid
    if (g_device_count > usb_cnt)
    {       
        // Open the specific device and discover endpoints
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            // Perform actual board data read
            r = (int)read_board_cpld1(handle, ep_out, ep_in, pBoardData);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }


    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}


/**
 * @brief Reads MCU version from a specific USB logical device index.
 * 
 * @param usb_cnt Logical device index.
 * @param pMCUVERSIONData Output pointer for version.
 * @return int 0 on success, 1 on failure.
 */
int usb_read_multi_mcu_version(unsigned char usb_cnt, unsigned int* pMCUVERSIONData)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            // Perform MCU version read
            r = read_device_version(handle, ep_out, ep_in, (uint32_t*)pMCUVERSIONData);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }

    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}


/**
 * @brief Sends a reset command to a specific USB device.
 * 
 * @param usb_cnt Logical device index.
 * @return int 0 on success, 1 on failure.
 */
int usb_multi_mcu_reset(unsigned char usb_cnt)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            // Execute reset
            r = set_device_reset(handle, ep_out, ep_in);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }


    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}


/**
 * @brief Sets a reset variable on a specific USB device.
 * 
 * @param usb_cnt Logical device index.
 * @param reset_var Variable byte to set.
 * @return int 0 on success, 1 on failure.
 */
int usb_multi_mcu_reset_set_var(unsigned char usb_cnt, unsigned char reset_var)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            // Set the variable
            r = set_device_reset_var(handle, ep_out, ep_in, reset_var);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }


    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}

/**
 * @brief Gets the reset variable from a specific USB device.
 * 
 * @param usb_cnt Logical index.
 * @param pReset_var Output pointer.
 * @return int 0 on success, 1 on failure.
 */
int usb_multi_mcu_reset_get_var(unsigned char usb_cnt, unsigned char* pReset_var)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            // Retrieve variable
            r = get_device_reset_var(handle, ep_out, ep_in, pReset_var);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }

    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}



/**
 * @brief Internal wrapper for CPLD I2C Read.
 */
int cpld_i2c_get(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in, unsigned char i2c_reg, unsigned char i2c_len, unsigned char* pi2c_array) {
    // Prepare and send the command
    char cmd_i2c_get[PACKET_SIZE] = { 0 };
    cmd_i2c_get[0] = (char)0xd4; // I2C Bypass command
    cmd_i2c_get[1] = 0;          // CPLD IN I2C0
    cmd_i2c_get[2] = cpld_adr;   // Slave address
    cmd_i2c_get[3] = 1;          // Operation: Read
    cmd_i2c_get[4] = i2c_len;
    cmd_i2c_get[5] = i2c_reg;
    int actual_length = 0;
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_i2c_get, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return -1; 
    }


    // Receive response
    unsigned char in_data[transfer_size] = { 0 };
    actual_length = 0;

    int r_in = libusb_interrupt_transfer(handle, ep_in, in_data, sizeof(in_data), &actual_length, 1000); // 1 second timeout

    if (r_in != 0) {
        fprintf(stderr, "Error reading from device: %s\n", libusb_error_name(r_in));
        return -1; 
    }

    if (actual_length < 4) {
        fprintf(stderr, "Response too short\n");
        return -1; 
    }
    // Extract returned I2C data (skipping protocol headers)
    for (int i = 0; i < i2c_len; i++)
        pi2c_array[i] = in_data[3 + i];

    return 0; 
}


/**
 * @brief Performs CPLD I2C Read on a specific USB logical device index.
 * 
 * @param usb_cnt Logical index.
 * @param i2c_reg Register to read.
 * @param i2c_len Number of bytes.
 * @param i2c_array Output data buffer.
 * @return int 0 on success, 1 on failure.
 */
int usbd_multi_cpld_i2c_get(unsigned char usb_cnt, unsigned char i2c_reg, unsigned char i2c_len, unsigned char *i2c_array)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            r = cpld_i2c_get(handle, ep_out, ep_in, i2c_reg, i2c_len,i2c_array);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }
  

    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}


/**
 * @brief Internal wrapper for CPLD I2C Write.
 */
int cpld_i2c_set(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in, unsigned char i2c_reg, unsigned char i2c_data) {
    // Prepare and send the command
    char cmd_i2c_set[PACKET_SIZE] = { 0 };
    cmd_i2c_set[0] = (char)0xd0; // I2C write command
    cmd_i2c_set[1] = 0;          // CPLD IN I2C0
    cmd_i2c_set[2] = cpld_adr;
    cmd_i2c_set[3] = 0x02;       // Operation: Write
    cmd_i2c_set[4] = i2c_reg;
    cmd_i2c_set[5] = i2c_data;
    int actual_length;
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_i2c_set, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return -1; 
    }
    return 0; 
}


/**
 * @brief Performs CPLD I2C Write on a specific USB logical device index.
 */
int usbd_multi_cpld_i2c_set(unsigned char usb_cnt, unsigned char i2c_reg, unsigned char i2c_data)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            r = cpld_i2c_set(handle, ep_out, ep_in, i2c_reg, i2c_data);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }

    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}


/**
 * @brief General I2C Read bypass for any device address.
 */
int pass_i2c_get(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in, unsigned char i2c_addr,unsigned char i2c_reg, unsigned char i2c_len, unsigned char* pi2c_array) {
    char cmd_i2c_get[PACKET_SIZE] = { 0 };
    cmd_i2c_get[0] = (char)0xd4; 
    cmd_i2c_get[1] = 0;          // Channel 0
    cmd_i2c_get[2] = i2c_addr;   // Custom slave address
    cmd_i2c_get[3] = 1;          // Read
    cmd_i2c_get[4] = i2c_len;
    cmd_i2c_get[5] = i2c_reg;
    int actual_length = 0;
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_i2c_get, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return -1; 
    }


    // Prepare response buffer
    unsigned char in_data[transfer_size] = { 0 };
    actual_length = 0;

    int r_in = libusb_interrupt_transfer(handle, ep_in, in_data, sizeof(in_data), &actual_length, 1000); 

    if (r_in != 0) {
        fprintf(stderr, "Error reading from device: %s\n", libusb_error_name(r_in));
        return -1; 
    }

    if (actual_length < 4) {
        fprintf(stderr, "Response too short\n");
        return -1; 
    }
    for (int i = 0; i < i2c_len; i++)
        pi2c_array[i] = in_data[3 + i];

    return 0; 
}


/**
 * @brief Reads global LED status via an 0xB3 command.
 */
int pass_GLOBAL_LED_get(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in, unsigned char* pGLOBAL_array) {
    // Prepare and send the command

    char cmd_GLOBALLED[PACKET_SIZE] = { 0 };
    cmd_GLOBALLED[0] = (char)0xB3; // read GOBAL LED command prefix
    int actual_length = 0;
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_GLOBALLED, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return -1; 
    }


    // Prepare receive buffer
    unsigned char in_data[transfer_size] = { 0 };
    actual_length = 0;

    int r_in = libusb_interrupt_transfer(handle, ep_in, in_data, sizeof(in_data), &actual_length, 1000); 

    if (r_in != 0) {
        fprintf(stderr, "Error reading from device: %s\n", libusb_error_name(r_in));
        return -1; 
    }

    if (actual_length < 4) {
        fprintf(stderr, "Response too short\n");
        return -1; 
    }
   
    // Store returned bytes
    pGLOBAL_array[0] = in_data[1];
    pGLOBAL_array[1] = in_data[2];

    return 0; 
}


/**
 * @brief General bypass I2C Read logical wrapper.
 */
int usbd_multi_pass_i2c_get(unsigned char usb_cnt, unsigned char i2c_addr, unsigned char i2c_reg, unsigned char i2c_len, unsigned char* i2c_array)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            r = pass_i2c_get(handle, ep_out, ep_in, i2c_addr,i2c_reg, i2c_len, i2c_array);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }


    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}



/**
 * @brief Global LED Status logical wrapper.
 */
int usbd_multi_pass_GLOBAL_get(unsigned char usb_cnt,unsigned char* LED_array)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            r = pass_GLOBAL_LED_get(handle, ep_out, ep_in, LED_array);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }


    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}

/**
 * @brief Internal wrapper for general I2C Write bypass.
 */
int pass_i2c_set(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in,unsigned char i2c_adddr, unsigned char i2c_reg, unsigned char i2c_data) {
    // Prepare and send the command
    char cmd_i2c_set[PACKET_SIZE] = { 0 };
    cmd_i2c_set[0] = (char)0xd0; 
    cmd_i2c_set[1] = 0;          // Channel 0
    cmd_i2c_set[2] = i2c_adddr;
    cmd_i2c_set[3] = 0x02;       // Operation: Write
    cmd_i2c_set[4] = i2c_reg;
    cmd_i2c_set[5] = i2c_data;
    int actual_length;
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_i2c_set, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return -1; 
    }
    return 0; 
}


/**
 * @brief General bypass I2C Write logical wrapper.
 */
int usbd_multi_pass_i2c_set(unsigned char usb_cnt, unsigned char i2c_addr, unsigned char i2c_reg, unsigned char i2c_data)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            r = pass_i2c_set(handle, ep_out, ep_in, i2c_addr, i2c_reg, i2c_data);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }

    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}



/**
 * @brief Commands the MCU to jump into LDROM mode for ISP.
 */
int usbd_multi_mcu_jumper_ldrom(unsigned char usb_cnt)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            int actual_length = 0;
            char cmd_jmper_ldrom[PACKET_SIZE] = { 0 };
            cmd_jmper_ldrom[0] = (char)0xb1; // jumper to ldrom command prefix
            cmd_jmper_ldrom[1] = 0x5a;
            cmd_jmper_ldrom[2] = (char)0xa5;
            r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_jmper_ldrom, PACKET_SIZE, &actual_length, 0); 
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }
    

    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}


/**
 * @brief Sends an 0xDA 0x0 command to stop BMC monitoring logic.
 */
int usbd_multi_mcu_stop_bmc_monitor(unsigned char usb_cnt)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            int actual_length = 0;
            char cmd_i2c[PACKET_SIZE] = { 0 };
            cmd_i2c[0] = (char)0xda; // Monitoring control command prefix
            cmd_i2c[1] = 0x0;        // Stop action
            r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_i2c, PACKET_SIZE, &actual_length, 0); 
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }
    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}

/**
 * @brief Sends an 0xDA 0x1 command to start BMC monitoring logic.
 */
int usbd_multi_mcu_start_bmc_monitor(unsigned char usb_cnt)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            int actual_length = 0;
            char cmd_i2c[PACKET_SIZE] = { 0 };
            cmd_i2c[0] = (char)0xda; // Monitoring control
            cmd_i2c[1] = 0x01;       // Start action
            r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_i2c, PACKET_SIZE, &actual_length, 0); 
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }


    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}



/**
 * @brief Writes data to MCU's EEPROM from a binary file.
 * 
 * This function reads up to 256 bytes from a specified file and writes it to
 * the target MCU's EEPROM using USB HID command 0xC1. The buffer is pre-filled
 * with 0xFF, and only the bytes read from the file will overwrite these values.
 * 
 * @param usb_cnt Logical device index (0-based).
 * @param filename Path to the binary file to be written to EEPROM.
 * @return int 0 on success, 1 on failure, RES_FILE_NO_FOUND if file not found,
 *         RES_FILE_SIZE_OVER if file exceeds 256 bytes.
 */
int usbd_multi_mcu_eeprom_write(unsigned char usb_cnt, char* filename )
{
    unsigned char  W_EEPROM_BUFFER[256];
unsigned int loc_file_size;
    FILE* fp;
    
    // Initialize file size counter
    loc_file_size = 0;
    
    // Pre-fill EEPROM buffer with 0xFF (default erased state)
    for (unsigned int i = 0; i < 256; i++)
    {
        W_EEPROM_BUFFER[i] = 0xff;
    }
    
    // Attempt to open the binary file for reading
    if ((fp = fopen(filename, "rb")) == NULL)
    {
        dbg_printf("APROM FILE OPEN FALSE\n\r");
        return RES_FILE_NO_FOUND;
    }
    
    // Read file content into buffer
    if (fp != NULL)
    {
        // Fix for reading file size correctly
        fseek(fp, 0, SEEK_END);
        loc_file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        // Validate file size doesn't exceed EEPROM capacity
        if (loc_file_size > 256) {
            fclose(fp);
            return RES_FILE_SIZE_OVER;
        }
        
        // Read file data into buffer
        fread(W_EEPROM_BUFFER, 1, loc_file_size, fp);
        fclose(fp);
    }

    // === USB Communication Setup ===
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    // Initialize libusb library
    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    // Retrieve global device list
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    // Scan and update internal device map
    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

   // Check if any devices were found
   if (g_device_count == 0) {
        return 1;
    }
    
    r = -1;
    
    // Verify the requested device index is valid
    if (g_device_count > usb_cnt)
    {
        // Open the specific device and retrieve endpoints
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            int actual_length = 0;
            char cmd_eeprom_write[PACKET_SIZE] = { 0 };
            
            // Prepare command packet: Copy EEPROM data starting at byte[1]
            for (int i = 0; i < 256; i++)
            {
                cmd_eeprom_write[1 + i] = W_EEPROM_BUFFER[i];
            }
            
            // Set command byte for EEPROM write operation
            cmd_eeprom_write[0] = (char)0xc1; // EEPROM write command prefix

            // Execute USB interrupt transfer to send EEPROM write command
            r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_eeprom_write, PACKET_SIZE, &actual_length, 0); 

            // Send 0xC2 to read back EEPROM content and compare with write buffer
            if (r == 0)
            {
                char cmd_eeprom_read[PACKET_SIZE] = { 0 };
                unsigned char eeprom_readback[transfer_size] = { 0 };

                cmd_eeprom_read[0] = (char)0xc2;
                actual_length = 0;
                r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_eeprom_read, PACKET_SIZE, &actual_length, 0);

                if (r == 0)
                {
                    actual_length = 0;
                    r = libusb_interrupt_transfer(handle, ep_in, eeprom_readback, sizeof(eeprom_readback), &actual_length, 1000);

                    if (r == 0)
                    {
                        if (actual_length < 257 || eeprom_readback[0] != (unsigned char)0xc2)
                        {
                            printf("EEPROM readback response invalid, len=%d, cmd=0x%02x\n", actual_length, eeprom_readback[0]);
                            r = -1;
                        }
                        else if (memcmp(W_EEPROM_BUFFER, &eeprom_readback[1], 256) != 0)
                        {
                            printf("EEPROM verify failed after write\n");
                            r = -1;
                        }
                    }
                }
            }

            
            // Release interface and close device handle
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }
    
    // Clean up resources
    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}


int usbd_multi_mcu_eeprom_write_FRU1(unsigned char usb_cnt, char* filename )
{
    unsigned char  W_EEPROM_BUFFER[256];
unsigned int loc_file_size;
    FILE* fp;
    
    // Initialize file size counter
    loc_file_size = 0;
    
    // Pre-fill EEPROM buffer with 0xFF (default erased state)
    for (unsigned int i = 0; i < 256; i++)
    {
        W_EEPROM_BUFFER[i] = 0xff;
    }
    
    // Attempt to open the binary file for reading
    if ((fp = fopen(filename, "rb")) == NULL)
    {
        dbg_printf("APROM FILE OPEN FALSE\n\r");
        return RES_FILE_NO_FOUND;
    }
    
    // Read file content into buffer
    if (fp != NULL)
    {
        // Fix for reading file size correctly
        fseek(fp, 0, SEEK_END);
        loc_file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        // Validate file size doesn't exceed EEPROM capacity
        if (loc_file_size > 256) {
            fclose(fp);
            return RES_FILE_SIZE_OVER;
        }
        
        // Read file data into buffer
        fread(W_EEPROM_BUFFER, 1, loc_file_size, fp);
        fclose(fp);
    }

    // === USB Communication Setup ===
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    // Initialize libusb library
    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    // Retrieve global device list
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    // Scan and update internal device map
    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

   // Check if any devices were found
   if (g_device_count == 0) {
        return 1;
    }
    
    r = -1;
    
    // Verify the requested device index is valid
    if (g_device_count > usb_cnt)
    {
        // Open the specific device and retrieve endpoints
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            int actual_length = 0;
            char cmd_eeprom_write[PACKET_SIZE] = { 0 };
            
            // Prepare command packet: Copy EEPROM data starting at byte[1]
            for (int i = 0; i < 256; i++)
            {
                cmd_eeprom_write[1 + i] = W_EEPROM_BUFFER[i];
            }
            
            // Set command byte for EEPROM write operation
            cmd_eeprom_write[0] = (char)0xc5; // EEPROM write command prefix

            // Execute USB interrupt transfer to send EEPROM write command
            r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_eeprom_write, PACKET_SIZE, &actual_length, 0); 

            // Send 0xC2 to read back EEPROM content and compare with write buffer
            if (r == 0)
            {
                char cmd_eeprom_read[PACKET_SIZE] = { 0 };
                unsigned char eeprom_readback[transfer_size] = { 0 };

                cmd_eeprom_read[0] = (char)0xc6;
                actual_length = 0;
                r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_eeprom_read, PACKET_SIZE, &actual_length, 0);

                if (r == 0)
                {
                    actual_length = 0;
                    r = libusb_interrupt_transfer(handle, ep_in, eeprom_readback, sizeof(eeprom_readback), &actual_length, 1000);

                    if (r == 0)
                    {
                        if (actual_length < 257 || eeprom_readback[0] != (unsigned char)0xc2)
                        {
                            printf("EEPROM readback response invalid, len=%d, cmd=0x%02x\n", actual_length, eeprom_readback[0]);
                            r = -1;
                        }
                        else if (memcmp(W_EEPROM_BUFFER, &eeprom_readback[1], 256) != 0)
                        {
                            printf("EEPROM verify failed after write\n");
                            r = -1;
                        }
                    }
                }
            }

            
            // Release interface and close device handle
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }
    
    // Clean up resources
    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}

/**
 * @brief Sends an 0xDC command to control a specific GPIO on the MCU.
 */
int usbd_multi_MCU_GPIO_SET(unsigned char usb_cnt, unsigned char gpio_num, unsigned char gpio_val)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            int actual_length = 0;
            char cmd_i2c[PACKET_SIZE] = { 0 };
            cmd_i2c[0] = (char)0xdc; // GPIO control command prefix            
            cmd_i2c[1] = gpio_num;   // GPIO number
            cmd_i2c[2] = gpio_val;   // GPIO value
            r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_i2c, PACKET_SIZE, &actual_length, 0); 
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }
    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
}

/**
 * @brief Reads the firmware version from the device.
 * 
 * [Modified] Function declaration: returns int (status), added uint32_t* out_version (output)
 * 
 * @param handle USB device handle.
 * @param ep_out OUT endpoint.
 * @param ep_in IN endpoint.
 * @param gpio_val Output pointer for the GPIO value.
 * @return int 0 on success, -1 on failure.
 */
int read_device_gpio_status(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in,uint8_t GPIONUM,uint8_t* out_val) {
    // Prepare and send the command
    int actual_length = 0;
    char cmd_version[PACKET_SIZE] = { 0 };
    cmd_version[0] = (char)0xdd; // read gpio status command prefix
    cmd_version[1] = GPIONUM;    // GPIO number

    // Perform interrupt transfer to send command
    int r = libusb_interrupt_transfer(handle, ep_out, (unsigned char*)cmd_version, PACKET_SIZE, &actual_length, 0); // 0 = no timeout
    if (r < 0) {
        fprintf(stderr, "Error sending data: %s\n", libusb_error_name(r));
        return -1; // [Modified] Send failed -> return -1
    }

#if 0
    cmd_version[0] = 0xb0; // read version command

    if (send_string_hid_interrupt(handle, ep_out, cmd_version) != 0) {
        fprintf(stderr, "Failed to send read version command.\n");
        return -1; // [Modified] Send failed -> return -1
    }
#endif
    // Prepare the receive buffer
    unsigned char in_data[transfer_size] = { 0 };


    // Read the returned data from the device via interrupt transfer
    int r_in = libusb_interrupt_transfer(handle, ep_in, in_data, sizeof(in_data), &actual_length, 1000); // 1 second timeout

    if (r_in != 0) {
        fprintf(stderr, "Error reading version from device: %s\n", libusb_error_name(r_in));
        return -1; // [Modified] Read failed -> return -1
    }

    // Check if the returned data length is sufficient
    // Based on logic, indices 0~3 are needed, so at least 4 bytes must be read.
    if (actual_length < 4) {
        fprintf(stderr, "Version response was too short (%d bytes).\n", actual_length);
        return -1; // [Modified] Insufficient data -> return -1
    }

    // Convert the received 4 bytes (Big-Endian) to a uint32_t
    // [Modified] Pass the calculation result back through the pointer.
    if (out_val != NULL) {
        *out_val = in_data[1]; // Assuming GPIO value is in the first byte
    }

    return 0; // [Modified] Pass -> return 0
}



/**
 * @brief Gets the reset variable from a specific USB device.
 * 
 * @param usb_cnt Logical index.
 * @param pReset_var Output pointer.
 * @return int 0 on success, 1 on failure.
 */
int usb_multi_gpio_get_var(unsigned char usb_cnt, uint8_t gpionumber, unsigned char* pgpio_val)
{
    libusb_context* ctx = NULL;
    libusb_device** devs;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) return 1; 

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return 1; 
    }

    scan_and_update_map(ctx, devs, cnt);

    libusb_free_device_list(devs, 1);

    if (g_device_count == 0)
        return 1;
    r = -1;
    if (g_device_count > usb_cnt)
    {
        if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) == 0) {
            // Retrieve variable
            r = read_device_gpio_status(handle, ep_out, ep_in,gpionumber ,pgpio_val);
            libusb_release_interface(handle, INTERFACE_NUMBER);
            libusb_close(handle);
        }
    }

    clear_map();

    libusb_exit(ctx);
    return (r == 0) ? 0 : 1;
 
    
}