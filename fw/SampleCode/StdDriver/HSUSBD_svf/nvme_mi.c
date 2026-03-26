#include <stdio.h>
#include "NuMicro.h"
#include "hid_transfer.h"
#include <string.h>
#include "g_def.h"

// Array of possible NVMe-MI I2C addresses to probe.
// This makes it easy to add or remove addresses for different hardware models.
static const uint8_t s_au8NvmeMiAddr[] =
{
    NVME_TP1_ADDR,
    NVME_TP2_ADDR,
    NVME_TP3_ADDR,
};

// Array of TCA9548 I2C multiplexer addresses.
// This allows for easy expansion to multiple MUXes.
static const uint8_t s_au8_PCA9848_Addr[] =
{
    (0xB2 >> 1), // MUX 1 Address
    (0xB4 >> 1), // MUX 2 Address
};

/**
 * @brief Attempts to read NVMe-MI data from a device on the current I2C bus.
 *
 * This function iterates through a predefined list of possible NVMe-MI slave
 * addresses and attempts to read the basic management data structure.
 *
 * @param[in]   i2c_bus     The I2C peripheral to use (e.g., I2C1).
 * @param[out]  pu8DataBuf  Pointer to a buffer where the read data will be stored.
 * @param[in]   u32ReadCnt  The number of bytes to read.
 * @return      1 on successful read, 0 on failure.
 */
static uint8_t ReadNvmeDataFromChannel(I2C_T *i2c_bus, uint8_t *pu8DataBuf, uint32_t u32ReadCnt)
{
    uint8_t i;
    uint8_t u8DataLen;
    const uint8_t u8NumAddrs = sizeof(s_au8NvmeMiAddr) / sizeof(s_au8NvmeMiAddr[0]);

    for (i = 0; i < u8NumAddrs; i++)
    {
        // Send the register offset we want to read from.
        if (I2C_WriteByte(i2c_bus, s_au8NvmeMiAddr[i], NVME_READ_REG) == 0)
        {
            // Perform the read operation.
            u8DataLen = I2C_ReadMultiBytes(i2c_bus, s_au8NvmeMiAddr[i], pu8DataBuf, u32ReadCnt);

            // Check if the read was successful.
            if (u8DataLen == u32ReadCnt)
            {
                return 1; // Success
            }
        }
    }

    return 0; // Failure: No device responded on any of the known addresses.
}



static uint8_t ReadNvmeDataFromChannel_1(I2C_T *i2c_bus, uint8_t *pu8DataBuf, uint32_t u32ReadCnt)
{
    uint8_t i;
    uint8_t u8DataLen;
    const uint8_t u8NumAddrs = sizeof(s_au8NvmeMiAddr) / sizeof(s_au8NvmeMiAddr[0]);

    for (i = 0; i < u8NumAddrs; i++)
    {
        // Send the register offset we want to read from.
        if (UI2C_WriteByte(i2c_bus, s_au8NvmeMiAddr[i], NVME_READ_REG) == 0)
        {
            // Perform the read operation.
            u8DataLen = I2C_ReadMultiBytes(i2c_bus, s_au8NvmeMiAddr[i], pu8DataBuf, u32ReadCnt);

            // Check if the read was successful.
            if (u8DataLen == u32ReadCnt)
            {
                return 1; // Success
            }
        }
    }

    return 0; // Failure: No device responded on any of the known addresses.
}


/**
 * @brief Selects a specific channel on the TCA9548 I2C multiplexer.
 *
 * @param[in] i2c_bus    The I2C peripheral connected to the multiplexer (e.g., I2C1).
 * @param[in] u8MuxAddr  The I2C address of the target multiplexer.
 * @param[in] u8Channel  The channel number to select (0-7).
 * @return    1 on success, 0 on failure.
 */
static uint8_t SelectMuxChannel(I2C_T *i2c_bus, uint8_t u8MuxAddr, uint8_t u8Channel)
{
    if (I2C_WriteByte(i2c_bus, u8MuxAddr, (0x01 << u8Channel)) != 0)
    {
        // Optional: Add error logging here if needed.
        // printf("Failed to select MUX channel %d\n", u8Channel);
        return 0; // Failure
    }

    return 1; // Success
}

 
static uint8_t SelectMuxChannel_1(UI2C_T *i2c_bus, uint8_t u8MuxAddr, uint8_t u8Channel)
{
    if (UI2C_WriteByte(i2c_bus, u8MuxAddr, (0x01 << u8Channel)) != 0)
    {
        // Optional: Add error logging here if needed.
        // printf("Failed to select MUX channel %d\n", u8Channel);
        return 0; // Failure
    }

    return 1; // Success
}

/**
 * @brief Reads NVMe-MI (Management Interface) data for all installed drives.
 *
 * This function iterates through the drive slots, uses an I2C multiplexer (TCA9548)
 * to select each drive, and reads the 32-byte "Basic Management Command" data structure.
 * The collected data is stored in the global `bmc_report` buffer.
 */
