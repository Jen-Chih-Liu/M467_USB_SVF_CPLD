#include <stdio.h>
#include "NuMicro.h"
#include "hid_transfer.h"
#include <string.h>
#include "g_def.h"
extern volatile unsigned char mux_TCA9548_flag ;
// Array of possible NVMe-MI I2C addresses to probe.
// This makes it easy to add or remove addresses for different hardware models.
static const uint8_t s_au8NvmeMiAddr[] =
{
	NVME_TP0_ADDR,
    NVME_TP1_ADDR,
    NVME_TP2_ADDR,
    NVME_TP3_ADDR,
};

//#define TCA9548 (0xE0>>1)
const uint8_t s_au8_TCA9548_Addr[] =
{
    (0xe0 >> 1),
    //(0xe2 >> 1), // MUX 1 Address
};
// Array of TCA9548 I2C multiplexer addresses.
// This allows for easy expansion to multiple MUXes.
const uint8_t s_au8_PCA9848_Addr[] =
{
    (0xB2 >> 1), // MUX 1 Address
    (0xB4 >> 1), // MUX 2 Address
    (0xB6 >> 1), // MUX 3 Address
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
#if 0
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

#endif
#if 1
       if (s_au8NvmeMiAddr[i] == NVME_TP0_ADDR)
        {
            // NVME_TP0_ADDR (0xd4>>1) supports the full NVMe-MI Basic
            // Management Command: read the complete data structure.
            u8DataLen = I2C_ReadMultiBytesTwoRegs(i2c_bus, s_au8NvmeMiAddr[i], 0x00, pu8DataBuf, u32ReadCnt);

            if (u8DataLen == u32ReadCnt)
            {
                return 1; // Success
            }
        }
        else if (s_au8NvmeMiAddr[i] == NVME_TP3_ADDR)
        {
            // NVME_TP3_ADDR (0x36>>1) exposes its temperature at registers
            // 0x05/0x06 as a 13-bit two's complement value with 0.0625 C/LSB.
            // Convert it to signed 1 C/LSB for the NVMe CTemp byte.
            uint8_t au8Reg[2];
            uint8_t u8RegPtr = REG_TEMP_DATA; // 0x05

            // Manually set the register pointer with I2C_WriteMultiBytes,
            // then read the two temperature bytes with I2C_ReadMultiBytes.
            I2C_WriteMultiBytes(i2c_bus, s_au8NvmeMiAddr[i], &u8RegPtr, 1);

            if (I2C_ReadMultiBytes(i2c_bus, s_au8NvmeMiAddr[i], au8Reg, 2) == 2)
            {
                int16_t i16Raw = (int16_t)(((au8Reg[0] << 8) | au8Reg[1]) & 0x1FFF);

                // Sign-extend the 13-bit value (bit12 is the sign bit).
                if (i16Raw & 0x1000)
                    i16Raw |= 0xE000;

                // 0.0625 C/LSB -> 1 C/LSB (>> 4), keeping the sign.
                int8_t i8Temp = (int8_t)(i16Raw >> 4);

                // Build a minimal Basic Management structure so downstream parsing
                // (print_nvme_basic_management_info) still reports the temperature.
                memset(pu8DataBuf, 0, u32ReadCnt);
                pu8DataBuf[0] = 6;        // Status length
                pu8DataBuf[3] = (uint8_t)i8Temp;   // Composite temperature (signed, 1 C/LSB)
                return 1; // Success
            }
        }

        else
        {
            // NVME_TP1_ADDR/NVME_TP2_ADDR only expose a temperature
            // sensor: read register 0 and register 1 and derive the composite
            // temperature = ((reg0 << 8) | (reg1 & 0x7ff)) >> 4.
            uint8_t au8Reg[2];
            uint8_t u8RegPtr = NVME_READ_REG;

            // Test method: manually set the register pointer with I2C_WriteMultiBytes,
            // then read the two temperature bytes with I2C_ReadMultiBytes.
            I2C_WriteMultiBytes(i2c_bus, s_au8NvmeMiAddr[i], &u8RegPtr, 1);

            if (I2C_ReadMultiBytes(i2c_bus, s_au8NvmeMiAddr[i], au8Reg, 2) == 2)
            {
                uint8_t u8Temp = (uint8_t)((((au8Reg[0] << 8) | (au8Reg[1] & 0x7ff)) >> 4) & 0xff);

                // Build a minimal Basic Management structure so downstream parsing
                // (print_nvme_basic_management_info) still reports the temperature.
                memset(pu8DataBuf, 0, u32ReadCnt);
                pu8DataBuf[0] = 6;        // Status length
                pu8DataBuf[3] = u8Temp;   // Composite temperature
                return 1; // Success
            }
					}
				
#endif					
#if 0
        u8DataLen = I2C_ReadMultiBytesTwoRegs(i2c_bus, s_au8NvmeMiAddr[i], 0x00, pu8DataBuf, u32ReadCnt);

        if (u8DataLen == u32ReadCnt)
        {
            return 1; // Success
        }
        #endif

			}

    return 0; // Failure: No device responded on any of the known addresses.
}



