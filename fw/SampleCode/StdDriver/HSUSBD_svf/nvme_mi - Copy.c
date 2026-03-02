#include <stdio.h>
#include "NuMicro.h"
#include "hid_transfer.h"
#include <string.h>
#include "g_def.h"
void nvm_mi_read(void)
{
    unsigned char temp_buf[32]; // Temporary buffer to hold data read from a single NVMe drive.
    // Set a hardware selection pin (HWM_SEL) to low, likely to enable the I2C bus for NVMe drives.
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    PA9 = 0; // HWM_SEL

    unsigned char local_cnt = 0, data_len = 0, i = 0;
    unsigned char read_success = 0; // Flag to indicate if a read from the current slot was successful.

    // Loop through each installed HDD slot as reported by the CPLD.
    for (local_cnt = 0; local_cnt < bmc_report[cpld_hdd_amount]; local_cnt++)
    {
        read_success = 0; // Reset the success flag for each new slot.

        // Select the I2C channel for the current NVMe slot using the TCA9548 I2C multiplexer.
        if (I2C_WriteByte(I2C1, TCA9548, (0x01 << local_cnt)) != 0)
        {
            continue; // If selecting the MUX channel fails, skip to the next slot.
        }

        // --- Attempt to read from the first possible NVMe-MI address (TP1) ---
        if (I2C_WriteByte(I2C1, NVME_TP1_ADDR, NVME_READ_REG) == 0)
        {
            data_len = I2C_ReadMultiBytes(I2C1, NVME_TP1_ADDR, temp_buf, NVME_READ_COUNT);

            if (NVME_READ_COUNT == data_len)
            {
                read_success = 1; // Read was successful.
            }
        }

        // --- If TP1 failed, attempt to read from the second possible address (TP2) ---
        if (read_success == 0)
        {
            if (I2C_WriteByte(I2C1, NVME_TP2_ADDR, NVME_READ_REG) == 0)
            {
                data_len = I2C_ReadMultiBytes(I2C1, NVME_TP2_ADDR, temp_buf, NVME_READ_COUNT);

                if (NVME_READ_COUNT == data_len)
                {
                    read_success = 1; // Read was successful.
                }
            }
        }

        // --- If TP1 and TP2 failed, attempt to read from the third possible address (TP3) ---
        if (read_success == 0)
        {
            if (I2C_WriteByte(I2C1, NVME_TP3_ADDR, NVME_READ_REG) == 0)
            {
                data_len = I2C_ReadMultiBytes(I2C1, NVME_TP3_ADDR, temp_buf, NVME_READ_COUNT);

                if (NVME_READ_COUNT == data_len)
                {
                    read_success = 1; // Read was successful.
                }
            }
        }

        // --- If the read was successful from any address, copy the data to the main report buffer ---
        if (read_success == 1)
        {
            for (i = 0; i < NVME_READ_COUNT; i++)
            {
                // Calculate the correct offset in the bmc_report buffer for the current slot and copy the data.
                bmc_report[NVME_MEM_OFFSET + (local_cnt * NVME_READ_COUNT) + i] = temp_buf[i];
            }
        }
        else
        {
            // (Optional) If the read failed for this slot, the corresponding buffer area could be cleared or filled with 0xFF.
        }
    }

    // Set the hardware selection pin back to high, disabling the NVMe I2C bus.
    PA9 = 1; // HWM_SEL set 1
}
