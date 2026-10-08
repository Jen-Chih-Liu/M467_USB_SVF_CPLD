#include <stdio.h>
#include "NuMicro.h"
#include "hid_transfer.h"
#include <string.h>
#include "g_def.h"
extern volatile unsigned char mux_TCA9548_flag ;
/* 0 = second CPLD (UI2C0 bank) present, 1 = lost/absent. Defined in main.c. */
extern volatile uint32_t g_u32clpd_lost;
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
    //printf("u8MuxAddr=0x%x\n\r",u8MuxAddr);
   // printf("u8Channel=0x%x\n\r",u8Channel);
    if (I2C_WriteByte(i2c_bus, u8MuxAddr, (0x01 << u8Channel)) != 0)
    {
        // Optional: Add error logging here if needed.
         //printf("Failed to select MU//X channel %d\n", u8Channel);
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

        // Only NVMe drives support NVMe-MI commands. The CPLD port status
        // register stores 2 bits per slot (0b11 = NVMe present); only read
        // those slots and skip everything else (empty / SATA / not present).
        {
            uint8_t u8PortStatus = bmc_report[cpld_hdd_port_status + (u8SlotIndex / 4)];
            uint8_t u8SlotBits = (u8PortStatus >> ((u8SlotIndex % 4) * 2)) & 0x03;

            if (u8SlotBits != 0x03)
            {
                // Not an NVMe drive: mark data invalid and skip.
                memset(pu8Dest, 0xFF, NVME_READ_COUNT);
                continue;
            }
        }

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

        // Only NVMe drives support NVMe-MI commands. The CPLD port status
        // register stores 2 bits per slot (0b11 = NVMe present); only read
        // those slots and skip everything else (empty / SATA / not present).
        {
            uint8_t u8PortStatus = bmc_report1[cpld_hdd_port_status + (u8SlotIndex / 4)];
            uint8_t u8SlotBits = (u8PortStatus >> ((u8SlotIndex % 4) * 2)) & 0x03;

            if (u8SlotBits != 0x03)
            {
                // Not an NVMe drive: mark data invalid and skip.
                memset(pu8Dest, 0xFF, NVME_READ_COUNT);
                continue;
            }
        }

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


/* =======================================================================
 * NVMe-MI native command execution over MCTP / SMBus (I2C1)
 * -----------------------------------------------------------------------
 * Ported from the reference main_mctp_test.c so the production firmware can
 * execute native single-slot NVMe-MI commands on demand from the host
 * 0xE0 NVMEMI protocol. Currently used by:
 *     CommandID 6 = Configuration Get (Opcode 0x04)
 *     CommandID 7 = VPD Read          (Opcode 0x05)
 *
 * All helper state uses MCTP-specific names (s_mctp*) so it never collides
 * with the I2C4 BMC-slave state (g_u8SlvDataLen / g_au8SlvData / I2C_SlaveTRx)
 * that lives in main.c. I2C1 is normally used as a master (mux + Basic
 * Management polling); this module briefly reconfigures it as a slave
 * (address 0x10) only for the duration of one request/response exchange and
 * disables its interrupt again afterwards, leaving it usable as a master.
 * ======================================================================= */

#define MCU_ADDR_7BIT        0x10U          /* MCU MCTP endpoint slave address (7-bit) */
#define MCTP_I2C_PORT        I2C1           /* I2C bus used for MCTP/NVMe-MI */
#define MCTP_TX_MTU          64U            /* MCTP transmit MTU (payload bytes/packet) */
#define MCTP_RX_TIMEOUT_CNT  0x00A00000UL   /* Busy-wait timeout for a response packet */
#define MCTP_RXQ_DEPTH       16U            /* RX packet queue depth */
#define MCTP_RXQ_PKT_SIZE    128U           /* Max bytes per queued RX packet */

/* Slave RX scratch buffer + per-packet queue (filled by the I2C1 slave ISR). */
static uint8_t          s_mctpRxData[512];
static volatile uint8_t s_mctpRxLen  = 0;
static volatile uint8_t s_mctpRxFlag = 0;
static uint8_t          s_mctpRxqPkt[MCTP_RXQ_DEPTH][MCTP_RXQ_PKT_SIZE];
static uint8_t          s_mctpRxqLen[MCTP_RXQ_DEPTH];
static volatile uint8_t s_mctpRxqWr     = 0;
static volatile uint8_t s_mctpRxqRd     = 0;
static volatile uint8_t s_mctpRxqCurLen = 0;

typedef void (*mctp_i2c_cb_t)(uint32_t u32Status);
static mctp_i2c_cb_t s_mctpI2C1Handler = NULL;
static mctp_i2c_cb_t s_mctpUI2C0Handler = NULL;

/* Which bus the current MCTP/NVMe-MI exchange runs on:
 *   0 = I2C1  (bank 0, first CPLD's drives)
 *   1 = UI2C0 (bank 1, second CPLD's drives)
 * Set by SelectNvmeSlotChannel() from the global slot index. */
static volatile uint8_t s_mctpBus = 0;

/* UI2C0 slave RX phase tracker: 0 = next ACK is the address byte (discard),
 * 1 = next ACK is a data byte (store). Reset on START/STOP/NACK. */
static volatile uint8_t s_mctpUi2cData = 0;

/* I2C1 interrupt vector: only active while an MCTP exchange is in progress. */
void I2C1_IRQHandler(void)
{
    uint32_t u32Status = I2C_GET_STATUS(I2C1);

    if (s_mctpI2C1Handler != NULL)
        s_mctpI2C1Handler(u32Status);
}

/* Slave receive callback: stores incoming SMBus/MCTP bytes and, on STOP,
 * pushes the completed packet onto the RX queue for reassembly. */
static void Mctp_I2C1_SlaveTRx(uint32_t u32Status)
{
    uint8_t u8data;

    if (u32Status == 0x60)                  /* Own SLA+W received; ACK returned */
    {
        s_mctpRxLen = 0;
        s_mctpRxqCurLen = 0U;
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if ((u32Status == 0x80) || (u32Status == 0x88)) /* Data received; ACK returned */
    {
        u8data = (uint8_t)I2C_GET_DATA(I2C1);

        s_mctpRxData[s_mctpRxLen++] = u8data;
        if (s_mctpRxqCurLen < MCTP_RXQ_PKT_SIZE)
            s_mctpRxqPkt[s_mctpRxqWr][s_mctpRxqCurLen] = u8data;
        if (s_mctpRxqCurLen < 0xFFU)
            s_mctpRxqCurLen++;              /* > PKT_SIZE marks an oversize packet */

        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if (u32Status == 0xA0)             /* STOP/repeated START while addressed as slave */
    {
        s_mctpRxFlag = 1;
        s_mctpRxLen = 0;
        {
            uint8_t nextWr = (uint8_t)((s_mctpRxqWr + 1U) % MCTP_RXQ_DEPTH);
            if ((s_mctpRxqCurLen > 0U) && (nextWr != s_mctpRxqRd))
            {
                s_mctpRxqLen[s_mctpRxqWr] = s_mctpRxqCurLen;
                s_mctpRxqWr = nextWr;
            }
            s_mctpRxqCurLen = 0U;
        }
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    }
    else                                    /* Bus error / arbitration lost */
    {
        if ((u32Status == 0x68) || (u32Status == 0xB0))
        {
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI_AA);
        }
        else
        {
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_STO_SI);
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI);
        }
    }

    I2C_WAIT_SI_CLEAR(I2C1);
}