static uint8_t ReadNvmeDataFromChannel_1(UI2C_T *i2c_bus, uint8_t *pu8DataBuf, uint32_t u32ReadCnt)
{
    uint8_t i;
    uint8_t u8DataLen;
    const uint8_t u8NumAddrs = sizeof(s_au8NvmeMiAddr) / sizeof(s_au8NvmeMiAddr[0]);

    for (i = 0; i < u8NumAddrs; i++)
    {
#if 0

        // Send the register offset we want to read from.
        if (UI2C_WriteByte(i2c_bus, s_au8NvmeMiAddr[i], NVME_READ_REG) == 0)
        {
            // Perform the read operation.
            u8DataLen = UI2C_ReadMultiBytes(i2c_bus, s_au8NvmeMiAddr[i], pu8DataBuf, u32ReadCnt);

            // Check if the read was successful.
            if (u8DataLen == u32ReadCnt)
            {
                return 1; // Success
            }
        }

#endif
#if 1
    if (s_au8NvmeMiAddr[i] == NVME_TP0_ADDR)
        {
            // NVME_TP0_ADDR (0xd4>>1) supports the full NVMe-MI Basic
            // Management Command: read the complete data structure.
            u8DataLen = UI2C_ReadMultiBytesTwoRegs(i2c_bus, s_au8NvmeMiAddr[i], 0x00, pu8DataBuf, u32ReadCnt);

            if (u8DataLen == u32ReadCnt)
            {
                return 1; // Success
            }
        }
        else if (s_au8NvmeMiAddr[i] == NVME_TP3_ADDR)
        {
            // NVME_TP3_ADDR (0x36>>1) exposes its temperature at registers
            // 0x05/0x06 as a 13-bit two's complement value with 0.0625 C/LSB.
            // Convert it to signed 1 C/LSB for the NVMe CTemp byte.
            uint8_t au8Reg[2];
            uint8_t u8RegPtr = REG_TEMP_DATA; // 0x05

            // Manually set the register pointer with UI2C_WriteMultiBytes,
            // then read the two temperature bytes with UI2C_ReadMultiBytes.
            UI2C_WriteMultiBytes(i2c_bus, s_au8NvmeMiAddr[i], &u8RegPtr, 1);

            if (UI2C_ReadMultiBytes(i2c_bus, s_au8NvmeMiAddr[i], au8Reg, 2) == 2)
            {
                int16_t i16Raw = (int16_t)(((au8Reg[0] << 8) | au8Reg[1]) & 0x1FFF);

                // Sign-extend the 13-bit value (bit12 is the sign bit).
                if (i16Raw & 0x1000)
                    i16Raw |= 0xE000;

                // 0.0625 C/LSB -> 1 C/LSB (>> 4), keeping the sign.
                int8_t i8Temp = (int8_t)(i16Raw >> 4);

                // Build a minimal Basic Management structure so downstream parsing
                // (print_nvme_basic_management_info) still reports the temperature.
                memset(pu8DataBuf, 0, u32ReadCnt);
                pu8DataBuf[0] = 6;        // Status length
                pu8DataBuf[3] = (uint8_t)i8Temp;   // Composite temperature (signed, 1 C/LSB)
                return 1; // Success
            }
        }
        else
        {
            // NVME_TP1_ADDR/NVME_TP2_ADDR only expose a temperature
            // sensor: read register 0 and register 1 and derive the composite
            // temperature = ((reg0 << 8) | (reg1 & 0x7ff)) >> 4.
            uint8_t au8Reg[2];
            uint8_t u8RegPtr = NVME_READ_REG;

            // Test method: manually set the register pointer with UI2C_WriteMultiBytes,
            // then read the two temperature bytes with UI2C_ReadMultiBytes.
            UI2C_WriteMultiBytes(i2c_bus, s_au8NvmeMiAddr[i], &u8RegPtr, 1);

            if (UI2C_ReadMultiBytes(i2c_bus, s_au8NvmeMiAddr[i], au8Reg, 2) == 2)
            {
                uint8_t u8Temp = (uint8_t)((((au8Reg[0] << 8) | (au8Reg[1] & 0x7ff)) >> 4) & 0xff);

                // Build a minimal Basic Management structure so downstream parsing
                // (print_nvme_basic_management_info) still reports the temperature.
                memset(pu8DataBuf, 0, u32ReadCnt);
                pu8DataBuf[0] = 6;        // Status length
                pu8DataBuf[3] = u8Temp;   // Composite temperature
                return 1; // Success
            }

#if 0

            if (UI2C_ReadMultiBytesOneReg(i2c_bus, s_au8NvmeMiAddr[i], NVME_READ_REG, au8Reg, 2) == 2)
            {
                uint8_t u8Temp = (uint8_t)((((au8Reg[0] << 8) | (au8Reg[1] & 0x7ff)) >> 4) & 0xff);

                // Build a minimal Basic Management structure so downstream parsing
                // (print_nvme_basic_management_info) still reports the temperature.
                memset(pu8DataBuf, 0, u32ReadCnt);
                pu8DataBuf[0] = 6;        // Status length
                pu8DataBuf[3] = u8Temp;   // Composite temperature
                return 1; // Success
            }

#endif
        }
#endif

#if 0
        u8DataLen = UI2C_ReadMultiBytesTwoRegs(i2c_bus, s_au8NvmeMiAddr[i], 0x00, pu8DataBuf, u32ReadCnt);

        if (u8DataLen == u32ReadCnt)
        {
            return 1; // Success
        }
#endif

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
uint8_t SelectMuxChannel(I2C_T *i2c_bus, uint8_t u8MuxAddr, uint8_t u8Channel)
{
    printf("u8MuxAddr=0x%x\n\r",u8MuxAddr);
    printf("u8Channel=0x%x\n\r",u8Channel);
    if (I2C_WriteByte(i2c_bus, u8MuxAddr, (0x01 << u8Channel)) != 0)
    {
        // Optional: Add error logging here if needed.
         printf("Failed to select MU//X channel %d\n", u8Channel);
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
 * @brief Disables all channels on a PCA9848/TCA9548 I2C multiplexer.
 *
 * Writing 0x00 to the control register disconnects every downstream channel.
 * This must be used (instead of SelectMuxChannel(addr, 0), which actually
 * ENABLES channel 0) to make sure no stale MUX keeps a drive on the bus when
 * switching to another MUX.
 */
static void DisableMux(I2C_T *i2c_bus, uint8_t u8MuxAddr)
{
    I2C_WriteByte(i2c_bus, u8MuxAddr, 0x00);
}

static void DisableMux_1(UI2C_T *i2c_bus, uint8_t u8MuxAddr)
{
    UI2C_WriteByte(i2c_bus, u8MuxAddr, 0x00);
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

    if (bmc_report[cpld_hdd_amount] > 24)
        return;

    // Set HWM_SEL pin to low to enable the I2C bus for NVMe drives.
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    HWM_SEL = 0;

    // Loop through each installed HDD slot as reported by the CPLD.
    for (u8SlotIndex = 0; u8SlotIndex < bmc_report[cpld_hdd_amount]; u8SlotIndex++)
    {
        uint8_t u8MuxIndex = u8SlotIndex / u8ChannelsPerMux;
        uint8_t u8ChannelOnMux = u8SlotIndex % u8ChannelsPerMux;
        //  printf("u8MuxIndex=0x%x\n\r",u8MuxIndex);
        //  printf("u8ChannelOnMux=0x%x\n\r",u8ChannelOnMux);
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

        if (mux_TCA9548_flag == 0)
        {
            // Reset every PCA9848 MUX to channel 0 so no stale channel from a
            // previous MUX stays enabled (important once more than one MUX is used).
            #if 0
            uint8_t u8ResetIdx;
            for (u8ResetIdx = 0; u8ResetIdx < u8MuxCount; u8ResetIdx++)
            {
                SelectMuxChannel(I2C1, s_au8_PCA9848_Addr[u8ResetIdx], 0);
            }
            #endif
            DisableMux(I2C1, s_au8_PCA9848_Addr[0]);
            DisableMux(I2C1, s_au8_PCA9848_Addr[1]);
            DisableMux(I2C1, s_au8_PCA9848_Addr[2]);
            // Select the I2C channel for the current NVMe slot.
            if (!SelectMuxChannel(I2C1, s_au8_PCA9848_Addr[u8MuxIndex], u8ChannelOnMux))
            {

                // Mark data as invalid and skip to the next slot if MUX channel selection fails.
                memset(pu8Dest, 0xFF, NVME_READ_COUNT);
                continue;
            }
        }
        else
        {
            DisableMux(I2C1, s_au8_TCA9548_Addr[0]);

            //DisableMux(I2C1, s_au8_TCA9548_Addr[1]);
            if (!SelectMuxChannel(I2C1, s_au8_TCA9548_Addr[u8MuxIndex], u8ChannelOnMux))
            {
                // Mark data as invalid and skip to the next slot if MUX channel selection fails.
                memset(pu8Dest, 0xFF, NVME_READ_COUNT);
                continue;
            }

        }


        // Attempt to read the NVMe-MI data from the selected channel.
        if (ReadNvmeDataFromChannel(I2C1, au8TempBuf, NVME_READ_COUNT))
        {

            //printf("set\n\r");
            // If successful, copy the data to the correct location in the main report buffer.
            memcpy(pu8Dest, au8TempBuf, NVME_READ_COUNT);
        }
        else
        {
            //printf("no set\n\r");
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

    if (bmc_report1[cpld_hdd_amount] > 24)
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

        if (mux_TCA9548_flag == 0)
        {
        	#if 0
            // Reset every PCA9848 MUX to channel 0 so no stale channel from a
            // previous MUX stays enabled (important once more than one MUX is used).
            uint8_t u8ResetIdx;
            for (u8ResetIdx = 0; u8ResetIdx < u8MuxCount; u8ResetIdx++)
            {
                SelectMuxChannel_1(UI2C0, s_au8_PCA9848_Addr[u8ResetIdx], 0);
            }
#endif
            DisableMux_1(UI2C0, s_au8_PCA9848_Addr[0]);
            DisableMux_1(UI2C0, s_au8_PCA9848_Addr[1]);
            DisableMux_1(UI2C0, s_au8_PCA9848_Addr[2]);
            // Select the I2C channel for the current NVMe slot.
            if (!SelectMuxChannel_1(UI2C0, s_au8_PCA9848_Addr[u8MuxIndex], u8ChannelOnMux))
            {
                // Mark data as invalid and skip to the next slot if MUX channel selection fails.
                memset(pu8Dest, 0xFF, NVME_READ_COUNT);
                continue;
            }
        }
        else
        {
            DisableMux_1(UI2C0, s_au8_TCA9548_Addr[0]);

            // Select the I2C channel for the current NVMe slot.
            if (!SelectMuxChannel_1(UI2C0, s_au8_TCA9548_Addr[u8MuxIndex], u8ChannelOnMux))
            {
                // Mark data as invalid and skip to the next slot if MUX channel selection fails.
                memset(pu8Dest, 0xFF, NVME_READ_COUNT);
                continue;
            }
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
