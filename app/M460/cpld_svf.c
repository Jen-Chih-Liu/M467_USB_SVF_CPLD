// svf_usb_jtag.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
// 20250827 initial release
#ifdef _WIN32
#include <windows.h> // Needed for Sleep() on Windows platforms
#else
#include <time.h>    // Needed for nanosleep() on POSIX platforms (Linux, macOS)
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "libusb\include\libusb.h"
#include "config.h"
#define last_check 1
#define dump_svf_log 0

typedef struct {
    char** commands;
    int count;
} SvfCommands;
/**
 * @brief Cross-platform function for delays in seconds.
 * @param seconds The number of seconds to delay (can be a decimal).
 */
void sleep_seconds_svf(double seconds) {
    // Add a check to avoid issues with negative numbers
    if (seconds <= 0.0) {
        return;
    }

#ifdef _WIN32
    // On Windows platforms:
    // 1. Use the Sleep() function from <windows.h>.
    // 2. Sleep() accepts an integer in milliseconds (ms).
    // 3. Therefore, we multiply the seconds by 1000.
    // 4. (DWORD) is a type cast to ensure the correct parameter type is passed.
    Sleep((DWORD)(seconds * 1000));
#else
    // On POSIX platforms (your original version):
    // 1. Use the nanosleep() function from <time.h>.
    // 2. nanosleep() accepts a timespec struct, which contains seconds and nanoseconds.
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
#endif
}

/**
 * @brief Reads from the device to check for an error status code.
 * @param handle The libusb device handle.
 * @param ep_in The IN endpoint address.
 * @return Returns the 32-bit error code from the device. Returns 0 if there is no error.
 */
static uint32_t check_device_status(libusb_device_handle* handle, uint8_t ep_in) {
    unsigned char in_data[transfer_size] = { 0 };
    int actual_length = 0;
    uint32_t error_code = 0;

    int r_in = libusb_interrupt_transfer(handle, ep_in, in_data, sizeof(in_data), &actual_length, 1000); // 1-second timeout

    if (r_in == 0 && actual_length >= 4) {
        // Convert the received 4 bytes (Little-Endian) to a uint32_t
        error_code = in_data[0] | (in_data[1] << 8) | (in_data[2] << 16) | (in_data[3] << 24);
    }
    // Note: We ignore timeouts or other errors here, as they might just mean the device is busy
    // and hasn't sent an error code. The function will simply return 0 in those cases.
    return error_code;
}

/**
 * @brief Sends string data via HID Interrupt endpoint (padded to a multiple of PACKET_SIZE).
 * @param handle The device handle.
 * @param ep_out The OUT endpoint address.
 * @param data_string The string to send.
 * @return 0 on success, -1 on failure.
 */

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
/**
 * @brief Processes and sends all SVF commands.
 * @param svf Command struct.
 * @param handle USB handle.
 * @param ep_out/ep_in Endpoints.
 * @param out_failed_line [Output] Pointer to store the failed command index (1-based).
 * @return CPLD_ERROR enum value (0 for success).
 */
