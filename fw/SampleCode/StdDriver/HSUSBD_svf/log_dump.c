#include <stdio.h>
#include "NuMicro.h"
#include "hid_transfer.h"
#include <string.h>
#include "g_def.h"

/**
 * @brief Converts raw temperature sensor data into degrees Celsius.
 * @param h_bytem The high byte of the raw temperature reading.
 * @param l_byte The low byte of the raw temperature reading.
 * @return The calculated temperature in degrees Celsius as a float.
 * @note This function handles the 13-bit signed format of the temperature sensor.
 */
float show_temperature(uint8_t h_bytem, uint8_t l_byte)
{
    float final_celsius;
    int16_t signed_temp_val;
    uint16_t raw_temp;
    raw_temp = ((uint16_t)h_bytem << 8) | l_byte;
    //  Mask Flags & Handle Sign
    // Bits 15, 14, 13 are flags (TCRIT, HIGH, LOW). Bit 12 is the Sign bit.
    // We only care about Bits 12 down to 0 (13 bits total).
    uint16_t temp_13bit = raw_temp & 0x1FFF;

    // Check Bit 12 for the sign.
    if (temp_13bit & 0x1000)
    {
        // Negative temperature: Convert using manual 13-bit two's complement logic.
        // Subtract 2^13 (8192) to get the correct negative integer value.
        signed_temp_val = (int16_t)(temp_13bit - 8192);
    }
    else
    {
        // Positive temperature
        signed_temp_val = (int16_t)temp_13bit;
    }

    // Convert the signed integer value to Celsius using the defined resolution.
    final_celsius = (float)signed_temp_val * TEMP_RESOLUTION;

    return final_celsius;
}




/**
 * @brief Prints all collected CPLD information to the console.
 * @param p_buf Pointer to the buffer (bmc_report) containing the CPLD data.
 * @return None.
 */
void show_cpld_information(uint8_t *p_buf)
{

    printf("cpld version: 0x%x\n\r", p_buf[cpld_ver]);
    printf("cpld test version: 0x%x\n\r", p_buf[cpld_test_ver]);
    printf("cpld hdd amount: 0x%x\n\r", p_buf[cpld_hdd_amount]);

    // Loop through and print the port status for each possible HDD.
    for (uint8_t loc = 0; loc < cpld_hdd_max_cnt; loc++)
    {
        printf("cpld port status register [0x%x]: 0x%x\n\r", cpld_hdd_port_status + loc, p_buf[cpld_hdd_port_status + loc]);
    }

    // Loop through and print the general status for each possible HDD.
    for (uint8_t loc = 0; loc < cpld_hdd_max_cnt; loc++)
    {
        printf("cpld status register [0x%x]: 0x%x\n\r", cpld_hdd_status + loc, p_buf[cpld_hdd_status + loc]);
    }

    // Loop through and print the LED status for each possible HDD.
    for (uint8_t loc = 0; loc < cpld_hdd_max_cnt; loc++)
    {
        printf("cpld led register [0x%x]: 0x%x\n\r", cpld_hdd_led + loc, p_buf[cpld_hdd_led + loc]);
    }

}
/**
 * @brief Decodes and prints the 32-byte data structure from an NVMe Basic Management Command.
 * @param data Pointer to the 32-byte buffer containing the NVMe data.
 * @return None.
 * @note This function follows the NVMe Management Interface (NVMe-MI) specification
 *       for the "Basic Management Command - Get Feature" response.
 */