void nvm_mi_read(void)
{
    uint8_t au8TempBuf[NVME_READ_COUNT]; // Temporary buffer for a single drive's data.
    uint8_t u8SlotIndex;
    uint8_t *pu8Dest;
    const uint8_t u8MuxCount = sizeof(s_au8_PCA9848_Addr) / sizeof(s_au8_PCA9848_Addr[0]);
    const uint8_t u8ChannelsPerMux = 8; // Each TCA9548 has 8 channels

    if (bmc_report[cpld_hdd_amount] == 0xff)
        return;

    // Set HWM_SEL pin to low to enable the I2C bus for NVMe drives.
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    HWM_SEL = 0;

    // Loop through each installed HDD slot as reported by the CPLD.
    for (u8SlotIndex = 0; u8SlotIndex < bmc_report[cpld_hdd_amount]; u8SlotIndex++)
    {
        uint8_t u8MuxIndex = u8SlotIndex / u8ChannelsPerMux;
        uint8_t u8ChannelOnMux = u8SlotIndex % u8ChannelsPerMux;

        // Determine the destination buffer for the current slot.
        pu8Dest = &bmc_report[NVME_MEM_OFFSET + (u8SlotIndex * NVME_READ_COUNT)];

        // Ensure the calculated MUX index is within the bounds of our address array.
        if (u8MuxIndex >= u8MuxCount)
        {
            // This slot number is beyond the capacity of our configured MUXes.
            // Mark data as invalid and continue.
            memset(pu8Dest, 0xFF, NVME_READ_COUNT);
            continue;
        }

        // Select the I2C channel for the current NVMe slot.
        if (!SelectMuxChannel(I2C1, s_au8_PCA9848_Addr[u8MuxIndex], u8ChannelOnMux))
        {
            // Mark data as invalid and skip to the next slot if MUX channel selection fails.
            memset(pu8Dest, 0xFF, NVME_READ_COUNT);
            continue;
        }

        // Attempt to read the NVMe-MI data from the selected channel.
        if (ReadNvmeDataFromChannel(I2C1, au8TempBuf, NVME_READ_COUNT))
        {
            // If successful, copy the data to the correct location in the main report buffer.
            memcpy(pu8Dest, au8TempBuf, NVME_READ_COUNT);
        }
        else
        {
            // Optional: If the read failed, clear the buffer area for this slot.
            memset(pu8Dest, 0xFF, NVME_READ_COUNT); // Fill with 0xFF to indicate no data.
        }
    }

    // Set the hardware selection pin back to high, disabling the NVMe I2C bus.
    HWM_SEL = 1;
}



void nvm_mi_read_1(void)
{
    uint8_t au8TempBuf[NVME_READ_COUNT]; // Temporary buffer for a single drive's data.
    uint8_t u8SlotIndex;
    uint8_t *pu8Dest;
    const uint8_t u8MuxCount = sizeof(s_au8_PCA9848_Addr) / sizeof(s_au8_PCA9848_Addr[0]);
    const uint8_t u8ChannelsPerMux = 8; // Each TCA9548 has 8 channels

    if (bmc_report1[cpld_hdd_amount] == 0xff)
        return;

    // Set HWM_SEL pin to low to enable the I2C bus for NVMe drives.
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    HWM_SEL = 0;

    // Loop through each installed HDD slot as reported by the CPLD.
    for (u8SlotIndex = 0; u8SlotIndex < bmc_report1[cpld_hdd_amount]; u8SlotIndex++)
    {
        uint8_t u8MuxIndex = u8SlotIndex / u8ChannelsPerMux;
        uint8_t u8ChannelOnMux = u8SlotIndex % u8ChannelsPerMux;

        // Determine the destination buffer for the current slot.
        pu8Dest = &bmc_report1[NVME_MEM_OFFSET + (u8SlotIndex * NVME_READ_COUNT)];

        // Ensure the calculated MUX index is within the bounds of our address array.
        if (u8MuxIndex >= u8MuxCount)
        {
            // This slot number is beyond the capacity of our configured MUXes.
            // Mark data as invalid and continue.
            memset(pu8Dest, 0xFF, NVME_READ_COUNT);
            continue;
        }

        // Select the I2C channel for the current NVMe slot.
        if (!SelectMuxChannel_1(UI2C0, s_au8_PCA9848_Addr[u8MuxIndex], u8ChannelOnMux))
        {
            // Mark data as invalid and skip to the next slot if MUX channel selection fails.
            memset(pu8Dest, 0xFF, NVME_READ_COUNT);
            continue;
        }

        // Attempt to read the NVMe-MI data from the selected channel.
        if (ReadNvmeDataFromChannel_1(UI2C0, au8TempBuf, NVME_READ_COUNT))
        {
            // If successful, copy the data to the correct location in the main report buffer.
            memcpy(pu8Dest, au8TempBuf, NVME_READ_COUNT);
        }
        else
        {
            // Optional: If the read failed, clear the buffer area for this slot.
            memset(pu8Dest, 0xFF, NVME_READ_COUNT); // Fill with 0xFF to indicate no data.
        }
    }

    // Set the hardware selection pin back to high, disabling the NVMe I2C bus.
    HWM_SEL = 1;
}