int program_device_with_svf(SvfCommands svf, libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in, int* out_failed_line) {
    // Note: Initialize output parameter
    if (out_failed_line) *out_failed_line = 0;

    for (int i = 0; i < svf.count; i++) {
        const char* original_cmd = svf.commands[i];
        size_t original_len = strlen(original_cmd);

        // --- Prepare command with 0xA1 prefix for device ---
        char* cmd = (char*)malloc(original_len + 1);
        if (cmd == NULL) {
            fprintf(stderr, "Error: Memory allocation failed!\n");
            if (out_failed_line) *out_failed_line = i + 1;
            return CPLD_ERR_SVF_PARSE;
        }
        cmd[0] = 0xa1;
        memcpy(cmd + 1, original_cmd, original_len);
#if dump_svf_log
        printf("Command %d: %s\n", i + 1, original_cmd); // Uncomment for verbose debugging

        if ((i + 1) % 100 == 0) {
            printf("Progress: %d / %d\n", (i + 1), svf.count);
        }
#endif
        // --- Send Command ---
        if (send_string_hid_interrupt(handle, ep_out, cmd) != 0) {
            free(cmd);
            if (out_failed_line) *out_failed_line = i + 1;
            return CPLD_ERR_TIMEOUT; 
        }
        free(cmd);

        // --- Handle Delay (Command-specific) ---
        const char* sec_ptr = strstr(original_cmd, " SEC");
        if (sec_ptr != NULL) {
            // Simple parsing for delay value before " SEC"
            const char* num_start = sec_ptr;
            while (num_start > original_cmd && (*(num_start - 1) == ' ' || *(num_start - 1) == '\t')) {
                num_start--;
            }
            while (num_start > original_cmd && (isdigit(*(num_start - 1)) || strchr(".E+-", *(num_start - 1)))) {
                num_start--;
            }

            double seconds = atof(num_start);
#if 0
            if (seconds >= 0.05) {
                printf("Waiting for %.4f seconds...\n", seconds);
                sleep_seconds_svf(seconds);
            }
#endif
#if 1
            if (i < ((svf.count * 2) / 3))
            {
                // Unconditional sleep for first 2/3 of commands
                sleep_seconds_svf(seconds);
            }
            else
            {
                if (seconds >= 0.05) {
                    printf("Waiting for %.4f seconds...\n", seconds);
                    sleep_seconds_svf(seconds);
                }
            }
#endif
        }

        // --- Periodic Status Check ---
        if ((i + 1) % 1000 == 0) {
            uint32_t error_code = check_device_status(handle, ep_in);
            if (error_code != 0) {
                fprintf(stderr, "Error! Device reported error at SVF cmd %d (DevCode: %u)\n", i + 1, error_code);
                if (out_failed_line) *out_failed_line = i + 1;
                return CPLD_ERR_PROGRAMMING;
            }
        }
    }

    // --- Final Check ---
#ifdef last_check
    printf("Performing final check...\n");
    uint32_t final_error_code = check_device_status(handle, ep_in);
    if (final_error_code != 0) {
        fprintf(stderr, "Error! Device reported final error (DevCode: %u)\n", final_error_code);
        // This usually means the last command failed or verification failed
        if (out_failed_line) *out_failed_line = svf.count;
        return CPLD_ERR_PROGRAMMING;
    }
#endif

    return CPLD_SUCCESS;
}

/**
 * @brief Sends a command to read the version from the device and returns the version number.
 * @param handle The libusb device handle.
 * @param ep_out The OUT endpoint address.
 * @param ep_in The IN endpoint address.
 * @return Returns the 32-bit version number on success, or 0 on failure or if the version is zero.
 */
uint32_t read_device_version_svf(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in) {
    // Prepare and send the command
    char cmd_version[2] = { 0 }; // Use an array and initialize it
    cmd_version[0] = 0xb0; // read version command
    if (send_string_hid_interrupt(handle, ep_out, cmd_version) != 0) {
        fprintf(stderr, "Failed to send read version command.\n");
        return 0; // Send failed
    }

    // Prepare the receive buffer
    unsigned char in_data[transfer_size] = { 0 };
    int actual_length = 0;

    // Read the returned data from the device
    int r_in = libusb_interrupt_transfer(handle, ep_in, in_data, sizeof(in_data), &actual_length, 1000); // 1 second timeout

    if (r_in != 0) {
        fprintf(stderr, "Error reading version from device: %s\n", libusb_error_name(r_in));
        return 0; // Read failed
    }

    // Check if the returned data length is sufficient
    if (actual_length < 4) {
        fprintf(stderr, "Version response was too short (%d bytes).\n", actual_length);
        return 0; // Insufficient data
    }

    // Convert the received 4 bytes (Big-Endian) to a uint32_t
    // Note: in_data[0] is treated as the most significant byte, which is Big-Endian.
    uint32_t received_value = (in_data[0] << 24) | (in_data[1] << 16) | (in_data[2] << 8) | in_data[3];

    return received_value;
}



/**
 * @brief Reads an SVF file, preserving its format, until a semicolon is encountered.
 * @param filepath The path to the file.
 * @return A SvfCommands struct containing the command array and count.
 */
