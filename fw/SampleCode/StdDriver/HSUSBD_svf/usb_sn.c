#include "NuMicro.h"
#include "hid_transfer.h"
#include <string.h>
#include "g_def.h"
uint8_t g_u8StringSerial[USB_SERIAL_STR_LEN] __attribute__((aligned(4))) =
// USB String Descriptor for the Serial Number.
// The first two bytes are the header (bLength, bDescriptorType).
{
    USB_SERIAL_STR_LEN, // bLength
    0x03                // bDescriptorType (STRING)

};
void Set_USB_SerialNumber_From_UID(void)
{
    SYS_UnlockReg();                   /* Unlock register lock protection */

    FMC_Open();                        /* Enable FMC ISP function */

    uint32_t u32UID[3];
    uint32_t i, j;
    // Point to the start of the string data, after the 2-byte header.
    uint8_t *pDesc = &g_u8StringSerial[2];
    uint8_t u8Nibble;
    char cHex;

    // 1. Read the 96-bit Unique ID (UID) from the Nuvoton MCU.
    // The UID is stored in three 32-bit words.
    for (i = 0; i < 3; i++)
    {
        u32UID[i] = FMC_ReadUID(i);

    }

    // 2. Convert the UID (3 x 32-bit words) into a 24-character hexadecimal string.
    for (i = 0; i < 3; i++)
    {
        // To ensure a consistent big-endian display format (e.g., 0x12345678 becomes "1234..."),
        // process each 32-bit word from the Most Significant Nibble (MSN) to the LSN.
        for (j = 0; j < 8; j++)
        {
            // Extract one 4-bit nibble.
            // The shift amount (7-j)*4 ensures we get nibbles from left to right: 28, 24, ..., 0.
            u8Nibble = (u32UID[i] >> ((7 - j) * 4)) & 0x0F;

            // Convert the nibble to its ASCII hexadecimal character representation.
            if (u8Nibble < 10)
            {
                cHex = '0' + u8Nibble;
            }
            else
            {
                cHex = 'A' + (u8Nibble - 10);
            }

            // 3. Fill the descriptor buffer in UTF-16LE format.
            // For standard ASCII characters, the high byte is 0x00.
            // Low Byte: The ASCII character.
            *pDesc++ = (uint8_t)cHex;
            // High Byte: 0x00 for Basic Latin characters.
            *pDesc++ = 0x00;
        }
    }

    SYS_LockReg(); /* Lock register lock protection */
}