/* USCI0 (UI2C0) interrupt vector: only active while an MCTP exchange is in
 * progress on the UI2C0 (bank 1) bus. */
void USCI0_IRQHandler(void)
{
    uint32_t u32Status = UI2C_GET_PROT_STATUS(UI2C0);

    if (s_mctpUI2C0Handler != NULL)
        s_mctpUI2C0Handler(u32Status);
}

/* UI2C0 slave receive callback: USCI I2C uses protocol status flags instead of
 * the plain I2C state codes, so this mirrors Mctp_I2C1_SlaveTRx() using the
 * STAR/ACK/NACK/STOR interrupt flags. Incoming SMBus/MCTP bytes are queued the
 * same way (s_mctpRxq*) so the shared reassembly path is reused unchanged. The
 * first ACK after a START carries the matched slave address and is discarded;
 * subsequent ACKs are payload bytes (command code 0x0F, byte count, ...). */
static void Mctp_UI2C0_SlaveTRx(uint32_t u32Status)
{
    if ((u32Status & UI2C_PROTSTS_STARIF_Msk) == UI2C_PROTSTS_STARIF_Msk)
    {
        UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_STARIF_Msk);
        s_mctpRxLen = 0;
        s_mctpRxqCurLen = 0U;
        s_mctpUi2cData = 0U;                 /* next ACK = address byte */
        UI2C_SET_CONTROL_REG(UI2C0, (UI2C_CTL_PTRG | UI2C_CTL_AA));
    }
    else if ((u32Status & UI2C_PROTSTS_ACKIF_Msk) == UI2C_PROTSTS_ACKIF_Msk)
    {
        UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_ACKIF_Msk);

        if (s_mctpUi2cData == 0U)
        {
            /* Address phase: consume the matched slave address, start a fresh
             * packet, then switch to storing data bytes. */
            (void)UI2C_GET_DATA(UI2C0);
            s_mctpRxLen = 0;
            s_mctpRxqCurLen = 0U;
            s_mctpUi2cData = 1U;
        }
        else
        {
            uint8_t u8data = (uint8_t)UI2C_GET_DATA(UI2C0);

            s_mctpRxData[s_mctpRxLen++] = u8data;
            if (s_mctpRxqCurLen < MCTP_RXQ_PKT_SIZE)
                s_mctpRxqPkt[s_mctpRxqWr][s_mctpRxqCurLen] = u8data;
            if (s_mctpRxqCurLen < 0xFFU)
                s_mctpRxqCurLen++;          /* > PKT_SIZE marks an oversize packet */
        }

        UI2C_SET_CONTROL_REG(UI2C0, (UI2C_CTL_PTRG | UI2C_CTL_AA));
    }
    else if ((u32Status & UI2C_PROTSTS_NACKIF_Msk) == UI2C_PROTSTS_NACKIF_Msk)
    {
        UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_NACKIF_Msk);
        s_mctpUi2cData = 0U;
        UI2C_SET_CONTROL_REG(UI2C0, (UI2C_CTL_PTRG | UI2C_CTL_AA));
    }
    else if ((u32Status & UI2C_PROTSTS_STORIF_Msk) == UI2C_PROTSTS_STORIF_Msk)
    {
        UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_STORIF_Msk);

        s_mctpRxFlag = 1;
        s_mctpRxLen = 0;
        {
            uint8_t nextWr = (uint8_t)((s_mctpRxqWr + 1U) % MCTP_RXQ_DEPTH);
            if ((s_mctpRxqCurLen > 0U) && (nextWr != s_mctpRxqRd))
            {
                s_mctpRxqLen[s_mctpRxqWr] = s_mctpRxqCurLen;
                s_mctpRxqWr = nextWr;
            }
            s_mctpRxqCurLen = 0U;
        }
        s_mctpUi2cData = 0U;
        UI2C_SET_CONTROL_REG(UI2C0, (UI2C_CTL_PTRG | UI2C_CTL_AA));
    }
}

/* SMBus PEC (CRC-8): poly x^8 + x^2 + x + 1 (0x07), init 0. */
static uint8_t mctp_calc_pec(uint8_t *data, size_t len)
{
    uint8_t crc = 0;
    size_t  i;
    int     j;

    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x07);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* NVMe-MI MIC (CRC-32C, reflected): poly 0x82F63B78, init 0xFFFFFFFF, final XOR. */