void print_nvme_basic_management_info(uint8_t *data)
{

    // --- Block 1: Status Data (Offset 00h - 07h) ---

    // Byte 0: Length of Status (Should be 6)
    uint8_t status_len = data[0];

    if (status_len != 6)
        // If the length is incorrect, the data is invalid, so we exit.
        return;

    printf("=== NVMe Basic Management Command Data Structure (32 Bytes) ===\n");

    printf("Status Length: %d (Expected: 6)\n", status_len);

    // Byte 1: Status Flags (SFLGS)
    uint8_t sflgs = data[1];
    printf("Status Flags (0x%02X):\n", sflgs);
    printf("  - SMBus Arbitration: %s\n", (sflgs & 0x80) ? "No Contention" : "Contention/Reset");
    printf("  - Powered Up:        %s\n", (sflgs & 0x40) ? "Not Ready (Powering Up)" : "Ready"); // Note: 1=Cannot process, 0=Ready
    printf("  - Drive Functional:  %s\n", (sflgs & 0x20) ? "Functional" : "Failure");
    printf("  - Reset Not Required:%s\n", (sflgs & 0x10) ? "Yes" : "Reset Required");
    printf("  - PCIe Link Active:  %s\n", (sflgs & 0x08) ? "Active" : "Down");

    // Byte 2: SMART Critical Warnings
    uint8_t smart_warn = data[2];
    printf("SMART Critical Warnings (0=Warning, 1=OK):\n");
    printf("  - Bits: 0x%02X\n", smart_warn);
    // Note: This is a bitfield. A '0' in a bit position indicates a warning.
    // e.g., Bit 0: Available spare is below threshold.

    // Byte 3: Composite Temperature (CTemp)
    uint8_t temp_raw = data[3];
    printf("Composite Temperature: ");

    if (temp_raw <= 0x7E)
    {
        printf("%d C\n", temp_raw); // 0 to 126C [cite: 76]
    }
    else if (temp_raw == 0x7F)
    {
        printf(">= 127 C\n");       // 127C or higher [cite: 77]
    }
    else if (temp_raw == 0x80)
    {
        printf("No Data / Old Data\n"); // [cite: 78]
    }
    else if (temp_raw == 0x81)
    {
        printf("Sensor Failure\n");     // [cite: 79]
    }
    else if (temp_raw >= 0xC4)
    {
        // 0xC4-0xFF represents negative temperatures in two's complement.
        // Cast to int8_t directly works for 2's complement logic in C
        printf("%d C\n", (int8_t)temp_raw);
    }
    else
    {
        printf("Reserved (0x%02X)\n", temp_raw);
    }

    // Byte 4: Percentage Drive Life Used (PDLU)
    uint8_t life_used = data[4];
    printf("Drive Life Used: %d%%\n", (life_used == 255) ? 255 : life_used);
    // Note: A value of 255 indicates that the percentage used is > 254%.

    // Byte 5-6: Reserved
    // Byte 7: PEC (Packet Error Code) for the Status Block
    printf("PEC (Status Block): 0x%02X\n", data[7]);

    printf("---------------------------------------------------\n");

    // --- Block 2: Identification Data (Offset 08h - 1Fh) ---

    // Byte 8: Length of Identification (Should be 22)
    uint8_t id_len = data[8];
    printf("ID Length: %d (Expected: 22)\n", id_len);

    // Byte 9-10: Vendor ID (VID) - MSB first
    uint16_t vid = (data[9] << 8) | data[10];
    printf("Vendor ID: 0x%04X\n", vid);

    // Byte 11-30: Serial Number (20 ASCII characters)
    char serial[21];

    for (int i = 0; i < 20; i++)
    {
        serial[i] = data[11 + i];
    }

    serial[20] = '\0'; // Null-terminate for printing
    printf("Serial Number: %s\n", serial);

    // Byte 31: PEC (Packet Error Code) for the ID Block
    printf("PEC (ID Block): 0x%02X\n", data[31]);
}
// Global flag set by the UART IRQ handler when the "dumplog" command is received.
volatile uint8_t g_u8DumpLogFlag = 0;

// The command string to listen for on the UART.
static const char CMD_DUMPLOG[] = "dumplog";
#define CMD_LEN  7  // strlen("dumplog")

/**
 * @brief Interrupt handler for UART3.
 * @return None.
 * @details This handler reads incoming characters and uses a simple state machine
 *          to detect the "dumplog" command. If the command is detected, it sets
 *          the global `g_u8DumpLogFlag` to 1.
 */
void UART3_IRQHandler(void)
{
    uint8_t u8InChar = 0xFF;
    uint32_t u32IntSts = UART3->INTSTS;

    // Check if the interrupt is for received data available.
    if (u32IntSts & UART_INTSTS_RDAINT_Msk)
    {

        // Read all available bytes from the RX FIFO.
        while (!UART_GET_RX_EMPTY(UART3))
        {
            uint8_t u8RxData = UART_READ(UART3);

            // --- Command matching state machine ---
            static uint8_t u8CmdIndex = 0; // Current position in the command match.

            // Check if the received byte matches the expected character in the command.
            if (u8RxData == CMD_DUMPLOG[u8CmdIndex])
            {
                u8CmdIndex++; // Match found, advance the index.

                // If the entire command has been matched.
                if (u8CmdIndex >= CMD_LEN)
                {
                    g_u8DumpLogFlag = 1; // Set the flag to trigger the log dump in the main loop.
                    u8CmdIndex = 0;      // Reset the index for the next command.
                }
            }
            else
            {
                // Mismatch occurred.
                // Check if the current character is the start of a new command ('d').
                // This handles cases like "dumdumplog" correctly.
                if (u8RxData == CMD_DUMPLOG[0])
                {
                    u8CmdIndex = 1;
                }
                else
                {
                    u8CmdIndex = 0;
                }
            }

            // --- End of state machine ---
        }

    }


}