SvfCommands read_svf_preserve_format(const char* filepath) {
    SvfCommands svf = { NULL, 0 };
    FILE* f = fopen(filepath, "r");
    if (f == NULL) {
        perror("Could not open file");
        return svf;
    }

    char line[4096];
    char* current_command = NULL;
    size_t current_len = 0;

    while (fgets(line, sizeof(line), f)) {
        // Ignore comments and empty lines
        char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0' || *trimmed == '!' || *trimmed == '\n' || *trimmed == '\r') {
            continue;
        }

        // Remove trailing newline characters
        line[strcspn(line, "\r\n")] = 0;

        size_t line_len = strlen(line);

        // Append the current line to current_command
        char* new_command = realloc(current_command, current_len + line_len + 1);
        if (new_command == NULL) {
            free(current_command);
            fclose(f);
            svf.commands = NULL; // Indicate an error
            return svf;
        }
        current_command = new_command;
        memcpy(current_command + current_len, line, line_len + 1);
        current_len += line_len;

        // If the line contains a semicolon, it marks the end of a command
        if (strchr(line, ';') != NULL) {
            svf.count++;
            char** new_commands = realloc(svf.commands, svf.count * sizeof(char*));
            if (new_commands == NULL) {
                free(current_command);
                fclose(f);
                svf.commands = NULL; // Indicate an error
                return svf;
            }
            svf.commands = new_commands;
            svf.commands[svf.count - 1] = current_command;

            current_command = NULL; // Reset
            current_len = 0;
        }
    }

    free(current_command); // Handle case where the file does not end with a semicolon
    fclose(f);
    return svf;
}

/**
 * @brief Frees the memory allocated by read_svf_preserve_format.
 */
void free_svf_commands(SvfCommands svf) {
    if (svf.commands) {
        for (int i = 0; i < svf.count; i++) {
            free(svf.commands[i]);
        }
        free(svf.commands);
    }
}



/**
 * @brief Sends the SVF state initialization command (0xa0) and checks for an error response.
 * @param handle The libusb device handle.
 * @param ep_out The OUT endpoint address.
 * @param ep_in  The IN endpoint address.
 * @return Returns 0 on success. If the device reports an error, its 32-bit error code is returned.
 * Returns 0xFFFFFFFF on a communication failure.
 */
uint32_t initialize_svf_state(libusb_device_handle* handle, uint8_t ep_out, uint8_t ep_in) {
    // 1. Prepare and send the initialization command.
    char command[2] = { (char)0xa0, 0 }; // Command to initialize SVF state.
    send_string_hid_interrupt(handle, ep_out, command);

    // 2. Attempt to read the response from the device.
    unsigned char response_data[transfer_size] = { 0 };
    int actual_length = 0;
    int read_status = libusb_interrupt_transfer(handle, ep_in, response_data, sizeof(response_data), &actual_length, 1000); // 1-second timeout

    // 3. Process the response.
    if (read_status == 0 && actual_length >= 4) {
        // The device responded. Convert the 4-byte response to a 32-bit integer.
        // NOTE: This conversion is for a Big-Endian byte order, where the most
        // significant byte (MSB) arrives first in the data stream.
        uint32_t error_code = (response_data[0] << 24) |
            (response_data[1] << 16) |
            (response_data[2] << 8) |
            response_data[3];

        return error_code; // If 0, it's success. Otherwise, it's a device-specific error code.

    }
    else if (read_status != 0 && read_status != LIBUSB_ERROR_TIMEOUT) {
        // An error occurred during the USB transfer itself.
        fprintf(stderr, "Failed to read SVF init response: %s\n", libusb_error_name(read_status));
        return 0xFFFFFFFF; // Return a special value for communication failure.
    }

    // No error reported by the device and no communication failure (e.g., a timeout with no data).
    return 0; // Success
}


const char* cpld_error_to_string(int err_code) {
    switch (err_code) {
    case CPLD_SUCCESS:
        return "Success";

    case CPLD_ERR_FILE_ACCESS:
        return "SVF File Access Error"; 

    case CPLD_ERR_SVF_PARSE:
        return "SVF Parse Error";       

    case CPLD_ERR_USB_INIT:
        return "USB Init Failed";       

    case CPLD_ERR_NO_DEVICE:
        return "Target Device Not Found"; 

    case CPLD_ERR_OPEN_DEVICE:
        return "Open USB Device Failed"; 

    case CPLD_ERR_READ_VERSION:
        return "Read Firmware Version Failed";

    case CPLD_ERR_JTAG_INIT:
        return "JTAG State Init Failed"; // Response error for 0xA0 command (likely cable issue)

    case CPLD_ERR_PROGRAMMING:
        return "Programming Verification Failed"; // Device returned error during programming

    case CPLD_ERR_TIMEOUT:
        return "USB Transfer Timeout";   

    default:
        return "Unknown Error Code";     
    }
}