static uint32_t mctp_calc_mic(uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    size_t   i;
    int      j;

    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0; j < 8; j++)
        {
            if (crc & 1U)
                crc = (crc >> 1) ^ 0x82F63B78UL;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* Split one NVMe-MI message (NMH..MIC) into MCTP-over-SMBus packets and send. */
static void mctp_tx_message(uint8_t *pMsg, uint16_t msgLen)
{
    uint8_t  pkt[8U + MCTP_TX_MTU + 1U];
    uint16_t off = 0U;
    uint8_t  seq = 0U;

    if (pMsg == NULL)
        return;

    while (off < msgLen)
    {
        uint16_t frag = (uint16_t)(msgLen - off);
        uint8_t  som  = (off == 0U) ? 1U : 0U;
        uint8_t  eom;
        uint16_t p;

        if (frag > MCTP_TX_MTU)
            frag = MCTP_TX_MTU;
        eom = ((uint16_t)(off + frag) >= msgLen) ? 1U : 0U;

        pkt[0] = 0x3A;                              /* SSD address (write) */
        pkt[1] = 0x0F;                              /* MCTP over SMBus */
        pkt[2] = (uint8_t)(5U + frag);             /* byte count = src(1)+MCTP hdr(4)+payload */
        pkt[3] = (MCU_ADDR_7BIT << 1) | 1U;        /* source slave address */
        pkt[4] = 0x01;                              /* header version */
        pkt[5] = 0x00;                              /* dest EID */
        pkt[6] = 0x01;                              /* src EID */
        pkt[7] = (uint8_t)((som << 7) | (eom << 6) | ((seq & 0x03U) << 4) | (1U << 3));
        for (p = 0U; p < frag; p++)
            pkt[8U + p] = pMsg[off + p];
        pkt[8U + frag] = mctp_calc_pec(pkt, 8U + frag);

        if (s_mctpBus == 0U)
            I2C_WriteMultiBytes(I2C1, pkt[0] >> 1, &pkt[1], 8U + frag);
        else
            UI2C_WriteMultiBytes(UI2C0, pkt[0] >> 1, &pkt[1], 8U + frag);

        off = (uint16_t)(off + frag);
        seq = (uint8_t)((seq + 1U) & 0x03U);
        if (off < msgLen)
            CLK_SysTickDelay(2000);
    }
}

/* Receive and reassemble one MCTP message, verifying PEC per packet and MIC. */
static int32_t mctp_rx_message_core(uint8_t *pMsg, uint16_t maxLen, uint16_t *pMsgLen)
{
    uint8_t  pkt[MCTP_RXQ_PKT_SIZE + 1U];
    uint16_t len = 0U;
    uint8_t  started = 0U;
    uint8_t  expSeq = 0U;
    uint32_t waitCnt = 0U;

    if ((pMsg == NULL) || (pMsgLen == NULL))
        return -1;
    *pMsgLen = 0U;

    for (;;)
    {
        uint8_t rawLen;
        uint8_t bc;
        uint8_t tag;
        uint8_t som;
        uint8_t eom;
        uint8_t seq;
        uint8_t payLen;

        if (s_mctpRxqRd == s_mctpRxqWr)
        {
            if (++waitCnt > MCTP_RX_TIMEOUT_CNT)
                return -2;
            continue;
        }
        waitCnt = 0U;

        rawLen = s_mctpRxqLen[s_mctpRxqRd];
        if (rawLen > MCTP_RXQ_PKT_SIZE)
        {
            s_mctpRxqRd = (uint8_t)((s_mctpRxqRd + 1U) % MCTP_RXQ_DEPTH);
            return -3;
        }
        /* pkt[0] = MCU write address (part of PEC), pkt[1..] = received bytes. */
        pkt[0] = (uint8_t)(MCU_ADDR_7BIT << 1);
        memcpy(&pkt[1], s_mctpRxqPkt[s_mctpRxqRd], rawLen);
        s_mctpRxqRd = (uint8_t)((s_mctpRxqRd + 1U) % MCTP_RXQ_DEPTH);

        bc = pkt[2];
        if ((pkt[1] != 0x0FU) || (bc < 6U) || ((uint16_t)rawLen < ((uint16_t)bc + 3U)))
            return -4;
        if (mctp_calc_pec(pkt, (size_t)bc + 3U) != pkt[3U + bc])
            return -5;

        tag    = pkt[7];
        som    = (uint8_t)((tag >> 7) & 0x01U);
        eom    = (uint8_t)((tag >> 6) & 0x01U);
        seq    = (uint8_t)((tag >> 4) & 0x03U);
        payLen = (uint8_t)(bc - 5U);

        if (started == 0U)
        {
            if (som == 0U)
                return -6;
            started = 1U;
        }
        else
        {
            if (som != 0U)
                return -6;
            if (seq != expSeq)
                return -7;
        }

        if (((uint32_t)len + payLen) > maxLen)
            return -8;
        memcpy(&pMsg[len], &pkt[8], payLen);
        len = (uint16_t)(len + payLen);
        expSeq = (uint8_t)((seq + 1U) & 0x03U);

        if (eom != 0U)
            break;
    }

    /* MIC = CRC-32C over the whole message except the trailing 4 bytes. */
    if ((pMsg[0] & 0x80U) != 0U)
    {
        uint32_t micCalc;
        uint32_t micRx;

        if (len < 5U)
            return -9;
        micCalc = mctp_calc_mic(pMsg, (size_t)len - 4U);
        micRx   = (uint32_t)pMsg[len - 4U]
                | ((uint32_t)pMsg[len - 3U] << 8)
                | ((uint32_t)pMsg[len - 2U] << 16)
                | ((uint32_t)pMsg[len - 1U] << 24);
        if (micCalc != micRx)
            return -9;
    }

    *pMsgLen = len;
    return 0;
}

/* Wrapper that drains stale packets after a framing error so they do not
 * pollute the following command. */
static int32_t mctp_rx_message(uint8_t *pMsg, uint16_t maxLen, uint16_t *pMsgLen)
{
    int32_t ret = mctp_rx_message_core(pMsg, maxLen, pMsgLen);

    if (ret <= -3)
    {
        uint32_t idleCnt = 0U;

        while (idleCnt <= MCTP_RX_TIMEOUT_CNT)
        {
            if (s_mctpRxqRd != s_mctpRxqWr)
            {
                s_mctpRxqRd = (uint8_t)((s_mctpRxqRd + 1U) % MCTP_RXQ_DEPTH);
                idleCnt = 0U;
            }
            else
            {
                idleCnt++;
            }
        }
    }
    return ret;
}

/* Configure the current MCTP bus (s_mctpBus) as a slave at MCU_ADDR_7BIT and
 * install the matching RX callback, ready to receive right after the request is
 * transmitted. Call before mctp_tx_message(). */
static void mctp_bus_prepare(void)
{
    s_mctpRxqRd = s_mctpRxqWr;
    if (s_mctpBus == 0U)
    {
        I2C_SetSlaveAddr(I2C1, 0, MCU_ADDR_7BIT, I2C_GCMODE_DISABLE);
        s_mctpI2C1Handler = Mctp_I2C1_SlaveTRx;
    }
    else
    {
        UI2C_SetSlaveAddr(UI2C0, 0, MCU_ADDR_7BIT, UI2C_GCMODE_DISABLE);
        s_mctpUi2cData = 0U;
        s_mctpUI2C0Handler = Mctp_UI2C0_SlaveTRx;
    }
}

/* Enable the slave interrupt on the current MCTP bus and arm ACK so the drive's
 * response is captured. Call after mctp_tx_message(), before mctp_rx_message(). */
static void mctp_bus_listen(void)
{
    s_mctpRxFlag = 0;
    if (s_mctpBus == 0U)
    {
        I2C_EnableInt(I2C1);
        NVIC_EnableIRQ(I2C1_IRQn);
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    }
    else
    {
        UI2C_ENABLE_PROT_INT(UI2C0, (UI2C_PROTIEN_ACKIEN_Msk | UI2C_PROTIEN_NACKIEN_Msk |
                                     UI2C_PROTIEN_STORIEN_Msk | UI2C_PROTIEN_STARIEN_Msk));
        NVIC_EnableIRQ(USCI0_IRQn);
        UI2C_SET_CONTROL_REG(UI2C0, (UI2C_CTL_PTRG | UI2C_CTL_AA));
    }
}

/* Disable the slave interrupt on the current MCTP bus and detach the callback,
 * leaving the peripheral free for its normal master use (mux + polling). */
static void mctp_bus_end(void)
{
    if (s_mctpBus == 0U)
    {
        I2C_DisableInt(I2C1);
        NVIC_DisableIRQ(I2C1_IRQn);
        s_mctpI2C1Handler = NULL;
    }
    else
    {
        UI2C_DISABLE_PROT_INT(UI2C0, (UI2C_PROTIEN_ACKIEN_Msk | UI2C_PROTIEN_NACKIEN_Msk |
                                      UI2C_PROTIEN_STORIEN_Msk | UI2C_PROTIEN_STARIEN_Msk));
        NVIC_DisableIRQ(USCI0_IRQn);
        s_mctpUI2C0Handler = NULL;
    }
}

/* Send a 16-byte NVMe-MI Command request (NMH + opcode + DWORD0 + DWORD1),
 * append the MIC, and receive the (possibly multi-packet) response. */
static int32_t nvme_mi_cmd_xfer(const uint8_t *pReq, uint8_t *pResp, uint16_t maxLen, uint16_t *pRespLen)
{
    uint8_t  msg[20];
    uint32_t mic;
    int32_t  ret;

    if ((pReq == NULL) || (pResp == NULL) || (pRespLen == NULL))
        return -1;

    memcpy(msg, pReq, 16);
    mic = mctp_calc_mic(msg, 16);
    msg[16] = (uint8_t)(mic & 0xFF);
    msg[17] = (uint8_t)((mic >> 8) & 0xFF);
    msg[18] = (uint8_t)((mic >> 16) & 0xFF);
    msg[19] = (uint8_t)((mic >> 24) & 0xFF);

    mctp_bus_prepare();
    mctp_tx_message(msg, (uint16_t)sizeof(msg));
    mctp_bus_listen();
    ret = mctp_rx_message(pResp, maxLen, pRespLen);
    mctp_bus_end();
    return ret;
}

/* Route the mux(es) to the NVMe drive at the given GLOBAL slot index and select
 * the bus for the following MCTP exchange.
 *
 * The global slot index is a flat, 0-based number across both CPLD banks, laid
 * out the same way the host CLI / Basic Management report present the drives:
 *   [0 .. bank0Count-1]           -> bank 0, I2C1  (first CPLD)
 *   [bank0Count .. total-1]       -> bank 1, UI2C0 (second CPLD)
 * Within each bank the local slot maps as: localSlot / 8 selects the PCA9848/
 * TCA9548 mux, localSlot % 8 the channel (mirrors nvm_mi_read/_1()).
 *
 * Side effect: sets s_mctpBus so mctp_bus_prepare/listen/end and mctp_tx_message
 * operate on the correct peripheral. */
static uint8_t SelectNvmeSlotChannel(uint8_t u8SlotIndex)
{
    const uint8_t u8MuxCount = sizeof(s_au8_PCA9848_Addr) / sizeof(s_au8_PCA9848_Addr[0]);
    const uint8_t u8ChannelsPerMux = 8U;
    uint8_t u8Bank0Count = bmc_report[cpld_hdd_amount];
    uint8_t u8LocalSlot;
    uint8_t u8MuxIndex;
    uint8_t u8ChannelOnMux;

    if (u8Bank0Count == 0xFFU)               /* CPLD not read yet / invalid */
        u8Bank0Count = 0U;

    if (u8SlotIndex < u8Bank0Count)
    {
        /* Bank 0: first CPLD's drives on I2C1. */
        s_mctpBus   = 0U;
        u8LocalSlot = u8SlotIndex;
    }
    else
    {
        /* Bank 1: second CPLD's drives on UI2C0. Only valid when the second
         * CPLD is present (UI2C0 pins + UI2C0_Init done in main.c). */
        if (g_u32clpd_lost != 0U)
            return 0;

        s_mctpBus   = 1U;
        u8LocalSlot = (uint8_t)(u8SlotIndex - u8Bank0Count);
    }

    u8MuxIndex     = u8LocalSlot / u8ChannelsPerMux;
    u8ChannelOnMux = u8LocalSlot % u8ChannelsPerMux;

    if (u8MuxIndex >= u8MuxCount)
        return 0;

    if (s_mctpBus == 0U)
    {
        if (mux_TCA9548_flag == 0)
        {
            DisableMux(I2C1, s_au8_PCA9848_Addr[0]);
            DisableMux(I2C1, s_au8_PCA9848_Addr[1]);
            DisableMux(I2C1, s_au8_PCA9848_Addr[2]);
            return SelectMuxChannel(I2C1, s_au8_PCA9848_Addr[u8MuxIndex], u8ChannelOnMux);
        }
        else
        {
            DisableMux(I2C1, s_au8_TCA9548_Addr[0]);
            return SelectMuxChannel(I2C1, s_au8_TCA9548_Addr[u8MuxIndex], u8ChannelOnMux);
        }
    }
    else
    {
        if (mux_TCA9548_flag == 0)
        {
            DisableMux_1(UI2C0, s_au8_PCA9848_Addr[0]);
            DisableMux_1(UI2C0, s_au8_PCA9848_Addr[1]);
            DisableMux_1(UI2C0, s_au8_PCA9848_Addr[2]);
            return SelectMuxChannel_1(UI2C0, s_au8_PCA9848_Addr[u8MuxIndex], u8ChannelOnMux);
        }
        else
        {
            DisableMux_1(UI2C0, s_au8_TCA9548_Addr[0]);
            return SelectMuxChannel_1(UI2C0, s_au8_TCA9548_Addr[u8MuxIndex], u8ChannelOnMux);
        }
    }
}

/**
 * @brief Execute one native NVMe-MI command against the drive in a slot.
 *
 * Builds an NVMe-MI Command request (NMH + opcode + Management Request
 * DWORD0/DWORD1), performs the MCTP request/response exchange over I2C1,
 * and verifies PEC (per packet) and MIC (whole message).
 *
 * @param[in]  u8SlotIndex  NVMe slot / drive index (0-based).
 * @param[in]  u8Opcode     NVMe-MI Command Set opcode (0x00 = Read MI Data
 *                          Structure, 0x01 = NVM Subsystem Health Status Poll,
 *                          0x02 = Controller Health Status Poll,
 *                          0x04 = Configuration Get, 0x05 = VPD Read).
 * @param[in]  u32Dword0    NVMe Management Request DWORD0 (little-endian on wire).
 * @param[in]  u32Dword1    NVMe Management Request DWORD1 (little-endian on wire).
 * @param[out] pu8Out       Buffer for the response payload (Mgmt Response DWORD
 *                          followed by any response data, i.e. resp[5 .. end-MIC)).
 * @param[in]  u16MaxOut    Size of pu8Out.
 * @param[out] pu16OutLen   Number of payload bytes written to pu8Out.
 * @param[out] pu8MiStatus  NVMe-MI Status byte from the response (0 = Success).
 * @retval 0   Exchange completed (inspect *pu8MiStatus for command status).
 * @retval <0  Transport error: -100 mux fail, -101 short response, or the
 *             negative code returned by the MCTP receive path.
 */
int32_t nvme_mi_native_cmd(uint8_t u8SlotIndex, uint8_t u8Opcode,
                           uint32_t u32Dword0, uint32_t u32Dword1,
                           uint8_t *pu8Out, uint16_t u16MaxOut,
                           uint16_t *pu16OutLen, uint8_t *pu8MiStatus)
{
    uint8_t  req[16] = { 0x84, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t  resp[256];
    uint16_t rlen = 0U;
    int32_t  ret;

    if ((pu8Out == NULL) || (pu16OutLen == NULL) || (pu8MiStatus == NULL))
        return -1;

    *pu16OutLen  = 0U;
    *pu8MiStatus = 0xFFU;

    req[4]  = u8Opcode;
    req[8]  = (uint8_t)(u32Dword0 & 0xFF);
    req[9]  = (uint8_t)((u32Dword0 >> 8) & 0xFF);
    req[10] = (uint8_t)((u32Dword0 >> 16) & 0xFF);
    req[11] = (uint8_t)((u32Dword0 >> 24) & 0xFF);
    req[12] = (uint8_t)(u32Dword1 & 0xFF);
    req[13] = (uint8_t)((u32Dword1 >> 8) & 0xFF);
    req[14] = (uint8_t)((u32Dword1 >> 16) & 0xFF);
    req[15] = (uint8_t)((u32Dword1 >> 24) & 0xFF);

    /* Enable the NVMe I2C bus and route the mux to the requested drive slot. */
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    HWM_SEL = 0;

    if (!SelectNvmeSlotChannel(u8SlotIndex))
    {
        HWM_SEL = 1;
        return -100;
    }

    /* The USB command dispatcher in main.c runs this handler inside a critical
     * section with PRIMASK=1 (all interrupts masked). The MCTP response from
     * the drive is received via the I2C1 slave interrupt, which cannot fire
     * while interrupts are masked - that would make the receive always time
     * out. Temporarily unmask interrupts for the duration of the exchange, then
     * restore the caller's PRIMASK so the surrounding critical section is
     * preserved. */
    {
        uint32_t u32PriMask = __get_PRIMASK();
        __set_PRIMASK(0);

        ret = nvme_mi_cmd_xfer(req, resp, (uint16_t)sizeof(resp), &rlen);

        __set_PRIMASK(u32PriMask);
    }

    HWM_SEL = 1;

    if (ret != 0)
        return ret;
    if (rlen < 9U)          /* NMH(4) + Status(1) + >=0 payload + MIC(4) */
        return -101;

    *pu8MiStatus = resp[4];

    /* Return the NVMe Management Response DWORD + any response data:
     * resp[5 .. rlen-4), i.e. skip the 4-byte NMH header + 1-byte Status and
     * drop the trailing 4-byte MIC. */
    {
        uint16_t payLen = (uint16_t)(rlen - 9U);
        uint16_t n      = (payLen > u16MaxOut) ? u16MaxOut : payLen;
        uint16_t k;

        for (k = 0U; k < n; k++)
            pu8Out[k] = resp[5U + k];
        *pu16OutLen = n;
    }

    return 0;
}

/* Send a 68-byte NVMe Admin request (NMH(4) + 64-byte SQE), append the MIC at
 * bytes [68..71], and receive the (possibly large multi-packet) response. The
 * caller must provide a 72-byte message buffer with [0..67] populated. */
static int32_t nvme_mi_admin_xfer(uint8_t *pMsg, uint8_t *pResp, uint16_t maxLen, uint16_t *pRespLen)
{
    uint32_t mic;
    int32_t  ret;

    if ((pMsg == NULL) || (pResp == NULL) || (pRespLen == NULL))
        return -1;

    mic = mctp_calc_mic(pMsg, 68);
    pMsg[68] = (uint8_t)(mic & 0xFF);
    pMsg[69] = (uint8_t)((mic >> 8) & 0xFF);
    pMsg[70] = (uint8_t)((mic >> 16) & 0xFF);
    pMsg[71] = (uint8_t)((mic >> 24) & 0xFF);

    mctp_bus_prepare();
    mctp_tx_message(pMsg, 72);
    mctp_bus_listen();
    ret = mctp_rx_message(pResp, maxLen, pRespLen);
    mctp_bus_end();
    return ret;
}

/**
 * @brief Execute one NVMe Admin passthrough command over NVMe-MI (NMIMT=2h).
 *
 * Builds an NVMe-MI "NVMe Admin Command" request: NMH(4) + 64-byte Submission
 * Queue Entry (SQE) + MIC(4). The SQE carries the Admin opcode, NSID, the
 * NVMe-MI transfer Data Length (SQE Dword 7) and Command Dword 10/11. The full
 * response (NMH + 16-byte response header + data + MIC) is received and
 * reassembled via the MCTP receive path; a window of the returned data is then
 * copied out, which lets the host page through logs larger than one USB frame.
 *
 * @param[in]  u8SlotIndex  NVMe slot / drive index (0-based).
 * @param[in]  u8Opcode     NVMe Admin opcode (0x02 = Get Log Page, 0x06 = Identify).
 * @param[in]  u32Nsid      Namespace Identifier (SQE Dword 1).
 * @param[in]  u32Cdw10     Command Dword 10 (e.g. LID/RAE/NUMDL or CNS).
 * @param[in]  u32Cdw11     Command Dword 11 (e.g. NUMDU / log-specific).
 * @param[in]  u32DataLen   NVMe-MI transfer Data Length in bytes (SQE Dword 7).
 * @param[in]  u16OutOffset Byte offset into the returned data (for host paging).
 * @param[out] pu8Out       Buffer for the response data window.
 * @param[in]  u16MaxOut    Size of pu8Out (max bytes returned per call).
 * @param[out] pu16OutLen   Number of bytes written to pu8Out.
 * @param[out] pu16TotalLen Total data bytes available (before offset/clamp);
 *                          may be NULL if not needed.
 * @param[out] pu8MiStatus  NVMe-MI Status byte from the response (0 = Success).
 * @retval 0   Exchange completed (inspect *pu8MiStatus for command status).
 * @retval <0  Transport error: -100 mux fail, -101 short response, or the
 *             negative code returned by the MCTP receive path.
 */
int32_t nvme_mi_admin_cmd(uint8_t u8SlotIndex, uint8_t u8Opcode, uint32_t u32Nsid,
                          uint32_t u32Cdw10, uint32_t u32Cdw11, uint32_t u32DataLen,
                          uint16_t u16OutOffset, uint8_t *pu8Out, uint16_t u16MaxOut,
                          uint16_t *pu16OutLen, uint16_t *pu16TotalLen, uint8_t *pu8MiStatus)
{
    /* Large static buffer: Identify Controller returns NMH(4)+hdr(16)+4096+MIC(4)
     * = 4120 bytes. Static (not on the stack) to avoid stack overflow. */
    static uint8_t s_adminResp[4224];
    uint8_t  msg[72];
    uint16_t rlen = 0U;
    int32_t  ret;
    uint16_t j;

    if ((pu8Out == NULL) || (pu16OutLen == NULL) || (pu8MiStatus == NULL))
        return -1;

    *pu16OutLen  = 0U;
    if (pu16TotalLen != NULL)
        *pu16TotalLen = 0U;
    *pu8MiStatus = 0xFFU;

    for (j = 0U; j < (uint16_t)sizeof(msg); j++)
        msg[j] = 0U;

    msg[0]  = 0x84;                              /* NMH0: IC bit + NVMe-MI msg type */
    msg[1]  = 0x10;                              /* NMH1: NMIMT=2h (NVMe Admin Command) */
    msg[4]  = u8Opcode;                          /* SQE Dword 0: Opcode */
    msg[8]  = (uint8_t)(u32Nsid & 0xFF);         /* SQE Dword 1: NSID */
    msg[9]  = (uint8_t)((u32Nsid >> 8) & 0xFF);
    msg[10] = (uint8_t)((u32Nsid >> 16) & 0xFF);
    msg[11] = (uint8_t)((u32Nsid >> 24) & 0xFF);
    msg[32] = (uint8_t)(u32DataLen & 0xFF);      /* SQE Dword 7: Data Length (bytes) */
    msg[33] = (uint8_t)((u32DataLen >> 8) & 0xFF);
    msg[34] = (uint8_t)((u32DataLen >> 16) & 0xFF);
    msg[35] = (uint8_t)((u32DataLen >> 24) & 0xFF);
    msg[44] = (uint8_t)(u32Cdw10 & 0xFF);        /* SQE Dword 10 */
    msg[45] = (uint8_t)((u32Cdw10 >> 8) & 0xFF);
    msg[46] = (uint8_t)((u32Cdw10 >> 16) & 0xFF);
    msg[47] = (uint8_t)((u32Cdw10 >> 24) & 0xFF);
    msg[48] = (uint8_t)(u32Cdw11 & 0xFF);        /* SQE Dword 11 */
    msg[49] = (uint8_t)((u32Cdw11 >> 8) & 0xFF);
    msg[50] = (uint8_t)((u32Cdw11 >> 16) & 0xFF);
    msg[51] = (uint8_t)((u32Cdw11 >> 24) & 0xFF);

    /* Enable the NVMe I2C bus and route the mux to the requested drive slot. */
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    HWM_SEL = 0;

    if (!SelectNvmeSlotChannel(u8SlotIndex))
    {
        HWM_SEL = 1;
        return -100;
    }

    /* The 0xE0 dispatcher runs with PRIMASK=1; unmask interrupts around the
     * MCTP exchange so the I2C1 slave interrupt can receive the response, then
     * restore the caller's PRIMASK (see nvme_mi_native_cmd for details). */
    {
        uint32_t u32PriMask = __get_PRIMASK();
        __set_PRIMASK(0);

        ret = nvme_mi_admin_xfer(msg, s_adminResp, (uint16_t)sizeof(s_adminResp), &rlen);

        __set_PRIMASK(u32PriMask);
    }

    HWM_SEL = 1;

    if (ret != 0)
        return ret;
    if (rlen < 24U)     /* NMH(4) + resp hdr(16) + >=0 data + MIC(4) */
        return -101;

    *pu8MiStatus = s_adminResp[4];

    /* Data payload = s_adminResp[20 .. rlen-4); copy the requested window. */
    {
        uint16_t dataLen = (uint16_t)(rlen - 24U);
        uint16_t start   = u16OutOffset;
        uint16_t n;
        uint16_t k;

        if (pu16TotalLen != NULL)
            *pu16TotalLen = dataLen;

        if (start >= dataLen)
        {
            *pu16OutLen = 0U;
            return 0;
        }

        n = (uint16_t)(dataLen - start);
        if (n > u16MaxOut)
            n = u16MaxOut;

        for (k = 0U; k < n; k++)
            pu8Out[k] = s_adminResp[20U + start + k];

        *pu16OutLen = n;
    }

    return 0;
}

/**
 * @brief Execute one MCTP Control command against the drive in a slot.
 *
 * MCTP Control messages use MCTP message type 0x00 and carry no NVMe-MI header
 * or MIC. The message body is 3 bytes: [0]=0x00 (message type), [1]=0x80
 * (Rq=1, Instance ID=0), [2]=Command Code. The response body is
 * [0]=0x00, [1]=Rq/D/Instance, [2]=Command Code, [3]=Completion Code,
 * [4..]=response data. Used for read-only discovery commands:
 *   Command Code 0x03 = Get Endpoint UUID     (16-byte UUID)
 *   Command Code 0x05 = Get Message Type Support (count + supported types)
 *
 * @param[in]  u8SlotIndex   NVMe slot / drive index (0-based).
 * @param[in]  u8CmdCode     MCTP Control command code (0x03 or 0x05).
 * @param[out] pu8Out        Buffer for the response data (resp[4 .. end)).
 * @param[in]  u16MaxOut     Size of pu8Out.
 * @param[out] pu16OutLen    Number of bytes written to pu8Out.
 * @param[out] pu8CompCode   MCTP Control Completion Code (0 = Success).
 * @retval 0   Exchange completed (inspect *pu8CompCode for command status).
 * @retval <0  Transport error: -100 mux fail, -101 short response, or the
 *             negative code returned by the MCTP receive path.
 */
int32_t nvme_mi_mctp_ctrl_cmd(uint8_t u8SlotIndex, uint8_t u8CmdCode,
                              uint8_t *pu8Out, uint16_t u16MaxOut,
                              uint16_t *pu16OutLen, uint8_t *pu8CompCode)
{
    uint8_t  msg[3];
    uint8_t  resp[128];
    uint16_t rlen = 0U;
    int32_t  ret;

    if ((pu8Out == NULL) || (pu16OutLen == NULL) || (pu8CompCode == NULL))
        return -1;

    *pu16OutLen  = 0U;
    *pu8CompCode = 0xFFU;

    msg[0] = 0x00;          /* MCTP message type: Control */
    msg[1] = 0x80;          /* Rq=1, D=0, Instance ID=0 */
    msg[2] = u8CmdCode;     /* 0x03 = Get Endpoint UUID, 0x05 = Get Msg Type Support */

    /* Enable the NVMe I2C bus and route the mux to the requested drive slot. */
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    HWM_SEL = 0;

    if (!SelectNvmeSlotChannel(u8SlotIndex))
    {
        HWM_SEL = 1;
        return -100;
    }

    /* The 0xE0 dispatcher runs with PRIMASK=1; unmask interrupts around the
     * MCTP exchange so the I2C1 slave interrupt can receive the response, then
     * restore the caller's PRIMASK (see nvme_mi_native_cmd for details). A
     * control message has no MIC, so mctp_rx_message_core skips the MIC check
     * because the response message type byte (resp[0]=0x00) has bit7 clear. */
    {
        uint32_t u32PriMask = __get_PRIMASK();
        __set_PRIMASK(0);

        mctp_bus_prepare();
        mctp_tx_message(msg, (uint16_t)sizeof(msg));
        mctp_bus_listen();
        ret = mctp_rx_message(resp, (uint16_t)sizeof(resp), &rlen);
        mctp_bus_end();

        __set_PRIMASK(u32PriMask);
    }

    HWM_SEL = 1;

    if (ret != 0)
        return ret;
    if (rlen < 4U)          /* MsgType(1) + Rq/Instance(1) + CmdCode(1) + CompCode(1) */
        return -101;

    *pu8CompCode = resp[3];

    /* Return the response data: resp[4 .. rlen). */
    {
        uint16_t payLen = (uint16_t)(rlen - 4U);
        uint16_t n      = (payLen > u16MaxOut) ? u16MaxOut : payLen;
        uint16_t k;

        for (k = 0U; k < n; k++)
            pu8Out[k] = resp[4U + k];
        *pu16OutLen = n;
    }

    return 0;
}

/**
 * @brief Execute an NVMe Admin "Get Features" command over NVMe-MI (NMIMT=2h).
 *
 * Get Features (Admin opcode 0x0A) transfers no data payload; the requested
 * value is returned in the Completion Queue Entry DWORD0 (Command Specific
 * result). This routine builds the 64-byte SQE with CDW10 = FID | (SEL << 8),
 * performs the MCTP exchange, and extracts the CQE DWORD0 (resp[8..11]) plus
 * the NVMe command status (SC/SCT decoded from the CQE DWORD3, resp[16..19]).
 *
 * Example: Power Management (FID 0x02) returns Power State in bits [4:0] and
 * Workload Hint in bits [7:5] of the CQE DWORD0.
 *
 * @param[in]  u8SlotIndex  NVMe slot / drive index (0-based).
 * @param[in]  u8Fid        Feature Identifier (e.g. 0x02 = Power Management).
 * @param[in]  u8Sel        Select field (0 = current, 1 = default, 2 = saved,
 *                          3 = supported capabilities).
 * @param[out] pu32Result   CQE DWORD0 (Command Specific result / feature value).
 * @param[out] pu8MiStatus  NVMe-MI Status byte (resp[4]; 0 = Success).
 * @param[out] pu8Sc        NVMe command Status Code from the CQE (0 = Success);
 *                          may be NULL if not needed.
 * @retval 0   Exchange completed (inspect *pu8MiStatus / *pu8Sc for status).
 * @retval <0  Transport error: -100 mux fail, -101 short response, or the
 *             negative code returned by the MCTP receive path.
 */
int32_t nvme_mi_admin_get_feature(uint8_t u8SlotIndex, uint8_t u8Fid, uint8_t u8Sel,
                                  uint32_t *pu32Result, uint8_t *pu8MiStatus,
                                  uint8_t *pu8Sc)
{
    uint8_t  msg[72];
    uint8_t  resp[64];
    uint16_t rlen = 0U;
    int32_t  ret;
    uint16_t j;

    if ((pu32Result == NULL) || (pu8MiStatus == NULL))
        return -1;

    *pu32Result  = 0U;
    *pu8MiStatus = 0xFFU;
    if (pu8Sc != NULL)
        *pu8Sc = 0xFFU;

    for (j = 0U; j < (uint16_t)sizeof(msg); j++)
        msg[j] = 0U;

    msg[0]  = 0x84;                              /* NMH0: IC bit + NVMe-MI msg type */
    msg[1]  = 0x10;                              /* NMH1: NMIMT=2h (NVMe Admin Command) */
    msg[4]  = 0x0A;                              /* SQE Dword 0: Opcode = Get Features */
    msg[44] = u8Fid;                             /* SQE Dword 10: FID */
    msg[45] = u8Sel;                             /*               SEL (bits [10:8]) */

    /* Enable the NVMe I2C bus and route the mux to the requested drive slot. */
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    HWM_SEL = 0;

    if (!SelectNvmeSlotChannel(u8SlotIndex))
    {
        HWM_SEL = 1;
        return -100;
    }

    /* Unmask interrupts around the MCTP exchange (see nvme_mi_native_cmd). */
    {
        uint32_t u32PriMask = __get_PRIMASK();
        __set_PRIMASK(0);

        ret = nvme_mi_admin_xfer(msg, resp, (uint16_t)sizeof(resp), &rlen);

        __set_PRIMASK(u32PriMask);
    }

    HWM_SEL = 1;

    if (ret != 0)
        return ret;
    if (rlen < 24U)     /* NMH(4) + resp hdr(16) + MIC(4), no data payload */
        return -101;

    *pu8MiStatus = resp[4];

    /* CQE DWORD0 (Command Specific result) at resp[8..11]. */
    *pu32Result = (uint32_t)resp[8]
                | ((uint32_t)resp[9] << 8)
                | ((uint32_t)resp[10] << 16)
                | ((uint32_t)resp[11] << 24);

    /* NVMe command status: SC = bits [24:17], SCT = bits [27:25] of CQE DW3. */
    if (pu8Sc != NULL)
    {
        uint32_t dw3 = (uint32_t)resp[16]
                     | ((uint32_t)resp[17] << 8)
                     | ((uint32_t)resp[18] << 16)
                     | ((uint32_t)resp[19] << 24);
        *pu8Sc = (uint8_t)((dw3 >> 17) & 0xFFU);
    }

    return 0;
}

/**
 * @brief Execute one native NVMe-MI command that carries a write data payload.
 *
 * Like nvme_mi_native_cmd(), but appends @p u16DataLen bytes of data after the
 * 16-byte NVMe-MI Command request and before the MIC (message = NMH + Mgmt
 * Request DWORD0/1 + data + MIC). Used for NVMe-MI Command Set writes such as
 * VPD Write (Opcode 0x06): Dword0 [15:0]=DOFST, Dword1 [7:0]=DLEN, followed by
 * the DLEN data bytes to program.
 *
 * @param[in]  u8SlotIndex  NVMe slot / drive index (0-based).
 * @param[in]  u8Opcode     NVMe-MI Command Set opcode (0x06 = VPD Write).
 * @param[in]  u32Dword0    Management Request DWORD0 (little-endian on wire).
 * @param[in]  u32Dword1    Management Request DWORD1 (little-endian on wire).
 * @param[in]  pu8Data      Write data payload (may be NULL if u16DataLen == 0).
 * @param[in]  u16DataLen   Number of data bytes to append (<= 180).
 * @param[out] pu8Out       Buffer for the response payload (resp[5 .. end-MIC)).
 * @param[in]  u16MaxOut    Size of pu8Out.
 * @param[out] pu16OutLen   Number of payload bytes written to pu8Out.
 * @param[out] pu8MiStatus  NVMe-MI Status byte from the response (0 = Success).
 * @retval 0   Exchange completed (inspect *pu8MiStatus for command status).
 * @retval <0  Transport error: -1 bad args, -100 mux fail, -101 short response,
 *             or the negative code returned by the MCTP receive path.
 */
int32_t nvme_mi_native_write_cmd(uint8_t u8SlotIndex, uint8_t u8Opcode,
                                 uint32_t u32Dword0, uint32_t u32Dword1,
                                 const uint8_t *pu8Data, uint16_t u16DataLen,
                                 uint8_t *pu8Out, uint16_t u16MaxOut,
                                 uint16_t *pu16OutLen, uint8_t *pu8MiStatus)
{
    /* 16-byte request header + up to 180 data bytes + 4-byte MIC. */
    uint8_t  msg[16 + 180 + 4];
    uint8_t  resp[256];
    uint16_t msgLen;
    uint16_t rlen = 0U;
    uint32_t mic;
    int32_t  ret;
    uint16_t k;

    if ((pu8Out == NULL) || (pu16OutLen == NULL) || (pu8MiStatus == NULL))
        return -1;
    if (u16DataLen > 180U)
        return -1;
    if ((u16DataLen > 0U) && (pu8Data == NULL))
        return -1;

    *pu16OutLen  = 0U;
    *pu8MiStatus = 0xFFU;

    for (k = 0U; k < 16U; k++)
        msg[k] = 0U;
    msg[0]  = 0x84;                              /* NMH0: IC bit + NVMe-MI msg type */
    msg[1]  = 0x08;                              /* NMH1: NMIMT=1h (NVMe-MI Command) */
    msg[4]  = u8Opcode;
    msg[8]  = (uint8_t)(u32Dword0 & 0xFF);
    msg[9]  = (uint8_t)((u32Dword0 >> 8) & 0xFF);
    msg[10] = (uint8_t)((u32Dword0 >> 16) & 0xFF);
    msg[11] = (uint8_t)((u32Dword0 >> 24) & 0xFF);
    msg[12] = (uint8_t)(u32Dword1 & 0xFF);
    msg[13] = (uint8_t)((u32Dword1 >> 8) & 0xFF);
    msg[14] = (uint8_t)((u32Dword1 >> 16) & 0xFF);
    msg[15] = (uint8_t)((u32Dword1 >> 24) & 0xFF);

    for (k = 0U; k < u16DataLen; k++)
        msg[16U + k] = pu8Data[k];

    /* MIC = CRC-32C over NMH + DWORDs + data (all bytes before the MIC). */
    mic = mctp_calc_mic(msg, (size_t)(16U + u16DataLen));
    msg[16U + u16DataLen + 0U] = (uint8_t)(mic & 0xFF);
    msg[16U + u16DataLen + 1U] = (uint8_t)((mic >> 8) & 0xFF);
    msg[16U + u16DataLen + 2U] = (uint8_t)((mic >> 16) & 0xFF);
    msg[16U + u16DataLen + 3U] = (uint8_t)((mic >> 24) & 0xFF);
    msgLen = (uint16_t)(16U + u16DataLen + 4U);

    /* Enable the NVMe I2C bus and route the mux to the requested drive slot. */
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    HWM_SEL = 0;

    if (!SelectNvmeSlotChannel(u8SlotIndex))
    {
        HWM_SEL = 1;
        return -100;
    }

    /* Unmask interrupts around the MCTP exchange (see nvme_mi_native_cmd). */
    {
        uint32_t u32PriMask = __get_PRIMASK();
        __set_PRIMASK(0);

        mctp_bus_prepare();
        mctp_tx_message(msg, msgLen);
        mctp_bus_listen();
        ret = mctp_rx_message(resp, (uint16_t)sizeof(resp), &rlen);
        mctp_bus_end();

        __set_PRIMASK(u32PriMask);
    }

    HWM_SEL = 1;

    if (ret != 0)
        return ret;
    if (rlen < 9U)          /* NMH(4) + Status(1) + >=0 payload + MIC(4) */
        return -101;

    *pu8MiStatus = resp[4];

    {
        uint16_t payLen = (uint16_t)(rlen - 9U);
        uint16_t n      = (payLen > u16MaxOut) ? u16MaxOut : payLen;

        for (k = 0U; k < n; k++)
            pu8Out[k] = resp[5U + k];
        *pu16OutLen = n;
    }

    return 0;
}

/**
 * @brief Execute an NVMe Admin Format NVM command over NVMe-MI (NMIMT=2h).
 *
 * !!! DESTRUCTIVE: erases all user data on the targeted namespace(s). !!!
 * Format NVM (Admin opcode 0x80) transfers no data; the result is a CQE. The
 * operation can take much longer than a normal command, so the MCTP receive is
 * retried while it reports "no packet yet" (-2). The NVMe command status
 * (SC/SCT) is decoded from the CQE DWORD3 (resp[16..19]).
 *
 * @param[in]  u8SlotIndex  NVMe slot / drive index (0-based).
 * @param[in]  u32Nsid      Namespace Identifier (0xFFFFFFFF = all namespaces).
 * @param[in]  u32Cdw10     Command Dword 10 (LBAF/MSET/PI/PIL/SES).
 * @param[out] pu8MiStatus  NVMe-MI Status byte (resp[4]; 0 = Success).
 * @param[out] pu8Sc        NVMe command Status Code from the CQE (0 = Success);
 *                          may be NULL if not needed.
 * @retval 0   Exchange completed (inspect *pu8MiStatus / *pu8Sc for status).
 * @retval <0  Transport error: -100 mux fail, -101 short response, or the
 *             negative code returned by the MCTP receive path.
 */
int32_t nvme_mi_admin_format_nvm(uint8_t u8SlotIndex, uint32_t u32Nsid,
                                 uint32_t u32Cdw10, uint8_t *pu8MiStatus, uint8_t *pu8Sc)
{
    uint8_t  msg[72];
    uint8_t  resp[64];
    uint16_t rlen = 0U;
    int32_t  ret;
    uint16_t j;

    if (pu8MiStatus == NULL)
        return -1;

    *pu8MiStatus = 0xFFU;
    if (pu8Sc != NULL)
        *pu8Sc = 0xFFU;

    for (j = 0U; j < (uint16_t)sizeof(msg); j++)
        msg[j] = 0U;

    msg[0]  = 0x84;                              /* NMH0: IC bit + NVMe-MI msg type */
    msg[1]  = 0x10;                              /* NMH1: NMIMT=2h (NVMe Admin Command) */
    msg[4]  = 0x80;                              /* SQE Dword 0: Opcode = Format NVM */
    msg[8]  = (uint8_t)(u32Nsid & 0xFF);         /* SQE Dword 1: NSID */
    msg[9]  = (uint8_t)((u32Nsid >> 8) & 0xFF);
    msg[10] = (uint8_t)((u32Nsid >> 16) & 0xFF);
    msg[11] = (uint8_t)((u32Nsid >> 24) & 0xFF);
    msg[44] = (uint8_t)(u32Cdw10 & 0xFF);        /* SQE Dword 10: LBAF/MSET/PI/PIL/SES */
    msg[45] = (uint8_t)((u32Cdw10 >> 8) & 0xFF);
    msg[46] = (uint8_t)((u32Cdw10 >> 16) & 0xFF);
    msg[47] = (uint8_t)((u32Cdw10 >> 24) & 0xFF);

    /* Enable the NVMe I2C bus and route the mux to the requested drive slot. */
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    HWM_SEL = 0;

    if (!SelectNvmeSlotChannel(u8SlotIndex))
    {
        HWM_SEL = 1;
        return -100;
    }

    /* Unmask interrupts around the MCTP exchange (see nvme_mi_native_cmd).
     * Compute + append the MIC, send, then retry the receive because Format can
     * take longer than a single MCTP RX timeout window. */
    {
        uint32_t u32PriMask = __get_PRIMASK();
        uint8_t  retry;
        uint32_t mic = mctp_calc_mic(msg, 68);

        msg[68] = (uint8_t)(mic & 0xFF);
        msg[69] = (uint8_t)((mic >> 8) & 0xFF);
        msg[70] = (uint8_t)((mic >> 16) & 0xFF);
        msg[71] = (uint8_t)((mic >> 24) & 0xFF);

        __set_PRIMASK(0);

        mctp_bus_prepare();
        mctp_tx_message(msg, 72);
        mctp_bus_listen();

        ret = -2;
        for (retry = 0U; (retry < 20U) && (ret == -2); retry++)
            ret = mctp_rx_message(resp, (uint16_t)sizeof(resp), &rlen);

        mctp_bus_end();

        __set_PRIMASK(u32PriMask);
    }

    HWM_SEL = 1;

    if (ret != 0)
        return ret;
    /* Need at least NMH(4) + MI Status(1) to report a status. An error response
     * (MI Status != 0) is often shorter than a full CQE, so only require the
     * status byte here; decode the CQE Status Code only when the response is
     * long enough to contain CQE DWORD3 (resp[16..19]). */
    if (rlen < 5U)
        return -101;

    *pu8MiStatus = resp[4];

    if ((pu8Sc != NULL) && (rlen >= 20U))
    {
        uint32_t dw3 = (uint32_t)resp[16]
                     | ((uint32_t)resp[17] << 8)
                     | ((uint32_t)resp[18] << 16)
                     | ((uint32_t)resp[19] << 24);
        *pu8Sc = (uint8_t)((dw3 >> 17) & 0xFFU);
    }
    else if (pu8Sc != NULL)
    {
        /* Short response: no CQE DW3 present. If MI Status is Success, treat SC
         * as 0 (Format accepted); otherwise leave it as 0xFF so the caller
         * reports the MI Status as the failure reason. */
        *pu8Sc = (resp[4] == 0x00U) ? 0x00U : 0xFFU;
    }

    return 0;
}