int cpld_svf_update(unsigned char usb_cnt, char* svf_file, char* fail_reason_buf, size_t buf_len)
{
    int status = CPLD_SUCCESS; 
    int error_line_index = 0; // Stores the failed command index
    SvfCommands svf = { NULL, 0 };
    libusb_context* ctx = NULL;
    libusb_device** devs = NULL;
    libusb_device_handle* handle = NULL;
    uint8_t ep_out = 0, ep_in = 0;
    int r;
    time_t pstart_time, pend_time;

    // 0. Parameter check
    if (svf_file == NULL) {
        fprintf(stderr, "[CPLD] Error: SVF filename is NULL\n");
        return CPLD_ERR_FILE_ACCESS;
    }

    // 1. Read SVF file
    printf("[CPLD] Reading SVF file: %s\n", svf_file);
    svf = read_svf_preserve_format(svf_file);
    if (svf.commands == NULL || svf.count == 0) {
        fprintf(stderr, "[CPLD] Failed to read or parse SVF file\n");
        return CPLD_ERR_SVF_PARSE; 
    }
    printf("[CPLD] Total %d SVF commands read\n", svf.count);

    // 2. USB Initialization
    r = libusb_init(&ctx);
    if (r < 0) {
        fprintf(stderr, "[CPLD] Libusb init failed: %s\n", libusb_error_name(r));
        status = CPLD_ERR_USB_INIT;
        goto cleanup_svf; 
    }

    // 3. Scan device list
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        fprintf(stderr, "[CPLD] Get device list failed\n");
        status = CPLD_ERR_USB_INIT;
        goto cleanup_ctx;
    }

    scan_and_update_map(ctx, devs, cnt);

    // Check if specified index exists
    if (g_device_count == 0 || usb_cnt >= g_device_count) {
        fprintf(stderr, "[CPLD] Device index %d not found (Total: %d)\n", usb_cnt, g_device_count);
        status = CPLD_ERR_NO_DEVICE;
        goto cleanup_devs;
    }

    // 4. Open specific device
    if (open_specific_device_and_endpoints(MyDeviceMap[usb_cnt].device, &handle, &ep_out, &ep_in) != 0) {
        fprintf(stderr, "[CPLD] Failed to open device %d\n", usb_cnt);
        status = CPLD_ERR_OPEN_DEVICE;
        goto cleanup_devs;
    }

    // 5. Read version (Optional Check)
    uint32_t version = read_device_version_svf(handle, ep_out, ep_in);
    if (version == 0) {
        fprintf(stderr, "[CPLD] Warning: Could not retrieve device version (or version is 0)\n");
    }
    else {
        dbg_printf("[CPLD] Device version: 0x%x\n", version);
    }

    // 6. Initialize JTAG/SVF state
    uint32_t init_error = initialize_svf_state(handle, ep_out, ep_in);
    if (init_error != 0) {
        fprintf(stderr, "[CPLD] SVF state initialization failed (Code: 0x%x)\n", init_error);
        status = CPLD_ERR_JTAG_INIT;
        goto cleanup_handle;
    }
    dbg_printf("[CPLD] SVF state initialized.\n");

    // 7. Execute programming
    dbg_printf("[CPLD] Starting programming...\n");
    pstart_time = time(NULL);

    status = program_device_with_svf(svf, handle, ep_out, ep_in, &error_line_index);

    if (status != CPLD_SUCCESS) {
        fprintf(stderr, "[CPLD] Programming Failed at command #%d!\n", error_line_index);

        // --- Format error message ---
        if (fail_reason_buf) {
            if (status == CPLD_ERR_PROGRAMMING) {
                snprintf(fail_reason_buf, buf_len, "Programming Fail at Line %d", error_line_index);
            }
            else {
                snprintf(fail_reason_buf, buf_len, "%s (Line %d)", cpld_error_to_string(status), error_line_index);
            }
        }
    }
    else {
        dbg_printf("[CPLD] Programming Success!\n");
    }

    pend_time = time(NULL);
    dbg_printf("[CPLD] Total time: %lld seconds\n", (long long)(pend_time - pstart_time));


    // --- Resource release ---
cleanup_handle:
    if (handle) {
        libusb_release_interface(handle, INTERFACE_NUMBER);
        libusb_close(handle);
    }

cleanup_devs:
    if (devs) {
        libusb_free_device_list(devs, 1);
    }
    clear_map(); 

cleanup_ctx:
    if (ctx) {
        libusb_exit(ctx);
    }

cleanup_svf:
    free_svf_commands(svf);

    return status; // Return final error code (0 = Success)
}