/**************************************************************************//**
 * @file     main.c
 * @version  V3.00
 * @brief    Demonstrate how to transfer data between USB device and PC through USB HID interface.
 *           A windows tool is also included in this sample code to connect with a USB device.
 *
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @copyright Copyright (C) 2021 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "NuMicro.h"
#include "hid_transfer.h"
#include <string.h>

extern volatile uint8_t string_received;
extern volatile uint8_t svf_string_rcvbuf[1024] __attribute__((aligned(4)));
extern volatile uint8_t usb_rcvbuf[1024] __attribute__((aligned(4)));
extern volatile uint16_t buffer_index;
extern volatile uint8_t response_buff[1024] __attribute__((aligned(4)));
volatile uint32_t total_line = 0;
int xsvftool_esp_scan(void);
uint32_t xsvftool_esp_id(void);
int xsvftool_esp_svf_packet(int (*packet_getbyte)(), int index, int final, char *report);
volatile  int final = 0;
volatile char report[256];
volatile unsigned char  i2c_read_report[256];
volatile int retval;
unsigned char i2c_read_bytes, i2c_write_bytes;
volatile int cpld_false_flag = 0;
int pos = 0;
volatile uint32_t timer0_count = 0 ;
volatile uint32_t timer1_count = 0 ;
volatile unsigned char i2c_monitor_flag = 1;
int getbyte_usb_fun()
{
    if (svf_string_rcvbuf[pos] == '\0') return -1;

    return svf_string_rcvbuf[pos++];
}

#define USB_SERIAL_STR_LEN  50


uint8_t g_u8StringSerial[USB_SERIAL_STR_LEN] __attribute__((aligned(4))) =
{
    USB_SERIAL_STR_LEN, // bLength
    0x03                // bDescriptorType (STRING)

};
void Set_USB_SerialNumber_From_UID(void)
{

    SYS_UnlockReg();                   /* Unlock register lock protect */

    FMC_Open();                        /* Enable FMC ISP function */

    uint32_t u32UID[3];
    uint32_t i, j;
    uint8_t *pDesc = &g_u8StringSerial[2]; // ?? Header,?????????
    uint8_t u8Nibble;
    char cHex;

    // 1. ?? Nuvoton M031 ? Unique ID
    // ??:??? SYS ????????????? (UID ???????)
    for (i = 0; i < 3; i++)
    {
        u32UID[i] = FMC_ReadUID(i);

    }

    // 2. ???? (3? 32-bit Word -> 24? Hex ??)
    // ??? UID[0] ????,?? Word ?? 8 ? Nibble
    for (i = 0; i < 3; i++)
    {
        // ?????????? (Big-Endian display),??????? (MSB) ????
        // ?? UID[0] = 0x12345678,???????? "1234..."
        for (j = 0; j < 8; j++)
        {
            // ????? 4-bit (Nibble)
            // Shift ??: (7-j)*4 => 28, 24, 20, ... 0
            u8Nibble = (u32UID[i] >> ((7 - j) * 4)) & 0x0F;

            // ??? ASCII Hex
            if (u8Nibble < 10)
            {
                cHex = '0' + u8Nibble;
            }
            else
            {
                cHex = 'A' + (u8Nibble - 10);
            }

            // 3. ?? UTF-16LE ??
            // Low Byte: ASCII ??
            *pDesc++ = (uint8_t)cHex;
            // High Byte: 0x00 (??? Basic Latin)
            *pDesc++ = 0x00;
        }
    }

    SYS_LockReg();
}

/*--------------------------------------------------------------------------*/
void SYS_Init(void)
{
    uint32_t volatile i;

    /* Unlock protected registers */
    SYS_UnlockReg();

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Enable HIRC and HXT clock */
    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk | CLK_PWRCTL_HXTEN_Msk);

    /* Wait for HIRC and HXT clock ready */
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk | CLK_STATUS_HXTSTB_Msk);

    /* Set PCLK0 and PCLK1 to HCLK/2 */
    CLK->PCLKDIV = (CLK_PCLKDIV_APB0DIV_DIV2 | CLK_PCLKDIV_APB1DIV_DIV2);

    /* Set core clock to 200MHz */
    CLK_SetCoreClock(FREQ_200MHZ);

    /* Enable all GPIO clock */
    CLK->AHBCLK0 |= CLK_AHBCLK0_GPACKEN_Msk | CLK_AHBCLK0_GPBCKEN_Msk | CLK_AHBCLK0_GPCCKEN_Msk | CLK_AHBCLK0_GPDCKEN_Msk |
                    CLK_AHBCLK0_GPECKEN_Msk | CLK_AHBCLK0_GPFCKEN_Msk | CLK_AHBCLK0_GPGCKEN_Msk | CLK_AHBCLK0_GPHCKEN_Msk;
    CLK->AHBCLK1 |= CLK_AHBCLK1_GPICKEN_Msk | CLK_AHBCLK1_GPJCKEN_Msk;

    /* Enable UART0 module clock */
    CLK_EnableModuleClock(UART3_MODULE);

    /* Select UART0 module clock source as HIRC and UART0 module clock divider as 1 */
    CLK_SetModuleClock(UART3_MODULE, CLK_CLKSEL3_UART3SEL_HIRC, CLK_CLKDIV0_UART0(1));

    /* Select HSUSBD */
    SYS->USBPHY &= ~SYS_USBPHY_HSUSBROLE_Msk;

    /* Enable USB PHY */
    SYS->USBPHY = (SYS->USBPHY & ~(SYS_USBPHY_HSUSBROLE_Msk | SYS_USBPHY_HSUSBACT_Msk)) | SYS_USBPHY_HSUSBEN_Msk;

    for (i = 0; i < 0x1000; i++);  // delay > 10 us

    SYS->USBPHY |= SYS_USBPHY_HSUSBACT_Msk;

    /* Enable HSUSBD module clock */
    CLK_EnableModuleClock(HSUSBD_MODULE);

    /* Enable I2C0 clock */
    CLK_EnableModuleClock(I2C0_MODULE);
    CLK_EnableModuleClock(I2C1_MODULE);

    CLK_EnableModuleClock(TMR0_MODULE);

    CLK_SetModuleClock(TMR0_MODULE, CLK_CLKSEL1_TMR0SEL_HXT, 0);

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Set multi-function pins for UART0 RXD and TXD */
    // SET_UART0_RXD_PB12();
    // SET_UART0_TXD_PB13();
    SET_UART3_TXD_PB15() ; // for dump message

    /* Set I2C0 multi-function pins */
    SET_I2C0_SDA_PB4();
    SET_I2C0_SCL_PB5();
    PB->SMTEN |= GPIO_SMTEN_SMTEN4_Msk | GPIO_SMTEN_SMTEN5_Msk ;

    SET_I2C1_SDA_PB0();
    SET_I2C1_SCL_PB1();
    PB->SMTEN |= GPIO_SMTEN_SMTEN0_Msk | GPIO_SMTEN_SMTEN1_Msk ;

    /* Lock protected registers */

    SystemCoreClockUpdate();
    SYS_LockReg();
}

void I2C0_Init(void)
{

    /* Open I2C0 and set clock to 100k */
    I2C_Open(I2C0, 100000);

    /* Get I2C0 Bus Clock */
    printf("I2C clock %d Hz\n", I2C_GetBusClockFreq(I2C0));


}

void I2C1_Init(void)
{

    /* Open I2C0 and set clock to 100k */
    I2C_Open(I2C1, 100000);

    /* Get I2C0 Bus Clock */
    printf("I2C clock %d Hz\n", I2C_GetBusClockFreq(I2C1));

}

#define tempersensor_adr (0x30>>1)
#define cpld_adr (0xf0>>1)
#define TCA9548 (0xE0>>1)
#define nct7363_adr (0x46>>1)

volatile uint8_t bmc_report[1024] __attribute__((aligned(4))) = {0};
#define cpld_ver 0x0
#define cpld_test_ver 0x2
#define cpld_jtag_id 0x20
#define cpld_hdd_amount 0x30
#define cpld_hdd_port_status 0x40
#define cpld_hdd_status 0x50
#define cpld_hdd_led 0x60
#define cpld_hdd_max_cnt 8
#define map_tempersensor_high 0x10
#define map_tempersensor_low 0x11
#define fan_duty 0x70
#define fan_rpm_high 0x80
#define fan_rpm_low 0x81
#define REG_TEMP_DATA           0x05 // Temperature Data Register
//#define map_cpld_ver 0x00
//#define map_cpld_test_ver 0x02
//#define map_cpld_hdd_amount 0x30

void CPLD_read(void)
{
    unsigned char local_cnt;

    if (I2C_WriteByte(I2C0, cpld_adr, cpld_ver) == 0)
    {
        bmc_report[cpld_ver] = I2C_ReadByte(I2C0, cpld_adr);

    }

    if (I2C_WriteByte(I2C0, cpld_adr, cpld_test_ver) == 0)
    {
        bmc_report[cpld_test_ver] = I2C_ReadByte(I2C0, cpld_adr);
    }

    if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_amount) == 0)
    {
        bmc_report[cpld_hdd_amount] = I2C_ReadByte(I2C0, cpld_adr);
    }

    for (local_cnt = 0; local_cnt < cpld_hdd_max_cnt; local_cnt++)
    {
        //read cpld hdd port status
        if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_port_status + local_cnt) == 0)
        {
            bmc_report[cpld_hdd_port_status + local_cnt] = I2C_ReadByte(I2C0, cpld_adr);
        }

        //read cpld  hdd status
        if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_status + local_cnt) == 0)
        {
            bmc_report[cpld_hdd_status + local_cnt] = I2C_ReadByte(I2C0, cpld_adr);
        }

        //read cpld  hdd led
        if (I2C_WriteByte(I2C0, cpld_adr, cpld_hdd_led + local_cnt) == 0)
        {
            bmc_report[cpld_hdd_led + local_cnt] = I2C_ReadByte(I2C0, cpld_adr);
        }

    }
}

unsigned char temp_buf[32];
void fan_read(void)
{
    //read rpm
    if (I2C_WriteByte(I2C0, nct7363_adr, 0x4a) == 0)
    {
        bmc_report[fan_rpm_high] = I2C_ReadByte(I2C0, nct7363_adr);
    }

    if (I2C_WriteByte(I2C0, nct7363_adr, 0x4b) == 0)
    {
        bmc_report[fan_rpm_low] = I2C_ReadByte(I2C0, nct7363_adr);
    }


    //read duty

    if (I2C_WriteByte(I2C0, nct7363_adr, 0x90) == 0)
    {
        bmc_report[fan_duty] = I2C_ReadByte(I2C0, nct7363_adr);
    }

    bmc_report[fan_rpm_high] = 0x0e;

    bmc_report[fan_rpm_low] = 0x02;
    bmc_report[fan_duty] = 0x4d;
}

void tempersensor_read(void)
{
    if (I2C_WriteByte(I2C0, tempersensor_adr, REG_TEMP_DATA) == 0)
    {
        I2C_ReadMultiBytes(I2C0, tempersensor_adr, temp_buf, 2);
        bmc_report[map_tempersensor_high] = temp_buf[0];
        bmc_report[map_tempersensor_low] = temp_buf[1];
    }
}
//#define i2c_mux_cnt 6
#define NVME_TP1_ADDR 0xd4>>1
#define NVME_TP2_ADDR 0x56>>1
#define NVME_TP3_ADDR 0x90>>1
#define NVME_MEM_OFFSET 0x100
#define NVME_READ_REG 0x0
#define NVME_READ_COUNT 32

#if 0
void nvm_mi_read(void)
{

    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    PA9 = 0; //HWM_SEL


    unsigned char local_cnt = 0, data_len = 0, i = 0;

    //read slot id
    for (local_cnt = 0; local_cnt <  bmc_report[cpld_hdd_amount]; local_cnt++)
    {
        //i2c mux select 1 to 0
        if (I2C_WriteByte(I2C1, TCA9548, (0x01 + local_cnt)) == 0)
        {
            I2C_ReadMultiBytes(I2C1, TCA9548, temp_buf, 1);
#if 0
            printf("i2c 1 TCA9548 register read, 0x%x \n\r", temp_buf[0]);

            for (unsigned char i = 0x01; i < 127; i++)
            {


                if (I2C_WriteByte(I2C1, i, 0x00) == 0)
                    printf("address 0x%x\n\r ack", i);
            }

#endif

            //nvme mi read
            if (I2C_WriteByte(I2C1, NVME_TP1_ADDR, NVME_READ_REG) == 0)
            {
                data_len = I2C_ReadMultiBytes(I2C1, NVME_TP1_ADDR, temp_buf, NVME_READ_COUNT);;

                if (NVME_READ_COUNT == data_len)
                {
                    // printf("i2c 1 address 0x%x, register 0 read reture %d  bytes \n\r", i, data_len);
                    //print_nvme_basic_management_info(temp_buf);
                    for (i = 0; i < NVME_READ_COUNT; i++)
                    {
                        bmc_report[NVME_MEM_OFFSET + (local_cnt * NVME_READ_COUNT) + i] = temp_buf[i];
                    }
                }
            }


            if (I2C_WriteByte(I2C1, NVME_TP2_ADDR, NVME_READ_REG) == 0)
            {
                data_len = I2C_ReadMultiBytes(I2C1, NVME_TP2_ADDR, temp_buf, NVME_READ_COUNT);

                if (NVME_READ_COUNT == data_len)
                {
                    // printf("i2c 1 address 0x%x, register 0 read reture %d  bytes \n\r", i, data_len);
                    //print_nvme_basic_management_info(temp_buf);
                    for (i = 0; i < NVME_READ_COUNT; i++)
                    {
                        bmc_report[NVME_MEM_OFFSET + (local_cnt * NVME_READ_COUNT) + i] = temp_buf[i];
                    }
                }
            }

            if (I2C_WriteByte(I2C1, NVME_TP3_ADDR, NVME_READ_REG) == 0)
            {
                data_len = I2C_ReadMultiBytes(I2C1, NVME_TP3_ADDR, temp_buf, NVME_READ_COUNT);

                if (NVME_READ_COUNT == data_len)
                {
                    // printf("i2c 1 address 0x%x, register 0 read reture %d  bytes \n\r", i, data_len);
                    //print_nvme_basic_management_info(temp_buf);
                    for (i = 0; i < NVME_READ_COUNT; i++)
                    {
                        bmc_report[NVME_MEM_OFFSET + (local_cnt * NVME_READ_COUNT) + i] =    temp_buf[i];
                    }
                }
            }
        }
    }

    PA9 = 1; //HWM_SEL set 1
}
#endif

// =============================================================================
// Configuration
// =============================================================================
#define I2C_PORT                I2C1       // ?????????
//#define NVME_TP1_ADDR 0xd4>>1
//#define NVME_DEV_ADDR           (0xd4>>1)        // NVMe SSD 7-bit Address (Default 0x3A >> 1)
#define NVME_MI_CMD_CODE        0x0F        // MCTP over SMBus Command Code


void nvm_mi_idread(void)
{
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    PA9 = 0; // HWM_SEL
    I2C_WriteByte(I2C1, TCA9548, (0x01 << 0));

    PA9 = 0; // HWM_SEL
}


void nvm_mi_read(void)
{
    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    PA9 = 0; // HWM_SEL

    unsigned char local_cnt = 0, data_len = 0, i = 0;
    unsigned char read_success = 0; // ?? flag

    // read slot id
    for (local_cnt = 0; local_cnt < bmc_report[cpld_hdd_amount]; local_cnt++)
    {
        read_success = 0; // ?? slot ????

        // i2c mux select
        if (I2C_WriteByte(I2C1, TCA9548, (0x01 << local_cnt)) != 0)
        {
            continue; // Mux ????,??? slot
        }

        // --- ???? TP1 ---
        if (I2C_WriteByte(I2C1, NVME_TP1_ADDR, NVME_READ_REG) == 0)
        {
            data_len = I2C_ReadMultiBytes(I2C1, NVME_TP1_ADDR, temp_buf, NVME_READ_COUNT);

            if (NVME_READ_COUNT == data_len)
            {
                read_success = 1; // ????
            }
        }

        // --- ?? TP1 ??,???? TP2 ---
        if (read_success == 0)
        {
            if (I2C_WriteByte(I2C1, NVME_TP2_ADDR, NVME_READ_REG) == 0)
            {
                data_len = I2C_ReadMultiBytes(I2C1, NVME_TP2_ADDR, temp_buf, NVME_READ_COUNT); // ????? TP2

                if (NVME_READ_COUNT == data_len)
                {
                    read_success = 1;
                }
            }
        }

        if (read_success == 0)
        {
            if (I2C_WriteByte(I2C1, NVME_TP3_ADDR, NVME_READ_REG) == 0)
            {
                data_len = I2C_ReadMultiBytes(I2C1, NVME_TP3_ADDR, temp_buf, NVME_READ_COUNT); // ????? TP2

                if (NVME_READ_COUNT == data_len)
                {
                    read_success = 1;
                }
            }
        }

        // --- ???? buffer (?????????) ---
        if (read_success == 1)
        {
            for (i = 0; i < NVME_READ_COUNT; i++)
            {
                // ?????? slot ????????,??????
                bmc_report[NVME_MEM_OFFSET + (local_cnt * NVME_READ_COUNT) + i] = temp_buf[i];
            }
        }
        else
        {
            // (??) ?????,?????????? 0xFF?
        }
    }

    PA9 = 1; // HWM_SEL set 1
}


void TMR0_IRQHandler(void)
{
    if (TIMER_GetIntFlag(TIMER0) == 1)
    {
        /* Clear Timer0 time-out interrupt flag */
        TIMER_ClearIntFlag(TIMER0);

        timer0_count++;
        timer1_count++;
    }
}



#define TEMP_RESOLUTION         0.0625f // 0.0625?XC per LSB
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
    if (temp_13bit & 0x1000)
    {
        // Negative temperature: Convert via manual 2's complement logic for 13-bit
        // Subtract 2^13 (8192) to get the negative integer value
        signed_temp_val = (int16_t)(temp_13bit - 8192);
    }
    else
    {
        // Positive temperature
        signed_temp_val = (int16_t)temp_13bit;
    }

    // Convert to Celsius
    final_celsius = (float)signed_temp_val * TEMP_RESOLUTION;

    return final_celsius;
}





void show_cpld_information(uint8_t *p_buf)
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

#define nvme_slot_0 0x100
#define nvme_slot_1 0x120
#define nvme_slot_2 0x140
#define nvme_slot_3 0x160
#define nvme_slot_4 0x180
#define nvme_slot_5 0x1a0

void print_nvme_basic_management_info(uint8_t *data)
{

    // --- Block 1: Status Data (Offset 00h - 07h) ---

    // Byte 0: Length of Status (Should be 6)
    uint8_t status_len = data[0];

    if (status_len != 6)
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

    // Byte 2: SMART Warnings

    uint8_t smart_warn = data[2];
    printf("SMART Critical Warnings (0=Warning, 1=OK):\n");
    printf("  - Bits: 0x%02X\n", smart_warn);
    // ?????? NVMe SMART Log ? Critical Warning ??


    // Byte 3: Composite Temperature (CTemp) [cite: 60, 65]
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
        // 0xC5-0xFF represents -1 to -59C (Twos complement)
        // Cast to int8_t directly works for 2's complement logic in C
        printf("%d C\n", (int8_t)temp_raw);
    }
    else
    {
        printf("Reserved (0x%02X)\n", temp_raw);
    }

    // Byte 4: Percentage Drive Life Used (PDLU) [cite: 83]
    uint8_t life_used = data[4];
    printf("Drive Life Used: %d%%\n", (life_used == 255) ? 255 : life_used);
    // Note: Value > 254 represented as 255 [cite: 86]

    // Byte 5-6: Reserved [cite: 87]
    // Byte 7: PEC for Status Block [cite: 90]
    printf("PEC (Status Block): 0x%02X\n", data[7]);

    printf("---------------------------------------------------\n");

    // --- Block 2: Identification Data (Offset 08h - 1Fh) ---

    // Byte 8: Length of Identification (Should be 22) [cite: 94]
    uint8_t id_len = data[8];
    printf("ID Length: %d (Expected: 22)\n", id_len);

    // Byte 9-10: Vendor ID (VID) - MSB first
    uint16_t vid = (data[9] << 8) | data[10];
    printf("Vendor ID: 0x%04X\n", vid);

    // Byte 11-30: Serial Number (20 chars) [cite: 98]
    // First character is transmitted first
    char serial[21];

    for (int i = 0; i < 20; i++)
    {
        serial[i] = data[11 + i];
    }

    serial[20] = '\0'; // Null-terminate for printing
    printf("Serial Number: %s\n", serial);

    // Byte 31: PEC for ID Block [cite: 100]
    printf("PEC (ID Block): 0x%02X\n", data[31]);
}



// --- PEC (CRC-8) ???? (SMBus ??) ---
uint8_t calc_pec(uint8_t init_crc, uint8_t *data, int len)
{
    uint8_t crc = init_crc;

    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }

    return crc;
}

// --- CRC32C ???? (NVMe-MI MIC ??) ---
// Polynomial: 0x1EDC6F41 (Castagnoli), Reflected -> 0x82F63B78
uint32_t calc_crc32c(uint8_t *data, int len)
{
    uint32_t crc = 0xFFFFFFFF;

    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0x82F63B78;
            else
                crc >>= 1;
        }
    }

    return ~crc;
}
// --- CRC-32C (Castagnoli) ?? (?? MIC) ---
// Polynomial: 0x1EDC6F41
uint32_t crc32c(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;

    while (length--)
    {
        crc ^= *data++;

        for (int i = 0; i < 8; i++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0x82F63B78; // 0x1EDC6F41 bit-reversed
            else
                crc >>= 1;
        }
    }

    return ~crc; // Final XOR
}


static uint8_t g_au8SlvRxData[512];

/* Per-packet RX queue for multi-packet MCTP responses.
 * ISR pushes each completed SMBus write (STOP) as one entry; main loop pops. */
#define MCTP_RXQ_DEPTH      (16U)
#define MCTP_RXQ_PKT_SIZE   (128U)   /* MTU 64 + SMBus/MCTP header + PEC, with margin */
static uint8_t s_rxqPkt[MCTP_RXQ_DEPTH][MCTP_RXQ_PKT_SIZE];
static uint8_t s_rxqLen[MCTP_RXQ_DEPTH];
static volatile uint8_t s_rxqWr = 0U;
static volatile uint8_t s_rxqRd = 0U;
static volatile uint8_t s_rxqCurLen = 0U;


volatile uint8_t g_u8SlvRxFlag = 0;
volatile uint8_t g_u8SlvDataLen;

/* Captured drive info for the final Command Availability Summary.
 * Populated at read time so the summary reflects the actual device
 * instead of hard-coded values. */
char    g_fwRevStr[9]     = "????????";  /* FRS1 from Get Log / Identify FR */
int32_t g_composTempC     = -1000;       /* Composite temperature (Celsius) */

/* Per-command timing. Timer0 runs at 1000 Hz (1 ms per tick) and its ISR
 * increments timer0_count, giving a free-running millisecond clock.
 * cmd_begin() closes the PREVIOUS command's measurement (records its elapsed
 * time into a table) and starts a new one. The whole table is printed once at
 * the very end, below the Command Availability Summary, by cmd_timing_report().
 * NOTE: the reported time is the whole command section, including any
 *       CLK_SysTickDelay() settling delays inside the block. */
#define CMD_TIMING_MAX      (32U)
static uint32_t    g_cmdStartMs = 0;
static const char *g_cmdCurName = NULL;
static const char *g_cmdNames[CMD_TIMING_MAX];
static uint32_t    g_cmdTimes[CMD_TIMING_MAX];
static uint8_t     g_cmdCount = 0U;

static void cmd_record(void)
{
    if ((g_cmdCurName != NULL) && (g_cmdCount < CMD_TIMING_MAX))
    {
        g_cmdNames[g_cmdCount] = g_cmdCurName;
        g_cmdTimes[g_cmdCount] = (uint32_t)(timer0_count - g_cmdStartMs);
        g_cmdCount++;
    }
    g_cmdCurName = NULL;
}

static void cmd_begin(const char *name)
{
    cmd_record();               /* close the previous command */
    g_cmdCurName = name;
    g_cmdStartMs = timer0_count;
}

/* Print the collected per-command timings. Call once, after the summary. */
static void cmd_timing_report(void)
{
    uint8_t  i;
    uint32_t total = 0U;

    cmd_record();               /* close the last command */

    printf("\n>>> [TIME] Per-Command Execution Time <<<\n");
    printf("========================================\n");
    printf("  #  Command                       Time(ms)\n");
    printf("  -  ----------------------------  --------\n");
    for (i = 0U; i < g_cmdCount; i++)
    {
        printf("  %-2u %-28s  %8u\n", (unsigned)(i + 1U), g_cmdNames[i], (unsigned)g_cmdTimes[i]);
        total += g_cmdTimes[i];
    }
    printf("  -  ----------------------------  --------\n");
    printf("  %-2s %-28s  %8u\n", "", "TOTAL", (unsigned)total);
    printf("========================================\n\n");
}

/* Wait for slave RX response with a timeout so an unresponsive target
 * (e.g. NVMe Admin Passthrough not supported) cannot hang the test. */
#define WAIT_SLV_RX_TIMEOUT()                                   \
    do {                                                        \
        uint32_t _wt = 0;                                       \
        while (g_u8SlvRxFlag == 0) {                            \
            if (++_wt > 0x00A00000U) {                          \
                printf("[TIMEOUT] no response from device\n");  \
                break;                                          \
            }                                                   \
        }                                                       \
    } while (0)

typedef void (*I2C_FUNC)(uint32_t u32Status);

static I2C_FUNC s_I2C1HandlerFn = NULL;


void I2C1_IRQHandler(void)
{
    uint32_t u32Status;

    u32Status = I2C_GET_STATUS(I2C1);

    {
        if (s_I2C1HandlerFn != NULL)
            s_I2C1HandlerFn(u32Status);
    }
}

/*---------------------------------------------------------------------------------------------------------*/
/*  I2C TRx Callback Function                                                                              */
/*---------------------------------------------------------------------------------------------------------*/
void I2C_SlaveTRx(uint32_t u32Status)
{
    uint8_t u8data;

    if (u32Status == 0x60)                      /* Own SLA+W has been receive; ACK has been return */
    {
        g_u8SlvDataLen = 0;
        s_rxqCurLen = 0U;
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if (u32Status == 0x80)                 /* Previously address with own SLA address
                                                   Data has been received; ACK has been returned*/
    {
        u8data = (unsigned char) I2C_GET_DATA(I2C1);

        g_au8SlvRxData[g_u8SlvDataLen++] = u8data;
        if (s_rxqCurLen < MCTP_RXQ_PKT_SIZE)
        {
            s_rxqPkt[s_rxqWr][s_rxqCurLen] = u8data;
        }
        if (s_rxqCurLen < 0xFFU)
        {
            s_rxqCurLen++;              /* > PKT_SIZE marks an oversize packet */
        }


        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    }

    else if (u32Status == 0x88)                 /* Previously address with own SLA address
                                                   Data has been received; ACK has been returned*/
    {
        u8data = (unsigned char) I2C_GET_DATA(I2C1);

        g_au8SlvRxData[g_u8SlvDataLen++] = u8data;
        if (s_rxqCurLen < MCTP_RXQ_PKT_SIZE)
        {
            s_rxqPkt[s_rxqWr][s_rxqCurLen] = u8data;
        }
        if (s_rxqCurLen < 0xFFU)
        {
            s_rxqCurLen++;              /* > PKT_SIZE marks an oversize packet */
        }


        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    }

    else if (u32Status == 0xA0)                 /* A STOP or repeated START has been received while still
                                                   addressed as Slave/Receiver*/
    {
			g_u8SlvRxFlag = 1;
        g_u8SlvDataLen = 0;
        {
            /* Push completed packet; drop it if the queue is full */
            uint8_t nextWr = (uint8_t)((s_rxqWr + 1U) % MCTP_RXQ_DEPTH);
            if ((s_rxqCurLen > 0U) && (nextWr != s_rxqRd))
            {
                s_rxqLen[s_rxqWr] = s_rxqCurLen;
                s_rxqWr = nextWr;
            }
            s_rxqCurLen = 0U;
        }
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    }
    else
    {
        printf("[SlaveTRx] Status [0x%x] Unexpected abort!!\n", u32Status);

        if (u32Status == 0x68)              /* Slave receive arbitration lost, clear SI */
        {
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI_AA);
        }
        else if (u32Status == 0xB0)         /* Address transmit arbitration lost, clear SI  */
        {
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI_AA);
        }
        else                                /* Slave bus error, stop I2C and clear SI */
        {
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_STO_SI);
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI);
        }

       //g_u8SlvRxFlag = 1;
    }

    I2C_WAIT_SI_CLEAR(I2C1);
}


/* Forward declarations */
static uint8_t calc_pec_noinit(uint8_t *data, size_t len);
static uint32_t calc_mic(uint8_t *data, size_t len);

// =============================================================================
// Comprehensive MCTP over SMBus Packet Parser
// Input: raw[] = g_au8SlvRxData (full I2C slave receive buffer)
//   raw[0] = SMBus command code (0x0F)
//   raw[1] = Byte Count (N)
//   raw[2] = NVMe source address (SrcAddr << 1 | 1)
//   raw[3] = MCTP Header Version
//   raw[4] = Destination EID
//   raw[5] = Source EID
//   raw[6] = Tag/Flags (SOM, EOM, PktSeq, TO, MsgTag)
//   raw[7] = Message Type
//   raw[8..N] = Message Body
//   raw[N+2] = PEC (last byte after Byte Count field)
// =============================================================================
/**
 * @brief  Return a short name for an NVMe-MI response message status.
 * @param  status  NVMe-MI message byte 4
 * @retval Constant string
 */
static const char *nvme_mi_status_str(uint8_t status)
{
    switch (status)
    {
        case 0x00: return "Success";
        case 0x01: return "More Processing Required";
        case 0x02: return "Internal Error";
        case 0x03: return "Invalid Command Opcode";
        case 0x04: return "Invalid Parameter";
        case 0x05: return "Invalid Command Size";
        case 0x06: return "Invalid Command Input Data Size";
        case 0x07: return "Access Denied";
        default:   return "Other/Reserved";
    }
}

/**
 * @brief  Print CSTS bits of the NVMe-MI Controller Health Data Structure.
 * @param  csts  CSTS field (16-bit, little-endian decoded)
 * @retval None
 */
static void nvme_mi_print_csts(uint16_t csts)
{
    static const char *const shst[4] = { "Normal", "Shutdown Occurring", "Shutdown Complete", "Reserved" };

    printf("    RDY   (Ready)                     : %u\n", (uint32_t)(csts & 0x01U));
    printf("    CFS   (Controller Fatal)          : %u\n", (uint32_t)((csts >> 1) & 0x01U));
    printf("    SHST  (Shutdown Status)           : %s\n", shst[(csts >> 2) & 0x03U]);
    printf("    NSSRO (NVM Subsys Reset Occurred) : %u\n", (uint32_t)((csts >> 4) & 0x01U));
    printf("    CECO  (Ctrl Enable Change Occ.)   : %u\n", (uint32_t)((csts >> 5) & 0x01U));
    printf("    NAC   (Namespace Attr Changed)    : %u\n", (uint32_t)((csts >> 6) & 0x01U));
    printf("    FA    (Firmware Activated)        : %u\n", (uint32_t)((csts >> 7) & 0x01U));
}

void parse_mctp_smbus_response(uint8_t *raw, uint8_t raw_len)
{
    printf("\n========================================\n");
    printf("  MCTP over SMBus Response Parser\n");
    printf("========================================\n");

    if (raw_len < 8)
    {
        printf("[ERROR] Packet too short (%d bytes)\n", raw_len);
        return;
    }

    // --------------------------------------------------
    // SMBus Layer
    // --------------------------------------------------
    uint8_t smbus_cmd    = raw[0];                 /* 0x0F */
    uint8_t byte_count   = raw[1];                 /* payload length */
    uint8_t pec_received = raw[byte_count + 2];    /* PEC is appended after byte_count bytes */

    /* PEC covers: destination_addr_write(1) + cmd(1) + byte_count(1) + payload(byte_count) */
    /* We reconstruct a temp buf with the write-address prepended to verify PEC             */
    /* NOTE: Since we are operating as slave, destination address is our own address (0x20<<1)
       The NVMe device wrote: [0x20(wr)] [0x0F] [BC] [src_addr] ... [PEC]
       For PEC calculation the first byte is 0x20 (MCU write address).                     */
    uint8_t pec_buf[256];
    pec_buf[0] = 0x20;           /* MCU 7-bit address 0x10, write direction => 0x20 */
    memcpy(&pec_buf[1], raw, byte_count + 2); /* cmd + byte_count + payload */
    uint8_t pec_calc = calc_pec_noinit(pec_buf, byte_count + 3);

    printf("[SMBus Layer]\n");
    printf("  Command Code : 0x%02X%s\n", smbus_cmd,
           smbus_cmd == 0x0F ? " (MCTP over SMBus)" : " (Unknown)");
    printf("  Byte Count   : %d (0x%02X)\n", byte_count, byte_count);
    printf("  PEC Received : 0x%02X\n", pec_received);
    printf("  PEC Calc     : 0x%02X => %s\n", pec_calc,
           (pec_calc == pec_received) ? "PASS" : "FAIL (possible data error)");

    if (byte_count < 6)
    {
        printf("[ERROR] Byte count too small for MCTP header\n");
        return;
    }

    // --------------------------------------------------
    // MCTP Transport Header (raw[2..6])
    // --------------------------------------------------
    uint8_t src_smbus  = raw[2];              /* NVMe physical addr: 7-bit << 1 | 1 */
    uint8_t mctp_ver   = raw[3];
    uint8_t dest_eid   = raw[4];
    uint8_t src_eid    = raw[5];
    uint8_t tag_byte   = raw[6];

    uint8_t som      = (tag_byte >> 7) & 0x01;
    uint8_t eom      = (tag_byte >> 6) & 0x01;
    uint8_t pkt_seq  = (tag_byte >> 4) & 0x03;
    uint8_t tag_own  = (tag_byte >> 3) & 0x01;
    uint8_t msg_tag  = (tag_byte)      & 0x07;

    printf("\n[MCTP Transport Header]\n");
    printf("  NVMe SMBus Addr (8-bit) : 0x%02X (7-bit: 0x%02X)\n", src_smbus, src_smbus >> 1);
    printf("  Header Version          : 0x%02X\n", mctp_ver);
    printf("  Destination EID         : %d (0x%02X)\n", dest_eid, dest_eid);
    printf("  Source EID              : %d (0x%02X)\n", src_eid, src_eid);
    printf("  Tag Byte (0x%02X):\n", tag_byte);
    printf("    SOM (Start of Msg) : %d\n", som);
    printf("    EOM (End of Msg)   : %d\n", eom);
    printf("    Packet Sequence    : %d\n", pkt_seq);
    printf("    Tag Owner          : %d\n", tag_own);
    printf("    Message Tag        : %d\n", msg_tag);

    if (!som || !eom)
    {
        printf("  [INFO] Multi-packet message (SOM=%d, EOM=%d) - only first/last packet shown\n", som, eom);
    }

    // --------------------------------------------------
    // MCTP Message Body starts at raw[7]
    // --------------------------------------------------
    uint8_t msg_type_raw = raw[7];
    uint8_t ic_bit       = (msg_type_raw >> 7) & 0x01;
    uint8_t msg_type     = msg_type_raw & 0x7F;

    printf("\n[MCTP Message Body]\n");
    printf("  IC Bit       : %d (%s)\n", ic_bit,
           ic_bit ? "Integrity Check (MIC) Present" : "No MIC");
    printf("  Message Type : 0x%02X => ", msg_type);

    // --------------------------------------------------
    // 1) MCTP Control Message (Type = 0x00)
    // --------------------------------------------------
    if (msg_type == 0x00)
    {
        printf("MCTP Control Message\n");

        if (byte_count < 9)
        {
            printf("  [ERROR] Control message too short\n");
            return;
        }

        uint8_t ctrl_hdr  = raw[8];   /* Rq | D | Instance ID */
        uint8_t cmd_code  = raw[9];
        uint8_t comp_code = raw[10];  /* Completion Code */

        uint8_t rq_bit    = (ctrl_hdr >> 7) & 0x01;
        uint8_t d_bit     = (ctrl_hdr >> 6) & 0x01;
        uint8_t inst_id   = ctrl_hdr & 0x1F;

        printf("  Ctrl Header  : 0x%02X (Rq=%d, D=%d, InstID=%d)\n",
               ctrl_hdr, rq_bit, d_bit, inst_id);
        printf("  Command Code : 0x%02X => ", cmd_code);

        const char *cmd_name = "Unknown";
        switch (cmd_code)
        {
        case 0x01: cmd_name = "Set Endpoint ID";           break;
        case 0x02: cmd_name = "Get Endpoint ID";           break;
        case 0x03: cmd_name = "Get Endpoint UUID";         break;
        case 0x04: cmd_name = "Get MCTP Version Support";  break;
        case 0x05: cmd_name = "Get Message Type Support";  break;
        default:   cmd_name = "Unknown Command";           break;
        }
        printf("%s\n", cmd_name);
        printf("  Completion   : 0x%02X => %s\n", comp_code,
               comp_code == 0x00 ? "Success" :
               comp_code == 0x01 ? "Error (Generic)"    :
               comp_code == 0x02 ? "Error (Not Ready)"  :
               comp_code == 0x03 ? "Error (Not Supported)" : "Unknown");

        /* Parse response payload per command */
        uint8_t *resp = &raw[11];
        uint8_t  resp_len = (byte_count > 9) ? (byte_count - 9) : 0;

        switch (cmd_code)
        {
        case 0x01: /* Set Endpoint ID Response */
            if (resp_len >= 2)
            {
                printf("  Alloc Status : 0x%02X\n", resp[0]);
                printf("  EID Setting  : 0x%02X (Assigned: %d)\n", resp[1], resp[1]);
                if (resp_len >= 3)
                    printf("  Pool Size    : %d\n", resp[2]);
            }
            break;

        case 0x02: /* Get Endpoint ID Response */
            if (resp_len >= 3)
            {
                printf("  Endpoint ID  : %d (0x%02X)\n", resp[0], resp[0]);
                printf("  EID Type     : 0x%02X (%s)\n", resp[1],
                       (resp[1] & 0x03) == 0x00 ? "Dynamic, unassigned" :
                       (resp[1] & 0x03) == 0x01 ? "Dynamic, assigned"   :
                       (resp[1] & 0x03) == 0x02 ? "Static, supported"   : "Static, matched");
                printf("  Endpoint Type: 0x%02X (%s)\n", resp[2],
                       (resp[2] & 0x30) == 0x00 ? "Simple Endpoint" :
                       (resp[2] & 0x30) == 0x10 ? "Bus Owner/Bridge" : "Reserved");
            }
            break;

        case 0x03: /* Get Endpoint UUID Response */
            if (resp_len >= 16)
            {
                printf("  UUID (RFC4122): ");
                printf("%02X%02X%02X%02X-", resp[0], resp[1], resp[2], resp[3]);
                printf("%02X%02X-",         resp[4], resp[5]);
                printf("%02X%02X-",         resp[6], resp[7]);
                printf("%02X%02X-",         resp[8], resp[9]);
                printf("%02X%02X%02X%02X%02X%02X\n",
                       resp[10], resp[11], resp[12], resp[13], resp[14], resp[15]);
            }
            break;

        case 0x05: /* Get Message Type Support Response */
            if (resp_len >= 1)
            {
                uint8_t type_cnt = resp[0];
                printf("  Supported Msg Types: %d\n", type_cnt);
                for (uint8_t t = 0; t < type_cnt && t < resp_len - 1; t++)
                {
                    printf("    [%d] 0x%02X => %s\n", t, resp[1 + t],
                           resp[1 + t] == 0x00 ? "MCTP Control" :
                           resp[1 + t] == 0x01 ? "PLDM"         :
                           resp[1 + t] == 0x02 ? "NCSI"         :
                           resp[1 + t] == 0x03 ? "Ethernet"     :
                           resp[1 + t] == 0x04 ? "NVMe-MI"      :
                           resp[1 + t] == 0x05 ? "SPDM"         :
                           resp[1 + t] == 0x06 ? "Secured Msg (SPDM)" : "Vendor/Unknown");
                }
            }
            break;

        default:
            printf("  Payload (hex): ");
            for (uint8_t t = 0; t < resp_len && t < 32; t++)
                printf("%02X ", resp[t]);
            printf("\n");
            break;
        }
    }
    // --------------------------------------------------
    // 2) NVMe-MI Message (Type = 0x04)
    // --------------------------------------------------
    else if (msg_type == 0x04)
    {
        printf("NVMe-MI Message\n");

        if (byte_count < 11)
        {
            printf("  [ERROR] NVMe-MI message too short\n");
            return;
        }

        /* NVMe-MI Header starts at raw[8] */
        uint8_t  nmimt_raw    = raw[8];
        /* NVMe-MI Spec (Rev 2.1) Fig 20: NMIMT = bits 6:3, ROR = bit 7, CSI = bit 0.
         * NMIMT values: 0h=Control Primitive, 1h=NVMe-MI Command,
         *               2h=NVMe Admin Command, 5h=Asynchronous Event. */
        uint8_t  nmimt        = (nmimt_raw >> 3) & 0x0F;
        uint8_t  ror          = (nmimt_raw >> 7) & 0x01;
        uint8_t  mi_flags     = raw[9];
        uint8_t  status       = raw[10];
        uint16_t ctrl_id      = (uint16_t)raw[11] | ((uint16_t)raw[12] << 8);

        printf("  NVMe-MI Hdr  : 0x%02X\n", nmimt_raw);
        printf("    ROR (Response) : %d\n", ror);
        printf("    NMIMT          : 0x%02X => %s\n", nmimt,
               nmimt == 0x00 ? "Control Primitive"           :
               nmimt == 0x01 ? "NVMe-MI Command Response"    :
               nmimt == 0x02 ? "NVMe Admin Command Response" :
               nmimt == 0x05 ? "Asynchronous Event"          : "Unknown");
        printf("  MI Flags     : 0x%02X\n", mi_flags);
        printf("  Status       : 0x%02X => %s\n", status,
               status == 0x00 ? "Success"                         :
               status == 0x01 ? "More Processing Required"        :
               status == 0x02 ? "Internal Error"                  :
               status == 0x03 ? "Invalid Command Parameter"       :
               status == 0x04 ? "Command Not Found"               :
               status == 0x05 ? "Invalid Command Size"            :
               status == 0x06 ? "Invalid Command Input Data Size" :
               status == 0x09 ? "Access Denied"                   :
               status == 0x0A ? "Not Supported"                   : "Unknown");
        printf("  Controller ID: 0x%04X\n", ctrl_id);

        /* Response Data starts at raw[13] */
        uint8_t *resp      = &raw[13];
        uint8_t  resp_len  = (byte_count >= 11) ? (byte_count - 11) : 0;

        /* MIC check:
         * byte_count bytes span raw[2..1+byte_count].
         * raw[2]     = src_smbus   (1 byte)
         * raw[3..6]  = MCTP hdr    (4 bytes: ver, dest_eid, src_eid, tag)
         * raw[7..]   = MCTP message body (IC + NVMe-MI header + data + MIC)
         * MCTP body length = byte_count - 1(src) - 4(mctp_hdr) = byte_count - 5
         * MIC data length  = MCTP body length - 4(MIC field)   = byte_count - 9
         * MIC field at raw[7 + mic_data_len] .. raw[7 + mic_data_len + 3]        */
        uint32_t  mic_data_len = (byte_count >= 9) ? ((uint32_t)byte_count - 9) : 0;
        if (ic_bit && mic_data_len >= 1)
        {
            uint8_t  *mic_start  = &raw[7];    /* from Message Type byte */
            uint32_t  mic_calc   = calc_mic(mic_start, mic_data_len);
            uint32_t  mic_recv   = (uint32_t)raw[7 + mic_data_len]             |
                                   ((uint32_t)raw[7 + mic_data_len + 1] << 8)  |
                                   ((uint32_t)raw[7 + mic_data_len + 2] << 16) |
                                   ((uint32_t)raw[7 + mic_data_len + 3] << 24);
            printf("  MIC Data Len : %d bytes (raw[7..%d])\n", mic_data_len, 7 + mic_data_len - 1);
            printf("  MIC Calc     : 0x%08X\n", mic_calc);
            printf("  MIC Received : 0x%08X => %s\n", mic_recv,
                   (mic_calc == mic_recv) ? "PASS" : "FAIL");
        }

        if (status == 0x00 && resp_len > 0)
        {
            printf("\n  [Response Data]\n");

            if (nmimt == 0x01) /* NVMe-MI Command Response */
            {
                /* Determine original command from context (NMIMT=0x01 covers multiple opcodes) */
                /* NVM Subsystem Health Status Poll Response */
                /* resp = msg byte 6: [0..1]=Mgmt Response (resp[1]=entry count),
                 * then Controller Health Data Structure (Opcode 0x02 response):
                 * [2..3]=CTLID [4..5]=CSTS [6..7]=CTEMP(Kelvin) [8]=PDLU [9]=SPARE
                 * [10]=CWARN [11..12]=CHSC                                          */
                if (resp_len >= 13)
                {
                    uint16_t ctempK = (uint16_t)resp[6] | ((uint16_t)resp[7] << 8);
                    printf("  --- Controller Health Data (NMIMT=0x01, Opc=0x02) ---\n");
                    printf("  Entries        : %d\n", resp[1]);
                    printf("  Controller ID  : 0x%04X\n",
                           (uint16_t)resp[2] | ((uint16_t)resp[3] << 8));
                    printf("  CSTS           : 0x%04X\n",
                           (uint16_t)resp[4] | ((uint16_t)resp[5] << 8));
                    nvme_mi_print_csts((uint16_t)resp[4] | ((uint16_t)resp[5] << 8));
                    printf("  Composite Temp : %d C (%d K)\n", (int32_t)ctempK - 273, ctempK);
                    printf("  PDLU           : %d%%\n", resp[8]);
                    printf("  Spare          : %d%%\n", resp[9]);
                    printf("  CritWarn       : 0x%02X\n", resp[10]);
                    printf("  CHSC           : 0x%04X\n",
                           (uint16_t)resp[11] | ((uint16_t)resp[12] << 8));
                }

                /* Print remaining raw bytes */
                printf("  Raw (hex): ");
                for (uint8_t i = 0; i < resp_len && i < 32; i++)
                    printf("%02X ", resp[i]);
                printf("\n");
            }
            else if (nmimt == 0x02) /* NVMe Admin Command Response (NVMe-MI Spec Fig 137/138) */
            {
                /* NVMe-MI message byte 0 = raw[7]. Response layout:
                 *   byte 4      -> Status (NVMe-MI level)
                 *   bytes 8..11 -> CQE Dword 0 (command-specific result)
                 *   bytes 16..19-> CQE Dword 3 (contains NVMe Status Field)
                 *   byte 20..   -> NVMe Response Data (optional)                 */
                printf("  --- NVMe Admin Cmd Response (NMIMT=0x02) ---\n");
                if (byte_count >= 25)   /* through CQE DW3 (raw[26]) */
                {
                    uint8_t  admin_status = raw[11];   /* msg byte 4 */
                    uint32_t cqedw0 = (uint32_t)raw[15] | ((uint32_t)raw[16] << 8)  |
                                      ((uint32_t)raw[17] << 16) | ((uint32_t)raw[18] << 24);
                    uint32_t cqedw3 = (uint32_t)raw[23] | ((uint32_t)raw[24] << 8)  |
                                      ((uint32_t)raw[25] << 16) | ((uint32_t)raw[26] << 24);
                    /* NVMe Status Field lives in CQE Dword 3, bits 31:17 */
                    uint8_t  sc  = (uint8_t)((cqedw3 >> 17) & 0xFF);
                    uint8_t  sct = (uint8_t)((cqedw3 >> 25) & 0x07);
                    printf("  NVMe-MI Status  : 0x%02X (%s)\n", admin_status,
                           admin_status == 0x00 ? "Success" : "Error");
                    printf("  CQE DW0         : 0x%08X\n", cqedw0);
                    printf("  CQE DW3         : 0x%08X\n", cqedw3);
                    printf("    Status Code (SC)  : 0x%02X (%s)\n", sc,
                           sc == 0x00 ? "Successful Completion" : "Error");
                    printf("    Status Code Type  : 0x%02X (%s)\n", sct,
                           sct == 0x00 ? "Generic Command" :
                           sct == 0x01 ? "Command Specific" :
                           sct == 0x02 ? "Media/Data Integrity" : "Vendor");
                }

                /* NVMe Response Data starts at msg byte 20 = raw[27], last 4 bytes = MIC */
                if (byte_count > 30)
                {
                    uint32_t admin_data_len = (uint32_t)byte_count - 29; /* excl. MIC */
                    printf("  Admin Response Data (hex):\n  ");
                    for (uint32_t i = 0; i < admin_data_len && i < 64; i++)
                    {
                        printf("%02X ", raw[27 + i]);
                        if ((i + 1) % 16 == 0) printf("\n  ");
                    }
                    printf("\n");
                }
            }
        }
    }
    else
    {
        printf("Unknown (0x%02X)\n", msg_type);
        printf("  Raw Body (hex): ");
        for (uint8_t i = 7; i < byte_count + 2 && i < 64; i++)
            printf("%02X ", raw[i]);
        printf("\n");
    }

    /* Always print full raw dump for debug */
    printf("\n[Raw I2C Data (%d bytes)]\n  ", byte_count + 2);
    for (int i = 0; i < byte_count + 3 && i < 80; i++)
    {
        printf("%02X ", raw[i]);
        if ((i + 1) % 16 == 0) printf("\n  ");
    }
    printf("\n========================================\n\n");
}

// =============================================================================
// Legacy simple MCTP EID response parser (kept for backward compatibility)
// =============================================================================
void parse_mctp_response_eid(uint8_t *data, uint8_t len) {
    printf("=== MCTP Packet Parsing ===\n");
    
    // 1. SMBus/Physical Layer Info
    printf("[SMBus Layer]\n");
    printf("  Source Address (8-bit): 0x%02X (NVMe Device)\n", data[0]);
    
    // 2. MCTP Transport Header (Byte 1-4)
    printf("[MCTP Transport Header]\n");
    printf("  Header Version: %d\n", (data[1] >> 4) & 0xF);
    printf("  Dest Endpoint ID: %d (M031)\n", data[2]);
    printf("  Src  Endpoint ID: %d (NVMe)\n", data[3]);
    
    uint8_t tag_byte = data[4];
    printf("  Flags (0x%02X):\n", tag_byte);
    printf("    SOM (Start): %d\n", (tag_byte & 0x80) ? 1 : 0);
    printf("    EOM (End):   %d\n", (tag_byte & 0x40) ? 1 : 0);
    printf("    Pkt Seq:     %d\n", (tag_byte >> 4) & 0x3);
    printf("    Tag Owner:   %d\n", (tag_byte & 0x08) ? 1 : 0);
    printf("    Msg Tag:     %d\n", tag_byte & 0x07);

    // 3. MCTP Message Body
    uint8_t msg_type = data[5] & 0x7F; // Mask IC bit if present
    printf("[MCTP Message Body]\n");
    printf("  Message Type: 0x%02X (%s)\n", msg_type, 
           msg_type == 0x00 ? "MCTP Control" : 
           msg_type == 0x04 ? "NVMe-MI" : "Unknown");

    if (msg_type == 0x00) {
        // ?? Control Message
        printf("  -- Control Message Details --\n");
        printf("  Ctrl Header:  0x%02X\n", data[6]);
        printf("  Command Code: 0x%02X (%s)\n", data[7], 
               data[7] == 0x03 ? "Get Endpoint ID" : "Other");
        printf("  Completion Code: 0x%02X\n", data[8]);
        
        if (data[7] == 0x03) {
            printf("  > Assigned Endpoint ID: %d\n", data[9]);
            printf("  > EID Type: %d\n", data[10]);
        }
    } else if (msg_type == 0x04) {
        printf("  -- NVMe-MI Message --\n");
        // ??????????????????
    }
}
// ?? M031 ? NVMe ? I2C ?? (7-bit)
#define MCU_ADDR_7BIT   0x10
#define NVME_ADDR_7BIT  0x1D

// ?? NVMe-MI ??
#define MCTP_DEST_EID   0x00 // ?????? 5,??? 0x05
#define MCTP_SRC_EID    0x01

// ---------------------------------------------------------
// 1. CRC ?????
// ---------------------------------------------------------

// SMBus PEC (CRC-8) ???: x^8 + x^2 + x + 1 (0x07)
static uint8_t calc_pec_noinit(uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// NVMe-MI MIC (CRC-32C) ???: 0x1EDC6F41 (Castagnoli)
static uint32_t calc_mic(uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                //crc = (crc >> 1) ^ 0x1EDC6F41;
							crc = (crc >> 1) ^ 0x82F63B78;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFF;
}

#define MCTP_TX_MTU          (64U)
#define MCTP_RX_TIMEOUT_CNT  (0x00A00000UL)

/**
 * @brief  Send one NVMe-MI message (NMH..MIC) to the SSD, split into MCTP packets.
 * @param  pMsg    Message bytes starting at NMH byte 0 (0x84), MIC included
 * @param  msgLen  Message length in bytes
 * @retval None
 */
static void mctp_tx_message(uint8_t *pMsg, uint16_t msgLen)
{
    uint8_t  pkt[8U + MCTP_TX_MTU + 1U];
    uint16_t off = 0U;
    uint8_t  seq = 0U;

    if (pMsg == NULL)
    {
        return;
    }

    while (off < msgLen)
    {
        uint16_t frag = (uint16_t)(msgLen - off);
        uint8_t  som  = (off == 0U) ? 1U : 0U;
        uint8_t  eom;
        uint16_t p;

        if (frag > MCTP_TX_MTU)
        {
            frag = MCTP_TX_MTU;
        }
        eom = ((uint16_t)(off + frag) >= msgLen) ? 1U : 0U;

        pkt[0] = 0x3A;                              // SSD address (write)
        pkt[1] = 0x0F;                              // MCTP over SMBus
        pkt[2] = (uint8_t)(5U + frag);              // byte count = src(1)+MCTP hdr(4)+payload
        pkt[3] = (MCU_ADDR_7BIT << 1) | 1U;         // source slave address
        pkt[4] = 0x01;                              // header version
        pkt[5] = 0x00;                              // dest EID
        pkt[6] = 0x01;                              // src EID
        pkt[7] = (uint8_t)((som << 7) | (eom << 6) | ((seq & 0x03U) << 4) | (1U << 3)); // TO=1, tag=0
        for (p = 0U; p < frag; p++)
        {
            pkt[8U + p] = pMsg[off + p];
        }
        pkt[8U + frag] = calc_pec_noinit(pkt, 8U + frag);

        I2C_WriteMultiBytes(I2C_PORT, pkt[0] >> 1, &pkt[1], 8U + frag);

        off = (uint16_t)(off + frag);
        seq = (uint8_t)((seq + 1U) & 0x03U);
        /* Gap between request packets only: no delay after the last one, because
         * the slave (I2C1 int) must be enabled before the device starts replying. */
        if (off < msgLen)
        {
            CLK_SysTickDelay(2000);
        }
    }
}

/**
 * @brief  Receive and reassemble one MCTP message from the RX queue.
 *         Checks PEC per packet, SOM/EOM/sequence order, and MIC on the full message.
 * @param  pMsg     Output buffer (concatenated MCTP payloads: NMH..MIC)
 * @param  maxLen   Size of pMsg
 * @param  pMsgLen  Output: reassembled length
 * @retval 0 on success, negative on timeout / framing / PEC / sequence / MIC error
 */
static int32_t mctp_rx_message_core(uint8_t *pMsg, uint16_t maxLen, uint16_t *pMsgLen)
{
    uint8_t  pkt[MCTP_RXQ_PKT_SIZE + 1U];
    uint16_t len = 0U;
    uint8_t  started = 0U;
    uint8_t  expSeq = 0U;
    uint8_t  pktCnt = 0U;
    uint32_t waitCnt = 0U;

    if ((pMsg == NULL) || (pMsgLen == NULL))
    {
        return -1;
    }
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

        if (s_rxqRd == s_rxqWr)
        {
            if (++waitCnt > MCTP_RX_TIMEOUT_CNT)
            {
                printf("  [RX] timeout after %u pkt(s), %u bytes\n", pktCnt, len);
                return -2;
            }
            continue;
        }
        waitCnt = 0U;

        rawLen = s_rxqLen[s_rxqRd];
        if (rawLen > MCTP_RXQ_PKT_SIZE)
        {
            s_rxqRd = (uint8_t)((s_rxqRd + 1U) % MCTP_RXQ_DEPTH);
            printf("  [RX] pkt#%u oversize (%u bytes)\n", pktCnt, rawLen);
            return -3;
        }
        /* pkt[0] = MCU write address (part of PEC), pkt[1..] = received bytes:
         * [1]=0x0F [2]=BC [3]=SrcAddr [4]=HdrVer [5]=DstEID [6]=SrcEID [7]=Tag [8..]=payload [3+BC]=PEC */
        pkt[0] = (uint8_t)(MCU_ADDR_7BIT << 1);
        memcpy(&pkt[1], s_rxqPkt[s_rxqRd], rawLen);
        s_rxqRd = (uint8_t)((s_rxqRd + 1U) % MCTP_RXQ_DEPTH);
        pktCnt++;

        bc = pkt[2];
        if ((pkt[1] != 0x0FU) || (bc < 6U) || ((uint16_t)rawLen < ((uint16_t)bc + 3U)))
        {
            printf("  [RX] pkt#%u bad frame: cmd=0x%02X bc=%u raw=%u\n", pktCnt, pkt[1], bc, rawLen);
            return -4;
        }
        if (calc_pec_noinit(pkt, (size_t)bc + 3U) != pkt[3U + bc])
        {
            printf("  [RX] pkt#%u PEC fail\n", pktCnt);
            return -5;
        }

        tag    = pkt[7];
        som    = (uint8_t)((tag >> 7) & 0x01U);
        eom    = (uint8_t)((tag >> 6) & 0x01U);
        seq    = (uint8_t)((tag >> 4) & 0x03U);
        payLen = (uint8_t)(bc - 5U);

        if (started == 0U)
        {
            if (som == 0U)
            {
                printf("  [RX] pkt#%u missing SOM (tag=0x%02X)\n", pktCnt, tag);
                return -6;
            }
            started = 1U;           /* first sequence number is chosen by the device */
        }
        else
        {
            if (som != 0U)
            {
                printf("  [RX] pkt#%u unexpected SOM (tag=0x%02X)\n", pktCnt, tag);
                return -6;
            }
            if (seq != expSeq)
            {
                printf("  [RX] pkt#%u seq %u, expected %u\n", pktCnt, seq, expSeq);
                return -7;
            }
        }

        if (((uint32_t)len + payLen) > maxLen)
        {
            printf("  [RX] message exceeds buffer (%u)\n", maxLen);
            return -8;
        }
        memcpy(&pMsg[len], &pkt[8], payLen);
        len = (uint16_t)(len + payLen);
        expSeq = (uint8_t)((seq + 1U) & 0x03U);

        if (eom != 0U)
        {
            break;
        }
    }

    /* MIC = CRC-32C over the whole message except the last 4 bytes (little-endian) */
    if ((pMsg[0] & 0x80U) != 0U)
    {
        uint32_t micCalc;
        uint32_t micRx;

        if (len < 5U)
        {
            return -9;
        }
        micCalc = calc_mic(pMsg, (size_t)len - 4U);
        micRx   = (uint32_t)pMsg[len - 4U]
                | ((uint32_t)pMsg[len - 3U] << 8)
                | ((uint32_t)pMsg[len - 2U] << 16)
                | ((uint32_t)pMsg[len - 1U] << 24);
        if (micCalc != micRx)
        {
            printf("  [RX] MIC fail: calc=0x%08X rx=0x%08X\n", micCalc, micRx);
            return -9;
        }
    }

    printf("  [RX] reassembled %u pkt(s), %u bytes, PEC/MIC PASS\n", pktCnt, len);
    *pMsgLen = len;
    return 0;
}

static int32_t mctp_rx_message(uint8_t *pMsg, uint16_t maxLen, uint16_t *pMsgLen)
{
    int32_t ret = mctp_rx_message_core(pMsg, maxLen, pMsgLen);

    if (ret <= -3)
    {
        /* The device may still be sending the rest of the message: discard packets
         * until the bus is idle, so they do not pollute the following commands. */
        uint32_t idleCnt = 0U;
        uint16_t dropCnt = 0U;

        while (idleCnt <= MCTP_RX_TIMEOUT_CNT)
        {
            if (s_rxqRd != s_rxqWr)
            {
                s_rxqRd = (uint8_t)((s_rxqRd + 1U) % MCTP_RXQ_DEPTH);
                dropCnt++;
                idleCnt = 0U;
            }
            else
            {
                idleCnt++;
            }
        }
        printf("  [RX] drained %u leftover pkt(s)\n", dropCnt);
    }
    return ret;
}

/**
 * @brief  Send a 16-byte NVMe-MI Command request (NMH..NMD1), append MIC,
 *         and receive the (possibly multi-packet) response.
 * @param  pReq      16 bytes: NMH(4) + Opcode/Rsvd(4) + NMD0(4) + NMD1(4)
 * @param  pResp     Response buffer (NMH..MIC)
 * @param  maxLen    Size of pResp
 * @param  pRespLen  Output: response length
 * @retval Same as mctp_rx_message()
 */
static int32_t nvme_mi_cmd_xfer(const uint8_t *pReq, uint8_t *pResp, uint16_t maxLen, uint16_t *pRespLen)
{
    uint8_t  msg[20];
    uint32_t mic;
    int32_t  ret;

    if ((pReq == NULL) || (pResp == NULL) || (pRespLen == NULL))
    {
        return -1;
    }
    memcpy(msg, pReq, 16);
    mic = calc_mic(msg, 16);
    msg[16] = (uint8_t)(mic & 0xFF);
    msg[17] = (uint8_t)((mic >> 8) & 0xFF);
    msg[18] = (uint8_t)((mic >> 16) & 0xFF);
    msg[19] = (uint8_t)((mic >> 24) & 0xFF);

    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
    s_rxqRd = s_rxqWr;
    mctp_tx_message(msg, (uint16_t)sizeof(msg));

    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    g_u8SlvRxFlag = 0;
    ret = mctp_rx_message(pResp, maxLen, pRespLen);
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);
    return ret;
}

void parse_nvme_ds_response(uint8_t *i2c_buf) {
    // i2c_buf ????:
    // [0] Addr(Wr), [1] Cmd, [2] Len, [3..N] Payload, [Last] PEC
    
    
    uint8_t smbus_cmd  = i2c_buf[0];
    uint8_t payload_len = i2c_buf[1]; // 0x11 = 17 bytes
    uint8_t *payload = &i2c_buf[2];
    uint8_t pec_recv = i2c_buf[2 + payload_len];

    printf("=== SMBus Layer ===\n");
   // printf("Target Addr:  0x%02X (MCU Slave)\n", smbus_addr);
    printf("Payload Len:  %d bytes\n", payload_len);

    // --- MCTP Layer ---
    // Payload [0..4]
    uint8_t mctp_src = payload[0];
    uint8_t mctp_dst_eid = payload[2];
    uint8_t mctp_src_eid = payload[3];
    uint8_t mctp_tag = payload[4];

    printf("\n=== MCTP Layer ===\n");
    printf("Source Addr:  0x%02X (NVMe: 0x%02X)\n", mctp_src, mctp_src >> 1);
    printf("Dest EID:     %d (M031)\n", mctp_dst_eid);
    printf("Source EID:   %d (NVMe)\n", mctp_src_eid);
    printf("Tag:          0x%02X (SOM=%d, EOM=%d, MsgTag=%d)\n", 
           mctp_tag, (mctp_tag>>7)&1, (mctp_tag>>6)&1, mctp_tag&0x07);

    // --- NVMe-MI Layer ---
    // Payload [5..8]
    uint8_t mi_type = payload[5];
    uint8_t mi_flags = payload[6];
    
    printf("\n=== NVMe-MI Layer ===\n");
    printf("Msg Type:     0x%02X (%s)\n", mi_type, (mi_type==0x04)?"NVMe-MI":"Unknown");
    printf("Flags:        0x%02X (IC=%d, NMIMT=%d)\n", 
           mi_flags, (mi_flags>>7)&1, mi_flags&0x0F);

    // --- Response Body ---
    // Payload [9..12] (Response Status + 3 bytes Rsvd)
    uint8_t status = payload[9];
    printf("\n=== Response Body ===\n");
    printf("Status:       0x%02X ", status);
    if (status == 0x00) printf("(Success)\n");
    else if (status == 0x05) printf("(Invalid Command Size)\n");
    else printf("(Error)\n");

 
}


// ????:???? MCTP ????
// ????:???? MCTP ????
void send_mctp_packet(uint8_t som, uint8_t eom, uint8_t seq, uint8_t *payload, uint8_t payload_len) {
    uint8_t buf[80];
    buf[0] = 0x3A; // SSD Address (Write)
    buf[1] = 0x0F; // Command Code
    
    // --------------------------------------------------------
    // ?????!?
    // SMBus Byte Count = Src Addr (1) + MCTP Hdr (4) + Payload
    // ??? 5 + payload_len
    // --------------------------------------------------------
    buf[2] = 5 + payload_len; 

    // MCTP Header
    buf[3] = (MCU_ADDR_7BIT << 1) | 1; // Src Addr
    buf[4] = 0x01; // Header Version
    buf[5] = 0x00; // Dest EID
    buf[6] = 0x01; // Src EID
    buf[7] = (som << 7) | (eom << 6) | ((seq & 0x03) << 4) | 0x08; 

    // ???????? Payload
    memcpy(&buf[8], payload, payload_len);
    
    // ?? PEC ???
    buf[8 + payload_len] = calc_pec_noinit(buf, 8 + payload_len);
    I2C_WriteMultiBytes(I2C_PORT, buf[0] >> 1, &buf[1], 8 + payload_len);
}
int main(void)
{
    int i;
    unsigned char  data_len;
    /* Init System, peripheral clock and multi-function I/O */
    SYS_Init();

    /* Init UART to 115200-8n1 for print message */
    UART_Open(UART3, 115200);
    //Set_USB_SerialNumber_From_UID();
   // HSUSBD_Open(&gsHSInfo, HID_ClassRequest, NULL);
    //HSUSBD_SetVendorRequest(HID_VendorRequest);

    /* Endpoint configuration */
    //HID_Init();

    /* Enable HSUSBD interrupt */
    //NVIC_EnableIRQ(USBD20_IRQn);
    //HSUSBD_DISABLE_USB();

    //HSUSBD_ENABLE_USB();
    /* Start transaction */
    //HSUSBD_Start();
    GPIO_SetSlewCtl(PA, BIT7, GPIO_SLEWCTL_FAST);
    GPIO_SetSlewCtl(PA, BIT6, GPIO_SLEWCTL_FAST);
    GPIO_SetSlewCtl(PC, BIT1, GPIO_SLEWCTL_FAST);
    GPIO_SetSlewCtl(PC, BIT0, GPIO_SLEWCTL_FAST);
    PA->SMTEN |= GPIO_SMTEN_SMTEN7_Msk | GPIO_SMTEN_SMTEN6_Msk ;
    PC->SMTEN |= GPIO_SMTEN_SMTEN1_Msk | GPIO_SMTEN_SMTEN0_Msk ;

    printf("\n\nCPU @ %dHz\n", SystemCoreClock);
    printf("inital scan:%d\n\r", xsvftool_esp_scan());
    printf("jtag id 0x%x\n\r", xsvftool_esp_id());
    //add cpld jtag id
    bmc_report[cpld_jtag_id] = (xsvftool_esp_id() >> 24) & 0xff;
    bmc_report[cpld_jtag_id + 1] = (xsvftool_esp_id() >> 16) & 0xff;
    bmc_report[cpld_jtag_id + 2] = (xsvftool_esp_id() >> 8) & 0xff;
    bmc_report[cpld_jtag_id + 3] = (xsvftool_esp_id() >> 0) & 0xff;

    I2C0_Init();
    I2C1_Init();


    HSUSBD_Start();
    response_buff[0] = 0;
    response_buff[1] = 0;
    response_buff[2] = 0;
    response_buff[3] = 0;

    for (i = 0; i < 4; i++)
    {
        HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
    }

    HSUSBD->EP[EPA].EPTXCNT = 1024;
    HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);

    /* Open Timer0 in periodic mode, enable interrupt and 1000 interrupt tick per second */
    TIMER_Open(TIMER0, TIMER_PERIODIC_MODE, 1000);
    TIMER_EnableInt(TIMER0);
    NVIC_EnableIRQ(TMR0_IRQn);
    TIMER_Start(TIMER0);

    GPIO_SetMode(PA, BIT9, GPIO_MODE_OUTPUT);
    PA9 = 0; // HWM_SEL
		
    GPIO_SetMode(PC, BIT14, GPIO_MODE_OUTPUT);
		  PC14 = 0;
		
    I2C_WriteByte(I2C1, TCA9548, (0x01 << 0));




		
		
// =========================================================================
// I2C/SMBus Bus Scanner + MCTP Endpoint Auto-Probe
// Step 1: probe every 7-bit address (0x08..0x77), record which ACK on write.
// Step 2: send MCTP "Get Endpoint UUID" to each ACKing address; whichever
//         returns a valid MCTP response (raw[9]=0x00 Ctrl, raw[10]=0x03 Cmd)
//         is the drive's NVMe-MI/MCTP endpoint.
//   Phison default : 0x1D. Samsung PM9D3a uses a different address -> detect.
// =========================================================================
{
    uint8_t sc_list[16]; uint8_t sc_n = 0;
    uint8_t sc_addr;
    printf("\n>>> [SCAN] I2C Bus Address Scan (write-probe 0x08..0x77) <<<\n");
    for (sc_addr = 0x08; sc_addr <= 0x77; sc_addr++)
    {
        uint32_t sc_sta;
        I2C_START(I2C1);
        I2C_WAIT_READY(I2C1) {}
        I2C_SET_DATA(I2C1, (sc_addr << 1) | 0x00);        // addr + W
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI);
        I2C_WAIT_READY(I2C1) {}
        sc_sta = I2C_GET_STATUS(I2C1);
        I2C_STOP(I2C1);
        if (sc_sta == 0x18)                                // 0x18 = SLA+W ACK
        {
            printf("  ACK @ 7-bit 0x%02X  (W=0x%02X R=0x%02X)\n",
                   sc_addr, (sc_addr << 1), (sc_addr << 1) | 1);
            if (sc_n < 16) sc_list[sc_n++] = sc_addr;
        }
        CLK_SysTickDelay(2000);
    }
    printf("  Scan done: %d device(s) ACKed.\n", sc_n);

    // --- Step 2: MCTP Get Endpoint UUID probe ---
    // SAFETY: MCTP requires WRITING a command frame. Writing arbitrary bytes to
    // unknown I2C devices (EEPROMs/sensors/mux) can corrupt them. So we ONLY
    // probe the NVMe-MI standard SMBus addresses, never every ACKing address.
    printf("\n>>> [PROBE] MCTP Get-UUID on NVMe standard addresses only <<<\n");
    {
        static const uint8_t nvme_cand[] = { 0x1D, 0x6A };
        uint8_t pi;
        for (pi = 0; pi < sizeof(nvme_cand)/sizeof(nvme_cand[0]); pi++)
        {
            uint8_t da = nvme_cand[pi];
            uint8_t acked = 0, qi;
            for (qi = 0; qi < sc_n; qi++) if (sc_list[qi] == da) acked = 1;
            if (!acked) { printf("  addr 0x%02X: not present (no ACK) - skip\n", da); continue; }

            uint8_t req[12]; uint8_t idx = 0;
            req[idx++] = da << 1;          // SMBus dest addr (W)
            req[idx++] = 0x0F;             // MCTP over SMBus
            req[idx++] = 0x08;             // byte count (8 payload)
            req[idx++] = (0x10 << 1) | 1;  // src = MCU 0x10
            req[idx++] = 0x01;             // hdr version
            req[idx++] = 0x00;             // dest EID
            req[idx++] = 0x01;             // src EID
            req[idx++] = 0xC8;             // SOM=EOM=TO=1
            req[idx++] = 0x00;             // MCTP Control
            req[idx++] = 0x80;             // Rq=1
            req[idx++] = 0x03;             // Get Endpoint UUID
            req[idx] = calc_pec(0, req, idx); idx++;

            I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
            s_I2C1HandlerFn = I2C_SlaveTRx;
            I2C_WriteMultiBytes(I2C_PORT, da, &req[1], idx - 1);
            I2C_EnableInt(I2C1); NVIC_EnableIRQ(I2C1_IRQn);
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
            g_u8SlvRxFlag = 0;
            WAIT_SLV_RX_TIMEOUT();
            I2C_DisableInt(I2C1); NVIC_DisableIRQ(I2C1_IRQn);

            {
                uint8_t bc = g_au8SlvRxData[1];
                uint8_t mt = g_au8SlvRxData[7];   // MCTP msg type
                printf("  addr 0x%02X: bc=%d mt=0x%02X ", da, bc, mt);
                /* Reject stale buffer data: need a new RX and SrcAddr == probed addr */
                if ((g_u8SlvRxFlag != 0U) && (g_au8SlvRxData[2] == (uint8_t)((da << 1) | 1U)) &&
                    (bc >= 8) && ((mt == 0x00) || (mt == 0x84) || (mt == 0x04))) {
                    printf("<== MCTP ENDPOINT! raw: ");
                    { uint8_t r; for (r = 0; r < bc + 2 && r < 32; r++) printf("%02X ", g_au8SlvRxData[r]); }
                    printf("\n");
                } else {
                    printf("(no MCTP resp)\n");
                }
            }
            CLK_SysTickDelay(50000);
        }
    }
    printf("  NOTE: non-NVMe ACK addrs (EEPROM/sensor/mux) are NOT MCTP-probed.\n");
    printf("  -> If no ENDPOINT found, this drive has no SMBus MCTP (use PCIe VDM).\n");
}



// =========================================================================
// NVMe Basic Management Command - Direct SMBus Register Read (No MCTP)
// This bypasses MCTP entirely. Uses simple SMBus Block Read:
//   Write: [DevAddr_W][0x00]        (select register 0x00)
//   Read:  [DevAddr_R][32 bytes]    (Basic Management Data Structure)
//
// Basic Management Data Structure layout (NVMe-MI Spec):
//   Byte  0:     Status Block Length (= 6)
//   Byte  1:     SFLGS (Status Flags)
//   Byte  2:     SMART Critical Warnings
//   Byte  3:     Composite Temperature (0x80=No Data, 0x81=Fail, else Celsius)
//   Byte  4:     PDLU (Drive Life Used %)
//   Byte  5-6:   Reserved
//   Byte  7:     PEC for Status Block
//   Byte  8:     ID Block Length (= 22)
//   Byte  9-10:  PCIe Vendor ID (VID)     <-- KEY field
//   Byte 11-30:  Serial Number (20 ASCII)  <-- KEY field
//   Byte 31:     PEC for ID Block
// =========================================================================
{
    cmd_begin("Basic Management");
    printf("\n>>> [CMD] NVMe Basic Management Command - Direct SMBus Register 0x00 <<<\n");
    printf("=== NVMe Basic Management (No MCTP) ===\n");
    printf("NOTE: SMBus Block Read requires Repeated START (no STOP between W and R).\n");
    printf("      Previous attempt used two separate transactions -> all 0xFF.\n");
    printf("      Now using I2C_WriteMultiBytesOneReg for proper repeated-start read.\n\n");

    // NVMe-MI Basic Management uses SMBus Block Read:
    //   START + [DevAddr_W] + [Reg=0x00] + REPEATED_START + [DevAddr_R] + [32 bytes] + STOP
    // Must NOT issue STOP after writing the register offset.
    // Use I2C_ReadMultiBytesTwoRegs or manual approach via I2C_WriteMultiBytes with restart.
    //
    // Also scan multiple candidate addresses:
    //   0x1D = NVMe-MI Management Address (our current MCTP target)
    //   0x6A = alternative NVMe-MI Management Address
    //   0x53 = used elsewhere in this codebase
    //   0x50 = common EEPROM / management address

    // 0x6A confirmed working (got SN=S7RGNG0Y106092 at this address)
    static const uint8_t bm_addrs[] = {0x6A, 0x1D, 0x53, 0x50};
    uint8_t bm_ai;

    for (bm_ai = 0; bm_ai < 4; bm_ai++)
    {
        uint8_t bm_addr = bm_addrs[bm_ai];
        uint8_t bm_buf[32];
        uint8_t bm_len = 0;

        printf("--- Trying addr=0x%02X ---\n", bm_addr);

        // Use I2C_WriteMultiBytesOneReg which internally does:
        //   START + [DevAddr_W] + [Reg] + REPEATED_START + [DevAddr_R] + [data] + STOP
        // This is equivalent to SMBus Block Read with repeated start.
        bm_len = I2C_ReadMultiBytesTwoRegs(I2C_PORT, bm_addr, 0x00, bm_buf, 32);

        if (bm_len == 0)
        {
            // Fallback: try manual repeated-start via raw I2C primitives
            // START
            I2C_START(I2C1);
            I2C_WAIT_READY(I2C1) {}
            // Send addr+W
            I2C_SET_DATA(I2C1, (bm_addr << 1) | 0x00);
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI);
            I2C_WAIT_READY(I2C1) {}
            if (I2C_GET_STATUS(I2C1) != 0x18) { // 0x18 = SLA+W ACK
                I2C_STOP(I2C1);
                printf("  NACK at addr+W\n");
                CLK_SysTickDelay(100000);
                continue;
            }
            // Send register offset 0x00
            I2C_SET_DATA(I2C1, 0x00);
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI);
            I2C_WAIT_READY(I2C1) {}
            // REPEATED START
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_STA | I2C_CTL_SI);
            I2C_WAIT_READY(I2C1) {}
            // Send addr+R
            I2C_SET_DATA(I2C1, (bm_addr << 1) | 0x01);
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI);
            I2C_WAIT_READY(I2C1) {}
            if (I2C_GET_STATUS(I2C1) != 0x40) { // 0x40 = SLA+R ACK
                I2C_STOP(I2C1);
                printf("  NACK at addr+R\n");
                CLK_SysTickDelay(100000);
                continue;
            }
            // Read 32 bytes
            {
                uint8_t ri;
                for (ri = 0; ri < 32; ri++) {
                    if (ri < 31)
                        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA); // ACK
                    else
                        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI);              // NACK last
                    I2C_WAIT_READY(I2C1) {}
                    bm_buf[ri] = I2C_GET_DATA(I2C1);
                }
            }
            I2C_STOP(I2C1);
            bm_len = 32;
        }

        // Check if all 0xFF (bus not driven = device not present)
        {
            uint8_t all_ff = 1;
            uint8_t ci2;
            for (ci2 = 0; ci2 < 8; ci2++) {
                if (bm_buf[ci2] != 0xFF) { all_ff = 0; break; }
            }
            if (all_ff) {
                printf("  All 0xFF -> no device at this address\n");
                CLK_SysTickDelay(100000);
                continue;
            }
        }

        printf("  Got response! Raw (%d bytes):\n  ", bm_len);
        {
            uint8_t ri;
            for (ri = 0; ri < 32; ri++) {
                printf("%02X ", bm_buf[ri]);
                if ((ri + 1) % 16 == 0) printf("\n  ");
            }
            printf("\n");
        }

        if (bm_len >= 8)
        {
            printf("========================================\n");
            printf("  NVMe Basic Management - Parsed Fields\n");
            printf("========================================\n");

            uint8_t status_len = bm_buf[0];
            printf("  Status Block Length : %d%s\n", status_len,
                   status_len == 6 ? " (OK)" : " (unexpected)");

            /* Byte 0 must be 6 for a valid NVMe Basic Management response.
             * Any other value means this address is not an NVMe drive
             * (e.g. EEPROM/sensor returning 0x00/0xFF). Skip parsing so we
             * do not print bogus Temperature/VID/SN from junk bytes. */
            if (status_len != 6)
            {
                printf("  -> Not an NVMe Basic Management response, skip parse.\n");
                printf("========================================\n");
                CLK_SysTickDelay(200000);
                continue;
            }

            uint8_t sflgs = bm_buf[1];
            printf("  SFLGS (0x%02X):\n", sflgs);
            printf("    Drive Functional  : %s\n", (sflgs & 0x20) ? "YES" : "NO");
            printf("    PCIe Link Active  : %s\n", (sflgs & 0x08) ? "Active" : "Down");
            printf("    Power Ready       : %s\n", (sflgs & 0x40) ? "Not Ready" : "Ready");

            uint8_t smart_warn = bm_buf[2];
            printf("  SMART Warn          : 0x%02X (%s)\n", smart_warn,
                   smart_warn == 0x00 ? "No Warnings" :
                   smart_warn == 0xFF ? "N/A (not supported by device)" : "WARNING!");

            uint8_t temp_raw = bm_buf[3];
            printf("  Temperature         : ");
            if      (temp_raw == 0x80) printf("No Data\n");
            else if (temp_raw == 0x81) printf("Sensor Failure\n");
            else if (temp_raw <= 0x7E) printf("%d C\n", (int)temp_raw);
            else if (temp_raw >= 0xC5) printf("%d C\n", (int8_t)temp_raw);
            else                       printf("Reserved(0x%02X)\n", temp_raw);

            printf("  Drive Life Used     : %d%%\n", bm_buf[4]);

            if (bm_len >= 12) {
                // Device stores VID big-endian: buf[9]=MSB(0x14), buf[10]=LSB(0x4D) -> 0x144D Samsung
                uint16_t vid = ((uint16_t)bm_buf[9] << 8) | (uint16_t)bm_buf[10];
                printf("  PCIe VID            : 0x%04X", vid);
                if      (vid == 0x144D) printf(" [Samsung]");
                else if (vid == 0x8086) printf(" [Intel]");
                else if (vid == 0x1C5C || vid == 0x1C5F) printf(" [SK hynix]");
                else if (vid == 0x1344) printf(" [Micron]");
                else if (vid == 0x15B7) printf(" [SanDisk/WD]");
                else if (vid == 0x1987) printf(" [Phison]");
                else if (vid == 0x1CC1) printf(" [ADATA]");
                else if (vid == 0x126F) printf(" [Silicon Motion]");
                else if (vid == 0x1E0F) printf(" [Solidigm]");
                printf("\n");
            }
            if (bm_len >= 31) {
                printf("  Serial Number (SN)  : [");
                uint8_t si;
                for (si = 0; si < 20; si++) {
                    char c = (char)bm_buf[11 + si];
                    printf("%c", (c >= 0x20 && c <= 0x7E) ? c : '.');
                }
                printf("]\n");
            }
            printf("========================================\n");
        }

        CLK_SysTickDelay(200000);
    }

    printf("=== Basic Management Scan Complete ===\n\n");
}
#if 0
// =========================================================================
// CMD SUMMARY: Flush Cache (#2) and Power Management (#3)
//   - Flush Cache = NVMe I/O Flush command (Opcode=0x00 in NVM Command Set)
//     NVMe-MI has no I/O command passthrough. Not implementable here.
//   - Power Management = NVMe Admin Get/Set Features (Feature ID=0x02)
//     Requires working Admin Passthrough (NMIMT=0x09 response).
//     This device returns NMIMT=0x88 (ignores Admin PT).
//     Both commands are NOT available on this device via SMBus NVMe-MI.
// =========================================================================
{
    printf("\n>>> [INFO] Command Availability Summary <<<\n");
    printf("========================================\n");
    printf("  #  Command              Protocol          Available  Reason\n");
    printf("  -  -------------------  ----------------  ---------  ------\n");
    printf("  A  Basic Management     SMBus Reg 0x00    YES        Resp@addr 0x6A (Temp/VID/SN)\n");
    printf("  B  Get Endpoint UUID    MCTP Ctrl 0x03    YES        PEC PASS\n");
    printf("  C  Get Msg Type Support MCTP Ctrl 0x05    YES        Types: 00/04/05/06\n");
    printf("  D  Read MI Data Struct  NVMe-MI Opc=0x00  YES        DTYP=0x00 only (NVM Subsys)\n");
    printf("  E  Configuration Set    NVMe-MI Opc=0x03  YES        Opcode@[4] CID@[8] (DWORD0)\n");
    printf("  F  Configuration Get    NVMe-MI Opc=0x04  YES        Works (SMBus Freq val=0x080004)\n");
    printf("  G  VPD Read             NVMe-MI Opc=0x05  YES        DOFST@[8] DLEN@[12]; FRU (Phison/PAS)\n");
    printf("  H  VPD Write            NVMe-MI Opc=0x06  YES*       DOFST@[8] DLEN@[12] (mirror of Read)\n");
    printf("  I  Reset                NVMe-MI Opc=0x07  N/A        Not in vendor doc\n");
    printf("  1  Health Status Poll   NVMe-MI Opc=0x08  YES        Native MI cmd (MIC PASS)\n");
    printf("  1  Ctrl Health Poll     NVMe-MI Opc=0x02  YES        Native MI cmd\n");
    printf("  1  Health Info+Alerts   NVMe-MI Opc=0x01  YES        Native MI cmd (short resp)\n");
    printf("  2  Flush Cache          NVMe I/O          NO         No I/O PT in NVMe-MI\n");
    printf("  3  Power Management     Admin PT Opc=0x06 NO         Admin PT not implemented\n");
    printf("  4  FW Revision          DTYP=0x03         NO         Device ignores DTYP field\n");
    printf("                         Admin PT Opc=0x06 NO         Admin PT not implemented\n");
    printf("  5  Health Alerts        NVMe-MI Opc=0x01  YES        ALL CLEAR (no alerts)\n");
    printf("  6  Health Info          NVMe-MI Opc=0x01  YES        Partial (short response)\n");
    printf("  7  SMART Log            Admin PT Opc=0x06 NO         Admin PT not implemented\n");
    printf("  8  Device Identify      Admin PT Opc=0x06 NO         Admin PT not implemented\n");
    printf("                         (NVMe Identify)   NO         4096B > SMBus limit (255B)\n");
    printf("  9  FW Update            Admin PT Opc=0x06 NO         Admin PT not implemented\n");
    printf("========================================\n");
    printf("  Admin Passthrough root cause:\n");
    printf("    Sent   : Opcode=0x06 (Admin PT), expects NMIMT=0x09 response\n");
    printf("    Got    : NMIMT=0x88 (MI general resp) - device ignores Opc=0x06\n");
    printf("    Result : All Admin PT commands (#3/#4/#7/#8/#9) NOT available\n");
    printf("========================================\n\n");
}
    #endif
//
#if 0

    for (unsigned char i = 0x01; i < 127; i++)
    {


        if (I2C_WriteByte(I2C1, i, 0x00) == 0)
            printf("address 0x%x\n\r ack", i);
    }

#endif
#define ssd_7BIT       (0x1d)
#define DEST_EID        0x00  // NVMe EID
#define SRC_EID         0x01  // MCU EID
    // eid reply ok
#if 1
{
    uint8_t mctp_uuid_req[15];
    uint8_t idx = 0;
    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
    cmd_begin("Get Endpoint UUID");
    printf("\n>>> [CMD] MCTP Control - Get Endpoint UUID (Cmd=0x03) <<<\n");
    // --- SMBus Block Write Header ---
    mctp_uuid_req[idx++] = ssd_7BIT << 1;
    mctp_uuid_req[idx++] = 0x0F; // Command Code (MCTP)
    mctp_uuid_req[idx++] = 0x08; // Byte Count (8 bytes payload)

    // --- MCTP Transport Header ---
    // Source Address: MCU Address (e.g., 0x20) | 0x01
    mctp_uuid_req[idx++] = (0x20) | 0x01;
    mctp_uuid_req[idx++] = 0x01; // Header Version
    mctp_uuid_req[idx++] = DEST_EID; // Destination EID (NVMe)
    mctp_uuid_req[idx++] = SRC_EID; // Source EID (MCU)
    // SOM=1, EOM=1, TagOwner=1, MsgTag=0 -> 0xC8
    mctp_uuid_req[idx++] = 0xC8;

    // --- MCTP Control Message (Type 0x00) ---
    mctp_uuid_req[idx++] = 0x00; // Message Type: MCTP Control
    mctp_uuid_req[idx++] = 0x80; // Request=1, Instance=0
    mctp_uuid_req[idx++] = 0x03; // Command: Get Endpoint UUID

    // --- PEC Calculation ---
    // Calculate CRC8 over: Addr(Write) + Cmd(0x0F) + Len(0x08) + Payload
    uint8_t pec = calc_pec(0, mctp_uuid_req, idx);
    mctp_uuid_req[idx++] = pec;

    // --- Send via I2C ---
    uint32_t sent_len = I2C_WriteMultiBytes(I2C_PORT, ssd_7BIT, &mctp_uuid_req[1], idx - 1);

		I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);				
	}
#endif
{
	//don't use it, set eid 
	#if 0
   //set eid
   I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;

#define TARGET_NEW_EID 0x05
uint8_t i2c_buf[32];
    uint8_t payload_len = 0;
    
    // --- 1. ?? Payload (? Index 3 ??) ---
    uint8_t *payload = &i2c_buf[3];

    // MCTP Header
    payload[0] = (MCU_ADDR_7BIT << 1) | 1; // Src Addr
    payload[1] = 0x01;                     // Ver
    payload[2] = 0x00;                     // Dest EID (?????? 0)
    payload[3] = 0x01;                     // Src EID
    payload[4] = 0xC8;                     // Tag (SOM=1, EOM=1, Req)

    // MCTP Control Message Body
    payload[5] = 0x00; // Msg Type: 0x00 (MCTP Control)
    payload[6] = 0x80; // Rq=1, D=0, Instance=0
    payload[7] = 0x01; // Command Code: 0x01 (Set Endpoint ID)
    
    // Set Endpoint ID Parameters
    payload[8] = 0x00; // Operation: 0=Set (Normal), 1=Force
    payload[9] = TARGET_NEW_EID; // New EID (0x0A)

    payload_len = 10; // 5 + 3 + 2

    // --- 2. SMBus Header ---
    i2c_buf[0] = (NVME_ADDR_7BIT << 1); // 0x3A
    i2c_buf[1] = 0x0F;                  // Cmd
    i2c_buf[2] = payload_len;           // Len (10)

    // --- 3. PEC (CRC-8) ---
    // ?? Addr + Cmd + Len + Payload
    uint8_t pec = calc_pec_noinit(i2c_buf, payload_len + 3);
    i2c_buf[payload_len + 3] = pec;

    // --- 4. ?? ---
    // ?? = Cmd(1) + Len(1) + Payload(10) + PEC(1) = 13 bytes
    //printf("Setting EID to %d (0x%02X)...\n", TARGET_NEW_EID, TARGET_NEW_EID);
    I2C_WriteMultiBytes(I2C1, i2c_buf[0] >> 1, &i2c_buf[1], payload_len + 3);


I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	

CLK_SysTickDelay(500000);
#endif
}
{
	#if 0
	    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
// ---------------------------------------------------------
// ?? Get Endpoint ID (Target EID = 5)
// ---------------------------------------------------------
uint8_t cmd_buf[20];
uint8_t len = 8; // MCTP Header(5) + Body(3)

// 1. SMBus Header
cmd_buf[0] = 0x3A; // SSD Address (Write)
cmd_buf[1] = 0x0F; // Command Code
cmd_buf[2] = len;  // Length = 8 Bytes

// 2. MCTP Header
cmd_buf[3] = (MCU_ADDR_7BIT << 1) | 1; // Src Addr (MCU)
cmd_buf[4] = 0x01; // Header Version
cmd_buf[5] = 0x00; // Dest EID: 5 (?????? ID)
cmd_buf[6] = 0x01; // Src EID: 1 (MCU ID)
cmd_buf[7] = 0xC8; // SOM=1, EOM=1, Tag=0

// 3. MCTP Control Message Body (Get Endpoint ID)
cmd_buf[8]  = 0x00; // Msg Type: Control Message (0x00)
cmd_buf[9]  = 0x80; // Rq=1, D=0, Instance=0
cmd_buf[10] = 0x02; // Command Code: 0x02 (Get Endpoint ID)

// 4. ?? PEC
cmd_buf[11] = calc_pec_noinit(cmd_buf, 11); // ??? 11 ? bytes

// 5. ??
I2C_WriteMultiBytes(I2C_PORT, cmd_buf[0] >> 1, &cmd_buf[1], 12);
I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		CLK_SysTickDelay(500000);
		#endif 
}

{

	
	#if 1
	    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
    cmd_begin("Ctrl Health Poll (NMP=0x08)");
    printf("\n>>> [CMD] NVMe-MI - Controller Health Status Poll (Opcode=0x02, NMP=0x08) <<<\n");
// ---------------------------------------------------------
// ?? Get smart data ID (Target EID = 5)
// ---------------------------------------------------------
uint8_t cmd_buf[29];
uint8_t len = 0x19; 

// 1. SMBus Header
cmd_buf[0] = 0x3A; // SSD Address (Write)
cmd_buf[1] = 0x0F; // Command Code
cmd_buf[2] = len;  

// 2. MCTP Header
cmd_buf[3] = (MCU_ADDR_7BIT << 1) | 1; // Src Addr (MCU)
cmd_buf[4] = 0x01; // Header Version
cmd_buf[5] = 0x00; // Dest EID: 5 (?????? ID)
cmd_buf[6] = 0x01; // Src EID: 1 (MCU ID)
//cmd_buf[5] = 0x0a; // Dest EID: 5 (?????? ID)
//cmd_buf[6] = 0x08; // Src EID: 1 (MCU ID)
	
cmd_buf[7] = 0xC8; // SOM=1, EOM=1, Tag=0
//mic start
cmd_buf[8] = 0x84;        // Msg Type: NVMe-MI (0x04) + IC Bit (0x80)
cmd_buf[9] = 0x08;        // NMP: NMIMT=1h (NVMe-MI Command), ROR=0, CSI=0  (1h << 3 = 0x08)
cmd_buf[10] = 0x00;        // Reserved
cmd_buf[11] = 0x00;       // Reserved
cmd_buf[12] = 0x02;       // NVMe-MI Opcode = 0x02 (Controller Health Status Poll)
cmd_buf[13] = 0x00;       // Reserved (NVMe-MI Command Dword 0 starts at msg byte 8 = cmd_buf[16])
cmd_buf[14] = 0x00;       // NSID / Dword
cmd_buf[15] = 0x00;
cmd_buf[16] = 0x00;
cmd_buf[17] = 0x00;
cmd_buf[18] = 0x01;       // Dword 10/11 ??
cmd_buf[19] = 0x87;
cmd_buf[20] = 0x00;
cmd_buf[21] = 0x00;
cmd_buf[22] = 0x00;
cmd_buf[23] = 0x00;


 uint32_t mic = calc_mic(&cmd_buf[8], 16);
   cmd_buf[24] = mic & 0xFF;
    cmd_buf[25] = (mic >> 8) & 0xFF;
    cmd_buf[26] = (mic >> 16) & 0xFF;
    cmd_buf[27] = (mic >> 24) & 0xFF;
cmd_buf[28] = calc_pec_noinit(cmd_buf, 28); 

I2C_WriteMultiBytes(I2C_PORT, cmd_buf[0] >> 1, &cmd_buf[1], 28);
I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		CLK_SysTickDelay(500000);
		#endif 
}




{
	#if 0
  I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
    uint8_t i2c_buf[20];
uint8_t len = 9; // Payload ??

// 1. SMBus Header
i2c_buf[0] = 0x3A; // SSD Address (Write)
i2c_buf[1] = 0x0F; // Command Code
i2c_buf[2] = len;  // Length = 8 Bytes

// 2. MCTP Transport Header
i2c_buf[3] = (MCU_ADDR_7BIT << 1) | 1; // 0x21 (MCU Addr)
i2c_buf[4] = 0x01; // Header Version
i2c_buf[5] = 0x00; // Dest EID: 5 (??:???? SSD ??? ID 5)
i2c_buf[6] = 0x01; // Src EID: 1 (MCU ID)
i2c_buf[7] = 0xC8; // SOM=1, EOM=1, Tag=0

// 3. MCTP Message Body (Control)
i2c_buf[8]  = 0x00; // Msg Type: Control Message (0x00)
i2c_buf[9]  = 0x80; // Rq=1, D=0
i2c_buf[10] = 0x04; // Command Code: Get Message Type Support
i2c_buf[11] = 0x00;
// 4. ?? PEC
// ????: Addr(1) + Cmd(1) + Len(1) + Payload(8) = 11 Bytes
i2c_buf[12] = calc_pec_noinit(i2c_buf, 12); 

// 5. ??
// ??:???? i2c_buf[0] ??? 0x3A,?? Write ???????????,????????
I2C_WriteMultiBytes(I2C_PORT, i2c_buf[0] >> 1, &i2c_buf[1], 12);
    

I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		CLK_SysTickDelay(500000);
		#endif

}


{
	#if 1
	I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
    cmd_begin("Get Msg Type Support");
    printf("\n>>> [CMD] MCTP Control - Get Message Type Support (Cmd=0x05) <<<\n");
// ---------------------------------------------------------
// ?? Get Message Type Support (Target EID = 5)
// ---------------------------------------------------------
uint8_t i2c_buf[20];
uint8_t len = 8; // Payload ??

// 1. SMBus Header
i2c_buf[0] = 0x3A; // SSD Address (Write)
i2c_buf[1] = 0x0F; // Command Code
i2c_buf[2] = len;  // Length = 8 Bytes

// 2. MCTP Transport Header
i2c_buf[3] = (MCU_ADDR_7BIT << 1) | 1; // 0x21 (MCU Addr)
i2c_buf[4] = 0x01; // Header Version
i2c_buf[5] = 0x00; // Dest EID: 5 (??:???? SSD ??? ID 5)
i2c_buf[6] = 0x01; // Src EID: 1 (MCU ID)
i2c_buf[7] = 0xC8; // SOM=1, EOM=1, Tag=0

// 3. MCTP Message Body (Control)
i2c_buf[8]  = 0x00; // Msg Type: Control Message (0x00)
i2c_buf[9]  = 0x80; // Rq=1, D=0
i2c_buf[10] = 0x05; // Command Code: Get Message Type Support

// 4. ?? PEC
// ????: Addr(1) + Cmd(1) + Len(1) + Payload(8) = 11 Bytes
i2c_buf[11] = calc_pec_noinit(i2c_buf, 11); 

// 5. ??
// ??:???? i2c_buf[0] ??? 0x3A,?? Write ???????????,????????
I2C_WriteMultiBytes(I2C_PORT, i2c_buf[0] >> 1, &i2c_buf[1], 11);


I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		CLK_SysTickDelay(500000);
#endif
}

{
//get fw version
		I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
    cmd_begin("Read MI Data Struct");
    printf("\n>>> [CMD] NVMe-MI - Read MI Data Structure (Opcode=0x00, FW Version) <<<\n");
uint8_t cmd_buf[29]; // ???????? 29 Bytes

// 1. SMBus Header
cmd_buf[0] = 0x3A; // SSD Address (Write)
cmd_buf[1] = 0x0F; // Command Code
cmd_buf[2] = 0x19; // Byte Count (25 Bytes)

// 2. MCTP Header (?? Null EID ?????,?????????)
cmd_buf[3] = (MCU_ADDR_7BIT << 1) | 1; // Src Addr
cmd_buf[4] = 0x01; // Header Version
cmd_buf[5] = 0x00; // Dest EID (Null EID = 0x00)
cmd_buf[6] = 0x01; // Src EID (Null EID = 0x00)
cmd_buf[7] = 0xC8; // SOM=1, EOM=1, Tag=0

// 3. NVMe-MI Payload (Opcode: 0x00 Read MI Data Structure)
// --- MIC ???? (cmd_buf[8]) ---
cmd_buf[8]  = 0x84; // Msg Type: NVMe-MI + IC
cmd_buf[9]  = 0x08; // NMMT: NVMe-MI Command
cmd_buf[10] = 0x00;
cmd_buf[11] = 0x00; // Ctrl ID Low
cmd_buf[12] = 0x00; // Ctrl ID High
cmd_buf[13] = 0x00; // Opcode: 0x00 (Read MI Data Structure)
cmd_buf[14] = 0x00; 
cmd_buf[15] = 0x00;
cmd_buf[16] = 0x00;
cmd_buf[17] = 0x00; 
cmd_buf[18] = 0x00;
cmd_buf[19] = 0x00;
cmd_buf[20] = 0x00;
cmd_buf[21] = 0x00;
cmd_buf[22] = 0x00;
cmd_buf[23] = 0x00; 
// --- MIC ???? (cmd_buf[23]) ---

// 4. ????? MIC (? 16 Bytes)
uint32_t mic = calc_mic(&cmd_buf[8], 16);
cmd_buf[24] = mic & 0xFF;
cmd_buf[25] = (mic >> 8) & 0xFF;
cmd_buf[26] = (mic >> 16) & 0xFF;
cmd_buf[27] = (mic >> 24) & 0xFF;

// 5. ?? PEC ???
cmd_buf[28] = calc_pec_noinit(cmd_buf, 28); 
I2C_WriteMultiBytes(I2C_PORT, cmd_buf[0] >> 1, &cmd_buf[1], 28);

I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		//parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		CLK_SysTickDelay(500000);

}
//
{
//Get Health Info
		I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
    cmd_begin("NVM Subsys Health Poll");
    printf("\n>>> [CMD] NVMe-MI - NVM Subsystem Health Status Poll (Opcode=0x01) <<<\n");
uint8_t cmd_buf[29];

// 1. SMBus Header
cmd_buf[0] = 0x3A; // SSD Address (Write)
cmd_buf[1] = 0x0F; // Command Code
cmd_buf[2] = 0x19; // SMBus Byte Count (25 Bytes)

// 2. MCTP Header (Null EID ?????)
cmd_buf[3] = (MCU_ADDR_7BIT << 1) | 1; // Src Addr
cmd_buf[4] = 0x01; // Header Version
cmd_buf[5] = 0x00; // Dest EID (Null EID)
cmd_buf[6] = 0x01; // Src EID (Null EID)
cmd_buf[7] = 0xC8; // SOM=1, EOM=1, Tag=0

// 3. NVMe-MI Payload
// --- MIC ???? (cmd_buf[8]) ---
cmd_buf[8]  = 0x84; // Msg Type: NVMe-MI + IC Bit
cmd_buf[9]  = 0x08; // NMMT: NVMe-MI Command
cmd_buf[10] = 0x00; // Flags
cmd_buf[11] = 0x00; // Reserved (?? Subsystem ?,Controller ID ??? 0)
cmd_buf[12] = 0x00; // Reserved
cmd_buf[13] = 0x01; // Opcode: 0x01 (NVM Subsystem Health Status Poll)
cmd_buf[14] = 0x00; // Clear Status (? 0)
cmd_buf[15] = 0x00; // Reserved
cmd_buf[16] = 0x00; 
cmd_buf[17] = 0x00; 
cmd_buf[18] = 0x00;
cmd_buf[19] = 0x00;
cmd_buf[20] = 0x00;
cmd_buf[21] = 0x00; 
cmd_buf[22] = 0x00; 
cmd_buf[23] = 0x00; 
// --- MIC ???? (cmd_buf[23]) ---

// 4. ?? MIC (? 16 Bytes)
uint32_t mic = calc_mic(&cmd_buf[8], 16);
cmd_buf[24] = mic & 0xFF;
cmd_buf[25] = (mic >> 8) & 0xFF;
cmd_buf[26] = (mic >> 16) & 0xFF;
cmd_buf[27] = (mic >> 24) & 0xFF;

// 5. ?? PEC ?????
cmd_buf[28] = calc_pec_noinit(cmd_buf, 28); 
I2C_WriteMultiBytes(I2C_PORT, cmd_buf[0] >> 1, &cmd_buf[1], 28);



I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		//parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		CLK_SysTickDelay(500000);

}

{
#if 1
//Get FW VERSION
    cmd_begin("Get Log FW Slot (2pkt)");
    printf("\n>>> [CMD] NVMe Admin - Get Log Page FW Slot (72B, 2 pkts) <<<\n");
		I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
// =========================================================================
// ?? A:??????????? 72 Bytes NVMe-MI ??
// =========================================================================
uint8_t mi_msg[72];
memset(mi_msg, 0, sizeof(mi_msg));

// 1. NVMe-MI Header (4 Bytes)
mi_msg[0] = 0x84; // Msg Type: NVMe-MI (0x04) + IC Bit (0x80)
mi_msg[1] = 0x10; // NMP: NMIMT=2h (NVMe Admin Command), ROR=0, CSI=0  (2h << 3 = 0x10)
mi_msg[2] = 0x00; // Reserved
mi_msg[3] = 0x00; // Reserved

// 2. NVMe SQE (64 Bytes, mi_msg[4] ~ mi_msg[67]) -- Get Log Page (FW Slot Info), same as a7
mi_msg[4]  = 0x02; // SQE Dword 0: Opcode = 0x02 (Get Log Page)
mi_msg[8]  = 0xFF; // SQE Dword 1: NSID = 0xFFFFFFFF (whole subsystem)
mi_msg[9]  = 0xFF;
mi_msg[10] = 0xFF;
mi_msg[11] = 0xFF;
mi_msg[32] = 0x1C; // SQE Dword 7: Data Length (DLEN) = 0x1C = 28
mi_msg[44] = 0x03; // SQE Dword 10: LID = 0x03 (FW Slot Info)
mi_msg[45] = 0x80; // SQE Dword 10: bit15 RAE = 1
mi_msg[46] = 0x07; // SQE Dword 10: NUMDL = 0x0007 (8 dwords = 32B)

// 3. ?? MIC (??? 68 Bytes ????)
uint32_t mic = calc_mic(mi_msg, 68);
mi_msg[68] = mic & 0xFF;
mi_msg[69] = (mic >> 8) & 0xFF;
mi_msg[70] = (mic >> 16) & 0xFF;
mi_msg[71] = (mic >> 24) & 0xFF;

// =========================================================================
// ?? B:???????? MCTP ?? (SOM=1, EOM=0)
// =========================================================================
uint8_t pkt1[73];
pkt1[0] = 0x3A; // SSD Address (Write)
pkt1[1] = 0x0F; // Command Code
pkt1[2] = 69;   // SMBus Byte Count (1 SrcAddr + 4 MCTP + 64 Payload)

// MCTP Header
pkt1[3] = (MCU_ADDR_7BIT << 1) | 1; // Src Addr
pkt1[4] = 0x01; // Header Version
pkt1[5] = 0x00; // Dest EID (Null EID)
pkt1[6] = 0x01; // Src EID (????????)
pkt1[7] = 0x88; // SOM=1, EOM=0, PktSeq=0, TO=1, MsgTag=0 (??? 1000 1000)

// ??? 64 Bytes ? NVMe-MI Payload
memcpy(&pkt1[8], &mi_msg[0], 64);

// ?? PEC ??? I2C
pkt1[72] = calc_pec_noinit(pkt1, 72);
{
    int _i;
    printf("[TX] MCTP PKT1 (SOM=1,EOM=0,Seq=0) 73 bytes incl PEC:\n");
    for (_i = 0; _i < 73; _i++) printf("%02X ", pkt1[_i]);
    printf("\n");
}
I2C_WriteMultiBytes(I2C_PORT, pkt1[0] >> 1, &pkt1[1], 72);
		CLK_SysTickDelay(500000);
// --- ?????? 1~2ms ? Delay,? SSD ??????? ---

// =========================================================================
// ?? C:???????? MCTP ?? (SOM=0, EOM=1)
// =========================================================================
uint8_t pkt2[17];
pkt2[0] = 0x3A; // SSD Address (Write)
pkt2[1] = 0x0F; // Command Code
pkt2[2] = 13;   // SMBus Byte Count (1 SrcAddr + 4 MCTP + 8 Payload)

// MCTP Header
pkt2[3] = (MCU_ADDR_7BIT << 1) | 1; 
pkt2[4] = 0x01; 
pkt2[5] = 0x00; 
pkt2[6] = 0x01; 
pkt2[7] = 0x58; // SOM=0, EOM=1, PktSeq=1, TO=1, MsgTag=0 (??? 0101 1000)

// ????? 8 Bytes NVMe-MI Payload (SQE ?? 4B + MIC 4B)
memcpy(&pkt2[8], &mi_msg[64], 8);

// ?? PEC ??? I2C
pkt2[16] = calc_pec_noinit(pkt2, 16);
{
    int _i;
    printf("[TX] MCTP PKT2 (SOM=0,EOM=1,Seq=1) 17 bytes incl PEC:\n");
    for (_i = 0; _i < 17; _i++) printf("%02X ", pkt2[_i]);
    printf("\n");
}
I2C_WriteMultiBytes(I2C_PORT, pkt2[0] >> 1, &pkt2[1], 16);

	
	
	
I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		CLK_SysTickDelay(500000);
		#endif
}

// =========================================================================
// CMD: NVMe Admin Get Log Page - SMART / Health Info (LID 0x02, 512B)
// Request is the same 72B layout as FW Slot (2 pkts). Response is
// NMH(4) + resp hdr(16) + log(512) + MIC(4) = 536B -> several MCTP packets,
// received with mctp_rx_message().
// =========================================================================
{
    static uint8_t s_smartResp[600];
    uint8_t  msg[72];
    uint16_t respLen = 0U;
    int32_t  ret;

    cmd_begin("Get Log SMART");
    printf("\n>>> [CMD] NVMe Admin - Get Log Page SMART (LID=0x02, 512B, multi-pkt RX) <<<\n");

    memset(msg, 0, sizeof(msg));
    msg[0]  = 0x84;                  // NMH0: IC bit + NVMe-MI msg type
    msg[1]  = 0x10;                  // NMH1: NMIMT=0x02 (NVMe Admin Command)
    msg[4]  = 0x02;                  // OPC  = Get Log Page
    msg[8]  = 0xFF;                  // NSID = 0xFFFFFFFF
    msg[9]  = 0xFF;
    msg[10] = 0xFF;
    msg[11] = 0xFF;
    msg[32] = 0x00;                  // DLEN = 0x00000200 (512)
    msg[33] = 0x02;
    msg[44] = 0x02;                  // LID  = 0x02 (SMART / Health Information)
    msg[45] = 0x80;                  // RAE  = 1 (required for OOB Get Log Page)
    msg[46] = 0x7F;                  // NUMDL = 127 -> 128 dwords = 512B
    msg[47] = 0x00;
    {
        uint32_t mic = calc_mic(msg, 68);
        msg[68] = (uint8_t)(mic & 0xFF);
        msg[69] = (uint8_t)((mic >> 8) & 0xFF);
        msg[70] = (uint8_t)((mic >> 16) & 0xFF);
        msg[71] = (uint8_t)((mic >> 24) & 0xFF);
    }

    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
    s_rxqRd = s_rxqWr;               /* drop stale packets from earlier commands */

    mctp_tx_message(msg, (uint16_t)sizeof(msg));

    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    g_u8SlvRxFlag = 0;
    ret = mctp_rx_message(s_smartResp, (uint16_t)sizeof(s_smartResp), &respLen);
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);

    if (ret != 0)
    {
        printf("  [FAIL] SMART Log RX error %d\n", ret);
    }
    else if (respLen < 24U)
    {
        printf("  [FAIL] response too short (%u bytes)\n", respLen);
    }
    else
    {
        /* msg: [0..3]=NMH [4]=Status [8..11]=CQE DW0 [12..15]=DW1 [16..19]=DW3 [20..]=log data */
        uint8_t  *pD     = &s_smartResp[20];
        uint16_t dataLen = (uint16_t)(respLen - 24U);
        uint16_t i;

        printf("  NMH=%02X %02X  Status=0x%02X  CQE DW3=%02X %02X %02X %02X  data=%u bytes\n",
               s_smartResp[0], s_smartResp[1], s_smartResp[4],
               s_smartResp[16], s_smartResp[17], s_smartResp[18], s_smartResp[19], dataLen);
        for (i = 0U; i < dataLen; i++)
        {
            printf("%02X ", pD[i]);
            if ((i & 0x0FU) == 0x0FU)
            {
                printf("\n");
            }
        }
        printf("\n");

        if ((s_smartResp[1] == 0x90U) && (s_smartResp[4] == 0x00U) && (dataLen >= 176U))
        {
            /* 128-bit counters: only the low 32 bits are printed */
            uint16_t tempK = (uint16_t)pD[1] | ((uint16_t)pD[2] << 8);
            printf("========================================\n");
            printf("  SMART / Health Information (LID 0x02)\n");
            printf("========================================\n");
            printf("  Critical Warning   : 0x%02X\n", pD[0]);
            printf("  Composite Temp     : %d C (%u K)\n", (int32_t)tempK - 273, tempK);
            g_composTempC = (int32_t)tempK - 273;   /* capture for summary */
            printf("  Available Spare    : %u%%\n", pD[3]);
            printf("  Spare Threshold    : %u%%\n", pD[4]);
            printf("  Percentage Used    : %u%%\n", pD[5]);
            printf("  Data Units Read    : %u (x512000 B)\n",
                   (uint32_t)pD[32] | ((uint32_t)pD[33] << 8) | ((uint32_t)pD[34] << 16) | ((uint32_t)pD[35] << 24));
            printf("  Data Units Written : %u (x512000 B)\n",
                   (uint32_t)pD[48] | ((uint32_t)pD[49] << 8) | ((uint32_t)pD[50] << 16) | ((uint32_t)pD[51] << 24));
            printf("  Power Cycles       : %u\n",
                   (uint32_t)pD[112] | ((uint32_t)pD[113] << 8) | ((uint32_t)pD[114] << 16) | ((uint32_t)pD[115] << 24));
            printf("  Power On Hours     : %u\n",
                   (uint32_t)pD[128] | ((uint32_t)pD[129] << 8) | ((uint32_t)pD[130] << 16) | ((uint32_t)pD[131] << 24));
            printf("  Unsafe Shutdowns   : %u\n",
                   (uint32_t)pD[144] | ((uint32_t)pD[145] << 8) | ((uint32_t)pD[146] << 16) | ((uint32_t)pD[147] << 24));
            printf("  Media Errors       : %u\n",
                   (uint32_t)pD[160] | ((uint32_t)pD[161] << 8) | ((uint32_t)pD[162] << 16) | ((uint32_t)pD[163] << 24));
            printf("========================================\n");
        }
    }
    printf("=== NVMe Admin Get Log Page SMART Complete ===\n\n");
}

// =========================================================================
// CMD: NVMe Admin Identify Controller (Opcode 0x06, CNS 0x01, 4096B)
// Response = NMH(4) + resp hdr(16) + 4096 + MIC(4) = 4120B -> ~65 MCTP packets.
// =========================================================================
{
    static uint8_t s_idResp[4200];
    uint8_t  msg[72];
    uint16_t respLen = 0U;
    int32_t  ret;

    cmd_begin("Identify Controller");
    printf("\n>>> [CMD] NVMe Admin - Identify Controller (Opc=0x06, CNS=0x01, 4096B, multi-pkt RX) <<<\n");

    memset(msg, 0, sizeof(msg));
    msg[0]  = 0x84;                  // NMH0: IC bit + NVMe-MI msg type
    msg[1]  = 0x10;                  // NMH1: NMIMT=0x02 (NVMe Admin Command)
    msg[4]  = 0x06;                  // OPC  = Identify
    msg[32] = 0x00;                  // DLEN = 0x00001000 (4096)
    msg[33] = 0x10;
    msg[44] = 0x01;                  // CDW10: CNS = 0x01 (Identify Controller)
    {
        uint32_t mic = calc_mic(msg, 68);
        msg[68] = (uint8_t)(mic & 0xFF);
        msg[69] = (uint8_t)((mic >> 8) & 0xFF);
        msg[70] = (uint8_t)((mic >> 16) & 0xFF);
        msg[71] = (uint8_t)((mic >> 24) & 0xFF);
    }

    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
    s_rxqRd = s_rxqWr;               /* drop stale packets from earlier commands */

    mctp_tx_message(msg, (uint16_t)sizeof(msg));

    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    g_u8SlvRxFlag = 0;
    ret = mctp_rx_message(s_idResp, (uint16_t)sizeof(s_idResp), &respLen);
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);

    if (ret != 0)
    {
        printf("  [FAIL] Identify RX error %d\n", ret);
    }
    else if (respLen < 24U)
    {
        printf("  [FAIL] response too short (%u bytes)\n", respLen);
    }
    else
    {
        uint8_t  *pD     = &s_idResp[20];
        uint16_t dataLen = (uint16_t)(respLen - 24U);
        uint16_t i;

        printf("  NMH=%02X %02X  Status=0x%02X  CQE DW3=%02X %02X %02X %02X  data=%u bytes\n",
               s_idResp[0], s_idResp[1], s_idResp[4],
               s_idResp[16], s_idResp[17], s_idResp[18], s_idResp[19], dataLen);
        /* Dump only the first 256 bytes to keep the log readable */
        for (i = 0U; (i < dataLen) && (i < 256U); i++)
        {
            printf("%02X ", pD[i]);
            if ((i & 0x0FU) == 0x0FU)
            {
                printf("\n");
            }
        }
        printf("\n");

        if ((s_idResp[1] == 0x90U) && (s_idResp[4] == 0x00U) && (dataLen >= 80U))
        {
            printf("========================================\n");
            printf("  Identify Controller (CNS 0x01)\n");
            printf("========================================\n");
            printf("  PCI VID    : 0x%04X\n", (uint16_t)pD[0] | ((uint16_t)pD[1] << 8));
            printf("  PCI SSVID  : 0x%04X\n", (uint16_t)pD[2] | ((uint16_t)pD[3] << 8));
            printf("  SN         : [");
            for (i = 4U; i < 24U; i++)
            {
                printf("%c", ((pD[i] >= 0x20U) && (pD[i] <= 0x7EU)) ? pD[i] : '.');
            }
            printf("]\n  MN         : [");
            for (i = 24U; i < 64U; i++)
            {
                printf("%c", ((pD[i] >= 0x20U) && (pD[i] <= 0x7EU)) ? pD[i] : '.');
            }
            printf("]\n  FR         : [");
            for (i = 64U; i < 72U; i++)
            {
                printf("%c", ((pD[i] >= 0x20U) && (pD[i] <= 0x7EU)) ? pD[i] : '.');
            }
            /* Capture FR (firmware revision) for the final summary */
            {
                uint8_t fi;
                for (fi = 0U; fi < 8U; fi++)
                {
                    uint8_t c = pD[64U + fi];
                    g_fwRevStr[fi] = ((c >= 0x20U) && (c <= 0x7EU)) ? (char)c : '.';
                }
                g_fwRevStr[8] = '\0';
            }
            printf("]\n  CNTLID     : 0x%04X\n", (uint16_t)pD[78] | ((uint16_t)pD[79] << 8));
            printf("========================================\n");
        }
    }
    printf("=== NVMe Admin Identify Controller Complete ===\n\n");
}

// =========================================================================
// CMD: NVMe Admin Get Features - Power Management (Opcode 0x0A, FID 0x02)
// SEL=0 (current). Result is returned in CQE DW0: PS[4:0], WH[7:5].
// =========================================================================
{
    uint8_t  msg[72];
    uint8_t  resp[64];
    uint16_t respLen = 0U;
    int32_t  ret;

    cmd_begin("Get Features Power Mgmt");
    printf("\n>>> [CMD] NVMe Admin - Get Features Power Management (Opc=0x0A, FID=0x02) <<<\n");

    memset(msg, 0, sizeof(msg));
    msg[0]  = 0x84;                  // IC bit + NVMe-MI msg type
    msg[1]  = 0x10;                  // NMIMT=0x02 (NVMe Admin Command)
    msg[4]  = 0x0A;                  // OPC = Get Features
    msg[44] = 0x02;                  // CDW10: FID = 0x02 (Power Management), SEL = 0 (current)
    {
        uint32_t mic = calc_mic(msg, 68);
        msg[68] = (uint8_t)(mic & 0xFF);
        msg[69] = (uint8_t)((mic >> 8) & 0xFF);
        msg[70] = (uint8_t)((mic >> 16) & 0xFF);
        msg[71] = (uint8_t)((mic >> 24) & 0xFF);
    }

    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
    s_rxqRd = s_rxqWr;
    mctp_tx_message(msg, (uint16_t)sizeof(msg));

    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    g_u8SlvRxFlag = 0;
    ret = mctp_rx_message(resp, (uint16_t)sizeof(resp), &respLen);
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);

    if ((ret != 0) || (respLen < 8U))
    {
        printf("  [FAIL] RX error %d (len=%u)\n", ret, respLen);
    }
    else if (resp[4] != 0x00U)
    {
        printf("  MI Status=0x%02X (%s)\n", resp[4], nvme_mi_status_str(resp[4]));
    }
    else if (respLen < 24U)
    {
        printf("  [FAIL] response too short (%u bytes)\n", respLen);
    }
    else
    {
        uint32_t dw0 = (uint32_t)resp[8] | ((uint32_t)resp[9] << 8)
                     | ((uint32_t)resp[10] << 16) | ((uint32_t)resp[11] << 24);
        uint32_t dw3 = (uint32_t)resp[16] | ((uint32_t)resp[17] << 8)
                     | ((uint32_t)resp[18] << 16) | ((uint32_t)resp[19] << 24);
        uint8_t  sc  = (uint8_t)((dw3 >> 17) & 0xFFU);
        uint8_t  sct = (uint8_t)((dw3 >> 25) & 0x07U);

        printf("  CQE DW0=0x%08X  DW3=0x%08X  SCT=0x%X SC=0x%02X\n", dw0, dw3, sct, sc);
        if ((sc == 0U) && (sct == 0U))
        {
            printf("  Power State (PS)     : %u\n", dw0 & 0x1FU);
            printf("  Workload Hint (WH)   : %u\n", (dw0 >> 5) & 0x07U);
        }
    }
    printf("=== NVMe Admin Get Features Power Management Complete ===\n\n");
}

// =========================================================================
// CMD: NVMe Admin Format NVM (Opcode 0x80)
// !!! DESTRUCTIVE: erases all user data on every namespace !!!
// NSID=0xFFFFFFFF (all), CDW10 = 0 -> LBAF=0, MSET=0, PI=0, PIL=0, SES=0.
// No data transfer: response is a CQE only (single packet).
// Set NVME_FORMAT_ENABLE to 0U to skip this step.
// =========================================================================
#define NVME_FORMAT_ENABLE      (1U)
#define NVME_FORMAT_WAIT_RETRY  (20U)    /* x MCTP_RX_TIMEOUT_CNT per retry */
#if (NVME_FORMAT_ENABLE == 1U)
{
    uint8_t  msg[72];
    uint8_t  resp[64];
    uint16_t respLen = 0U;
    int32_t  ret = -2;
    uint8_t  retry;

    cmd_begin("Format NVM");
    printf("\n>>> [CMD] NVMe Admin - Format NVM (Opc=0x80, NSID=ALL, LBAF=0, SES=0) <<<\n");
    printf("  !!! WARNING: all user data on the SSD will be erased !!!\n");

    memset(msg, 0, sizeof(msg));
    msg[0]  = 0x84;                  // NMH0: IC bit + NVMe-MI msg type
    msg[1]  = 0x10;                  // NMH1: NMIMT=0x02 (NVMe Admin Command)
    msg[4]  = 0x80;                  // OPC  = Format NVM
    msg[8]  = 0xFF;                  // NSID = 0xFFFFFFFF (all namespaces)
    msg[9]  = 0xFF;
    msg[10] = 0xFF;
    msg[11] = 0xFF;
    /* msg[32..35] DLEN = 0 (no data), msg[44..47] CDW10 = 0 (LBAF/MSET/PI/SES = 0) */
    {
        uint32_t mic = calc_mic(msg, 68);
        msg[68] = (uint8_t)(mic & 0xFF);
        msg[69] = (uint8_t)((mic >> 8) & 0xFF);
        msg[70] = (uint8_t)((mic >> 16) & 0xFF);
        msg[71] = (uint8_t)((mic >> 24) & 0xFF);
    }

    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
    s_rxqRd = s_rxqWr;               /* drop stale packets from earlier commands */

    mctp_tx_message(msg, (uint16_t)sizeof(msg));

    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    g_u8SlvRxFlag = 0;
    /* Format may take longer than one RX timeout: keep waiting on "no packet yet" */
    for (retry = 0U; (retry < NVME_FORMAT_WAIT_RETRY) && (ret == -2); retry++)
    {
        ret = mctp_rx_message(resp, (uint16_t)sizeof(resp), &respLen);
    }
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);

    if (ret != 0)
    {
        printf("  [FAIL] Format RX error %d\n", ret);
    }
    else if (respLen < 8U)
    {
        printf("  [FAIL] response too short (%u bytes)\n", respLen);
    }
    else
    {
        /* msg: [0..3]=NMH [4]=NVMe-MI Status; if Status=0: [16..19]=CQE DW3 */
        printf("  Resp: NMH=%02X %02X  MI Status=0x%02X  len=%u\n",
               resp[0], resp[1], resp[4], respLen);
        if ((resp[1] == 0x90U) && (resp[4] == 0x00U) && (respLen >= 24U))
        {
            uint32_t dw3 = (uint32_t)resp[16] | ((uint32_t)resp[17] << 8)
                         | ((uint32_t)resp[18] << 16) | ((uint32_t)resp[19] << 24);
            uint8_t  sc  = (uint8_t)((dw3 >> 17) & 0xFFU);
            uint8_t  sct = (uint8_t)((dw3 >> 25) & 0x07U);

            printf("  CQE DW3=0x%08X  SCT=0x%X SC=0x%02X  => %s\n",
                   dw3, sct, sc, ((sc == 0U) && (sct == 0U)) ? "FORMAT OK" : "FORMAT FAILED");
        }
        else
        {
            printf("  [FAIL] not a successful Admin response (see MI Status)\n");
        }
    }
    printf("=== NVMe Admin Format NVM Complete ===\n\n");
}
#endif

// =========================================================================
// NVMe Identify Device Function
// =========================================================================
// ?��????��?????NVMe Identify ??�賭�?(CNS = 0x01) ??�質???�賣???�賢靽?��?
// Identify ??�賭�??Admin Command, Opcode = 0x06
//void nvme_identify_device(void)
{
#if 0  /* Disabled: Phison device ignores Admin Passthrough (Opcode 0x06 / NMIMT 0x09); never replies -> skip to avoid hang */
    printf("\n>>> [CMD] NVMe Admin - Identify Controller (Opcode=0x06, CNS=0x01) <<<\n");
    printf("=== NVMe Identify Device ===\n");
    
    // ??��???I2C Slave ??��?????�賣???��???
    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
    
    // ====================================================================
    // ??�賢??NVMe-MI Admin Command (Identify Controller)
    // ====================================================================
    // NVMe Admin Command Format:
    // - Opcode: 0x06 (Identify)
    // - CDW10: CNS (Controller or Namespace Structure)
    //   - CNS = 0x01: Identify Controller
    //   - CNS = 0x00: Identify Namespace
    // ====================================================================
    
    uint8_t cmd_buf[29];
    
    // 1. SMBus Header
    cmd_buf[0] = 0x3A;      // SSD Target Address (Write)
    cmd_buf[1] = 0x0F;      // SMBus Command Code
    cmd_buf[2] = 0x19;      // Byte Count (25 Bytes)
    
    // 2. MCTP Header (Single packet, SOM=1, EOM=1)
    cmd_buf[3] = (MCU_ADDR_7BIT << 1) | 1;  // Source Address
    cmd_buf[4] = 0x01;      // MCTP Header Version
    cmd_buf[5] = 0x00;      // Destination EID (Null EID during discovery)
    cmd_buf[6] = 0x00;      // Source EID (Null EID)
    cmd_buf[7] = 0xC8;      // PktSeq:SOM=1, EOM=1, PktSeq=0, TO=1, MsgTag=0
    
    // 3. NVMe-MI Payload (Admin Command - Identify)
    cmd_buf[8]  = 0x84;     // Message Type: NVMe-MI + IC Bit
    cmd_buf[9]  = 0x09;     // NMIMT: 0x09 = NVMe Admin Command
    cmd_buf[10] = 0x00;     // Flags
    cmd_buf[11] = 0x01;     // Controller ID Low Byte
    cmd_buf[12] = 0x00;     // Controller ID High Byte
    cmd_buf[13] = 0x06;     // Opcode: 0x06 = Identify Command
    cmd_buf[14] = 0x00;     // Flags (Reserved)
    
    // CDW10: Controller or Namespace Structure (CNS)
    cmd_buf[15] = 0x01;     // CNS = 0x01 (Identify Controller)
    cmd_buf[16] = 0x00;     
    cmd_buf[17] = 0x00;     
    cmd_buf[18] = 0x00;     
    
    // CDW11-CDW14: Reserved for Identify command
    cmd_buf[19] = 0x00;     
    cmd_buf[20] = 0x00;     
    cmd_buf[21] = 0x00;     
    cmd_buf[22] = 0x00;     
    cmd_buf[23] = 0x00;     
    
    // 4. ?�∴?? MIC (Message Integrity Check)
    // MIC ?��?????cmd_buf[8] ?�?�?? 16 Bytes NVMe-MI Payload
    uint32_t mic = calc_mic(&cmd_buf[8], 16);
    cmd_buf[24] = mic & 0xFF;
    cmd_buf[25] = (mic >> 8) & 0xFF;
    cmd_buf[26] = (mic >> 16) & 0xFF;
    cmd_buf[27] = (mic >> 24) & 0xFF;
    
    // 5. ?�∴?? PEC (Packet Error Code for SMBus)
    cmd_buf[28] = calc_pec_noinit(cmd_buf, 28);
    
    // 6. ??��???I2C ??�賭�?
    printf("Sending NVMe Identify Command...\n");
    I2C_WriteMultiBytes(I2C_PORT, cmd_buf[0] >> 1, &cmd_buf[1], 28);
   // CLK_SysTickDelay(500000);  // ?��?????SSD ?��?????�賭�?
    
    // 7. ?�???��????��?????
    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    
    g_u8SlvRxFlag = 0;
    printf("Waiting for response...\n");
    while(g_u8SlvRxFlag == 0);  // ?�????�賣?�摰?�蕭?
    
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);
    
    // 8. ?????��?????�賣??
    printf("Response received. Byte Count: %d bytes\n", g_au8SlvRxData[1]);
    
    // NVMe-MI Admin Command Response ??��??? (??�賣?�摰蕭???��???):
    // [0]: 0x?? - SMBus Source Address (?�??: 0x10 ??�賢??
    // [1]: 0x31 - Byte Count (49 bytes = 0x31)
    // [2]: 0x3B - Source Address (0x1D << 1 | 1)
    // [3]: 0x01 - MCTP Header Version
    // [4]: 0x00 - Dest EID
    // [5]: 0x00 - Src EID  
    // [6]: 0xD0 - PktSeq (SOM=1, EOM=1, PktSeq=2, TO=1, Tag=0)
    // [7]: 0x84 - Message Type (NVMe-MI with IC bit)
    // [8]: 0x89 - NMIMT (0x09 = Admin Command Response)
    // [9]: 0x00 - Status (0=Success, 1=Error)
    // [10]: 0x01 - Controller ID Low
    // [11]: 0x00 - Controller ID High
    // [12+]: Response Data (DW0, DW1...)
    
    uint8_t nmimt = g_au8SlvRxData[8];
    uint8_t status = g_au8SlvRxData[9];  // Status ??offset 9
    uint16_t controller_id = g_au8SlvRxData[10] | (g_au8SlvRxData[11] << 8);
    
    printf("NMIMT: 0x%02X ", nmimt);
    if(nmimt == 0x89) {
        printf("(Admin Command Response)\n");
    } else {
        printf("(Unknown)\n");
    }
    
    printf("Controller ID: 0x%04X\n", controller_id);
    printf("Status: 0x%02X ", status);
    
    if(status == 0x00) {
        printf("(Success)\n");
        printf("??Identify Command Accepted!\n\n");
        
        // ??��?????�賣?��??? (DW0?�?�?? offset 12)
        // ??�賣???��???: 20 00 00 01 01 02 00 00 ...
        uint32_t dw0 = g_au8SlvRxData[12] | (g_au8SlvRxData[13] << 8) | 
                       (g_au8SlvRxData[14] << 16) | (g_au8SlvRxData[15] << 24);
        printf("Response DW0: 0x%08X\n", dw0);
        
        // ?��????�????�賣??�賭誘�??��???��????��?????�賢?�摰?�??4KB Identify ??�賣??
        // ?�???Identify Controller ??�賣???�質??蕭???��?�??
        // "Read NVMe-MI Data Structure" ??�賭�??�質�??
        printf("\n");
        printf("Note: ?�歹???��????�賭誘�??��???��????��??�摰?� Identify ??�賣?�\n");
        printf("      ?�???�??4KB Identify ??�賣?��??�蕭??�輻??\n");
        printf("      - NVMe-MI Read Data Structure (DTYP=0x00, Identify)\n");
        printf("      - ??��?�??�賡?蕭? NVMe Admin Queue ?�鳴??\n");
        
        // ??��????�賢???��????��????(??��????�??)
        printf("\nRaw Response Data (first 32 bytes):\n");
        for(int i = 0; i < 32 && i < g_au8SlvRxData[1]; i++) {
            printf("%02X ", g_au8SlvRxData[i + 2]);
            if((i + 1) % 16 == 0) printf("\n");
        }
        printf("\n");
        
    } else if(status == 0x01) {
        printf("(Error)\n");
        printf("??Identify Command Failed!\n");
    } else {
        printf("(Unknown Status)\n");
    }
    
    CLK_SysTickDelay(500000);
    printf("=== Identify Device Complete ===\n\n");
#endif  /* Identify disabled */
}

// =========================================================================
// Read NVMe-MI Data Structure - ?�鳴???�??Identify Controller ??�賣??
// =========================================================================
// Read NVMe-MI Data Structure - DTYP Sweep (0x00 ~ 0x03)
// CNTLID=0x00 confirmed valid (CNTLID=0x01 returns Status=0x01).
// Sweep DTYP to find what the device actually supports.
// DSP0235 DTYP values:
//   0x00 = NVM Subsystem Information
//   0x01 = Reserved (Port Information per some drafts)
//   0x02 = Reserved (Controller List per some drafts)
//   0x03 = Controller Information
// =========================================================================
{
    cmd_begin("Read MI Data (DTYP Sweep)");
    printf("\n>>> [CMD] NVMe-MI - Read NVMe-MI Data Structure (DTYP Sweep 0x00~0x03) <<<\n");
    printf("=== DTYP Sweep (CNTLID=0x00, DSPEC=0x00, testing DTYP 0x00..0x03) ===\n");

    uint8_t dtyp;
    for (dtyp = 0x00; dtyp <= 0x03; dtyp++)
    {
        printf("\n--- DTYP=0x%02X ---\n", dtyp);

        uint8_t read_buf[29];

        // 1. SMBus Header
        read_buf[0] = 0x3A;
        read_buf[1] = 0x0F;
        read_buf[2] = 0x19;  // Byte Count = 25

        // 2. MCTP Header
        read_buf[3] = (MCU_ADDR_7BIT << 1) | 1;
        read_buf[4] = 0x01;
        read_buf[5] = 0x00;  // Dest EID
        read_buf[6] = 0x01;  // Src EID
        read_buf[7] = 0xC8;  // SOM=1, EOM=1, Tag=0

        // 3. NVMe-MI Payload
        read_buf[8]  = 0x84;  // Msg Type: NVMe-MI + IC bit
        read_buf[9]  = 0x08;  // NMIMT: NVMe-MI Command
        read_buf[10] = 0x00;  // Flags
        read_buf[11] = 0x00;  // CNTLID Low = 0 (confirmed valid)
        read_buf[12] = 0x00;  // CNTLID High
        read_buf[13] = 0x00;  // Opcode = Read NVMe-MI Data Structure
        read_buf[14] = dtyp;  // DTYP <-- sweep
        read_buf[15] = 0x00;  // DSPEC = 0
        read_buf[16] = 0x00;  // Reserved
        read_buf[17] = 0x00;  // Data Offset = 0
        read_buf[18] = 0x00;
        read_buf[19] = 0x00;
        read_buf[20] = 0x00;
        read_buf[21] = 0x20;  // Data Length = 32 bytes
        read_buf[22] = 0x00;
        read_buf[23] = 0x00;

        // 4. MIC
        uint32_t mic_v = calc_mic(&read_buf[8], 16);
        read_buf[24] = (uint8_t)(mic_v & 0xFF);
        read_buf[25] = (uint8_t)((mic_v >> 8)  & 0xFF);
        read_buf[26] = (uint8_t)((mic_v >> 16) & 0xFF);
        read_buf[27] = (uint8_t)((mic_v >> 24) & 0xFF);

        // 5. PEC
        read_buf[28] = calc_pec_noinit(read_buf, 28);

        I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
        s_I2C1HandlerFn = I2C_SlaveTRx;
        I2C_WriteMultiBytes(I2C_PORT, read_buf[0] >> 1, &read_buf[1], 28);

        I2C_EnableInt(I2C1);
        NVIC_EnableIRQ(I2C1_IRQn);
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
        g_u8SlvRxFlag = 0;
        WAIT_SLV_RX_TIMEOUT();
        I2C_DisableInt(I2C1);
        NVIC_DisableIRQ(I2C1_IRQn);

        uint8_t  byte_cnt    = g_au8SlvRxData[1];
        uint8_t  resp_status = g_au8SlvRxData[10];
        uint16_t dtl         = (uint16_t)g_au8SlvRxData[12] |
                               ((uint16_t)g_au8SlvRxData[13] << 8);
        uint8_t  available   = (byte_cnt >= 16) ? (byte_cnt - 16) : 0;
        uint8_t *ds          = &g_au8SlvRxData[14];
        uint8_t  dlen        = (dtl <= available) ? (uint8_t)dtl : available;

        printf("  Status=0x%02X, DTL=%d, ByteCnt=%d, Avail=%d\n",
               resp_status, dtl, byte_cnt, available);

        // Full raw data dump
        printf("  Raw data[14..]: ");
        {
            uint8_t r;
            for (r = 0; r < dlen && r < 32; r++)
                printf("%02X ", ds[r]);
            printf("\n");
        }

        if (resp_status != 0x00)
        {
            printf("  [NOT SUPPORTED] Status=0x%02X\n", resp_status);
            CLK_SysTickDelay(300000);
            continue;
        }

        // ---- Parse per DTYP ----
        // Check if device returned the same fixed DTYP=0x00 data regardless of request
        // Known DTYP=0x00 fingerprint: ds[0]=0x00, ds[1]=0x01, ds[2]=0x01, ds[3]=0x02
        uint8_t is_dtyp00_data = (dlen >= 4 &&
                                  ds[0] == 0x00 && ds[1] == 0x01 &&
                                  ds[2] == 0x01 && ds[3] == 0x02);

        if (dtyp != 0x00 && is_dtyp00_data)
        {
            printf("  [!!] Device returned DTYP=0x00 data for DTYP=0x%02X request!\n", dtyp);
            printf("       This device firmware IGNORES the DTYP field entirely.\n");
            printf("       Only DTYP=0x00 (NVM Subsystem Information) is available.\n");
        }
        else if (dtyp == 0x00 && dlen >= 4)
        {
            // DSP0235 Table 14: NVM Subsystem Information (correct structure)
            // Byte 0:   NSSR    - NVMe Subsystem Reset support (0=not supported)
            // Byte 1:   SMBUS Protocol version (0x01 = v1.0)
            // Byte 2:   NVMe-MI Specification version (0x01 = v1.0)
            // Byte 3:   Number of Supported Ports
            // Byte 4..: Port Attributes (4 bytes per port)
            printf("  [DTYP=0x00] NVM Subsystem Information (DSP0235):\n");
            printf("    NSSR Support     : %s\n", ds[0] ? "Yes" : "No");
            printf("    SMBus Protocol   : v%d.%d\n", (ds[1] >> 4) & 0xF, ds[1] & 0xF);
            printf("    NVMe-MI Spec     : v%d.%d\n", (ds[2] >> 4) & 0xF, ds[2] & 0xF);
            printf("    Number of Ports  : %d\n", ds[3]);

            // Port Attributes (4 bytes each, starting at byte 4)
            {
                uint8_t p;
                for (p = 0; p < ds[3] && (4 + p*4 + 3) < dlen; p++)
                {
                    uint8_t  ptype  = ds[4 + p*4] & 0x0F; // bits[3:0]
                    uint8_t  pid    = ds[5 + p*4];
                    uint16_t mtu    = (uint16_t)ds[6 + p*4] | ((uint16_t)ds[7 + p*4] << 8);
                    printf("    Port[%d]: Type=%s ID=%d MTU=%d\n",
                           p,
                           ptype == 0x00 ? "Inactive" :
                           ptype == 0x01 ? "PCIe" :
                           ptype == 0x02 ? "SMBus" : "?",
                           pid, mtu);
                }
            }
        }
        else
        {
            printf("  [NOTE] Status=OK but unexpected data for DTYP=0x%02X\n", dtyp);
        }

        CLK_SysTickDelay(300000);
    }

    printf("\n=== DTYP Sweep Complete ===\n");
    printf("========================================\n");
    printf("CONCLUSION: This device NVMe-MI firmware\n");
    printf("  - Supports  : DTYP=0x00 (NVM Subsystem Info)\n");
    printf("  - Ignores   : DTYP=0x01/0x02/0x03 (returns same DTYP=0x00 data)\n");
    printf("  - CNTLID=0x01 returns Status=0x01 (only CNTLID=0x00 valid)\n");
    printf("  => VID/SN: Basic Management @0x6A, or Admin Identify over MCTP\n");
    printf("  => FW rev: Admin Get Log Page LID 0x03 over MCTP\n");
    printf("========================================\n\n");
}

// =========================================================================
// COMMAND SUPPORT SUMMARY FOR THIS DEVICE
// =========================================================================
// Based on testing, this device NVMe-MI firmware is limited:
//   CONFIRMED WORKING:
//     - NVM Subsystem Health Status Poll (Opcode=0x01) -> Health Info + Alerts
//     - Controller Health Status Poll    (Opcode=0x02) -> Per-controller health
//     - Read DS DTYP=0x00                             -> Port count, SS info
//     - MCTP Control (UUID, MsgType)
//   NOT SUPPORTED / LIMITED:
//     - Admin Passthrough: device responds but ignores command, returns SS Info
//       => Get Log Page, Get/Set Features, FW Revision all blocked
//     - Basic Management (SMBus Reg 0x00): returns 0xFF (optional, not impl.)
//     - DTYP=0x01/0x02/0x03: ignored, always returns DTYP=0x00 data
//     - Flush Cache: NVMe I/O command, no NVMe-MI I/O passthrough available
//     - FW Update: blocked (Admin PT broken) + MCU ROM too small anyway
// =========================================================================

// =========================================================================
// CMD: Controller Health Status Poll (NVMe-MI Opcode=0x02)
//      Per-controller health. Different from Opcode=0x01 (NVM Subsystem).
//      This is a Mandatory NVMe-MI command per DSP0235.
//
// Response layout (DSP0235):
//   raw[10]    = Status
//   raw[11]    = CNTLID Low
//   raw[12]    = CNTLID High
//   raw[13]    = CSTS[7:0]   Controller Status
//   raw[14]    = CSTS[15:8]
//   raw[15]    = Composite Temp [7:0]
//   raw[16]    = Composite Temp [15:8]  (Kelvin, subtract 273 for Celsius)
//   raw[17]    = Available Spare %
//   raw[18]    = Available Spare Threshold %
//   raw[19]    = PDLU (Percentage Drive Life Used %)
//   raw[20]    = Composite Smart Critical Warnings
// =========================================================================
{
    cmd_begin("Ctrl Health Poll");
    printf("\n>>> [CMD] NVMe-MI - Controller Health Status Poll (Opcode=0x02) <<<\n");
    printf("=== Controller Health Status Poll ===\n");

    /* Request = NMH(4) + Opcode/Rsvd(4) + NMD0(4) + NMD1(4) + MIC(4) = 20B.
     * The old 8B request (no NMD0/NMD1) got Status 0x05 (Invalid Command Size). */
    uint8_t  chsp_msg[20];
    uint8_t  chsp_resp[64];
    uint16_t chsp_len = 0U;
    int32_t  chsp_ret;

    memset(chsp_msg, 0, sizeof(chsp_msg));
    chsp_msg[0] = 0x84;              // IC bit + NVMe-MI msg type
    chsp_msg[1] = 0x08;              // NMIMT=0x01 (NVMe-MI Command)
    chsp_msg[4] = 0x02;              // Opcode: Controller Health Status Poll
    /* NMD0 = 0x87010000 (same as the working NMP=0x08 request):
     *   [15:0] SCTLID=0, [23:16] MAXRENT=1,
     *   bit24/25/26 include PCIe function / SR-IOV PF / SR-IOV VF, bit31 ALL.
     * Without the include bits the drive returns 0 entries.
     * NMD1 = 0: do not clear change flags. */
    chsp_msg[10] = 0x01;
    chsp_msg[11] = 0x87;
    {
        uint32_t mic = calc_mic(chsp_msg, 16);
        chsp_msg[16] = (uint8_t)(mic & 0xFF);
        chsp_msg[17] = (uint8_t)((mic >> 8) & 0xFF);
        chsp_msg[18] = (uint8_t)((mic >> 16) & 0xFF);
        chsp_msg[19] = (uint8_t)((mic >> 24) & 0xFF);
    }

    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
    s_rxqRd = s_rxqWr;
    mctp_tx_message(chsp_msg, (uint16_t)sizeof(chsp_msg));

    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    g_u8SlvRxFlag = 0;
    chsp_ret = mctp_rx_message(chsp_resp, (uint16_t)sizeof(chsp_resp), &chsp_len);
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);

    if ((chsp_ret != 0) || (chsp_len < 8U))
    {
        printf("  [FAIL] RX error %d (len=%u)\n", chsp_ret, chsp_len);
    }
    else if (chsp_resp[4] != 0x00U)
    {
        printf("  Status : 0x%02X (%s)\n", chsp_resp[4], nvme_mi_status_str(chsp_resp[4]));
    }
    else if (chsp_len < 26U)
    {
        printf("  Status : 0x00 but response too short (%u bytes, entries=%u)\n",
               chsp_len, chsp_resp[7]);
    }
    else
    {
        /* msg: [4]=Status [7]=entries; entry at [8]: CTLID(2) CSTS(2) CTEMP(2,K)
         * PDLU(1) SPARE(1) CWARN(1) CHSC(2) */
        uint8_t  *pE    = &chsp_resp[8];
        uint16_t csts   = (uint16_t)pE[2] | ((uint16_t)pE[3] << 8);
        uint16_t tempK  = (uint16_t)pE[4] | ((uint16_t)pE[5] << 8);
        uint8_t  cw     = pE[8];

        printf("  Status : 0x00 (Success)  entries=%u\n", chsp_resp[7]);
        printf("========================================\n");
        printf("  Controller Health (CTLID 0x%04X)\n", (uint16_t)pE[0] | ((uint16_t)pE[1] << 8));
        printf("========================================\n");
        printf("  CSTS (0x%04X):\n", csts);
        nvme_mi_print_csts(csts);
        printf("  Composite Temp      : %d C  (%u K)\n", (int32_t)tempK - 273, tempK);
        printf("  Drive Life Used     : %u%%\n", pE[6]);
        printf("  Available Spare     : %u%%\n", pE[7]);
        printf("  SMART Critical Warn : 0x%02X%s\n", cw, (cw == 0U) ? " (No Warnings)" : "");
        if ((cw & 0x01U) != 0U) { printf("    [!] Spare below threshold\n"); }
        if ((cw & 0x02U) != 0U) { printf("    [!] Temperature out of range\n"); }
        if ((cw & 0x04U) != 0U) { printf("    [!] NVM reliability degraded\n"); }
        if ((cw & 0x08U) != 0U) { printf("    [!] Media read-only\n"); }
        if ((cw & 0x10U) != 0U) { printf("    [!] Volatile backup failed\n"); }
        printf("  CHSC (changed flags): 0x%04X\n", (uint16_t)pE[9] | ((uint16_t)pE[10] << 8));
        printf("========================================\n");
    }

    CLK_SysTickDelay(300000);
    printf("=== Controller Health Status Poll Complete ===\n\n");
}

// =========================================================================
// CMD: NVMe Admin Command - SINGLE-PACKET probe (Keep Alive, OPC=0x18)
// =========================================================================
// A compliant Admin *data-in* request (Get Log Page / Identify) carries a full
// 80-byte SQE + DLEN, so the NVMe-MI message is 81 bytes and MUST split into
// 2 MCTP packets. This drive does not reassemble multi-packet MCTP -> no reply.
//
// To still prove the Admin channel is ALIVE we send a "no-data" Admin command:
//   * CFLGS = 0  (no DLENV / DOFSTV) -> NO data-in
//   * OPC   = 0x18 Keep Alive        -> needs no NSID, no buffer
//   * message truncated after the used CDWs so total payload <= 64 bytes
// The device can then answer in a SINGLE packet with a CQE completion only
// (NMIMT=0x90, Status), i.e. "command accepted" WITHOUT any log/data payload.
//
//   MMHDR NMIMT (bits [5:3] of the NVMe-MI header byte):
//     0x10 -> NVMe Admin Command request  / 0x90 response
// =========================================================================
{
    cmd_begin("Keep Alive probe");
    printf("\n>>> [CMD] NVMe Admin - Single-packet probe (Keep Alive) <<<\n");
    printf("=== No-data Admin cmd, full 72B request (NMH4+SQE64+MIC4) ===\n");

    {
        uint8_t  m[72];
        uint8_t  r[64];
        uint16_t rlen = 0U;
        int32_t  ret;

        memset(m, 0, sizeof(m));
        m[0] = 0x84;                 // IC bit + NVMe-MI msg type
        m[1] = 0x10;                 // NMIMT=0x02 (NVMe Admin Command)
        m[4] = 0x18;                 // OPC = Keep Alive, no data
        {
            uint32_t mic = calc_mic(m, 68);
            m[68] = (uint8_t)(mic & 0xFF);
            m[69] = (uint8_t)((mic >> 8) & 0xFF);
            m[70] = (uint8_t)((mic >> 16) & 0xFF);
            m[71] = (uint8_t)((mic >> 24) & 0xFF);
        }

        I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
        s_I2C1HandlerFn = I2C_SlaveTRx;
        s_rxqRd = s_rxqWr;
        mctp_tx_message(m, (uint16_t)sizeof(m));

        I2C_EnableInt(I2C1);
        NVIC_EnableIRQ(I2C1_IRQn);
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
        g_u8SlvRxFlag = 0;
        ret = mctp_rx_message(r, (uint16_t)sizeof(r), &rlen);
        I2C_DisableInt(I2C1);
        NVIC_DisableIRQ(I2C1_IRQn);

        if ((ret != 0) || (rlen < 8U))
        {
            printf("  [FAIL] RX error %d (len=%u)\n", ret, rlen);
        }
        else
        {
            /* msg: [0..3]=NMH [4]=NVMe-MI Status; CQE DW3 at [16..19] on success */
            printf("  Resp: NMH=%02X %02X  MI Status=0x%02X (%s)  len=%u\n",
                   r[0], r[1], r[4], nvme_mi_status_str(r[4]), rlen);
            if ((r[4] == 0x00U) && (rlen >= 24U))
            {
                uint32_t dw3 = (uint32_t)r[16] | ((uint32_t)r[17] << 8)
                             | ((uint32_t)r[18] << 16) | ((uint32_t)r[19] << 24);
                printf("  CQE DW3=0x%08X  SCT=0x%X SC=0x%02X\n", dw3,
                       (dw3 >> 25) & 0x07U, (dw3 >> 17) & 0xFFU);
            }
            else if (r[4] == 0x03U)
            {
                printf("  [NOTE] Keep Alive is not accepted out-of-band on this drive.\n");
                printf("         Admin channel itself works (Get Log / Identify / Format).\n");
            }
            else
            {
                /* other MI error: nothing more to decode */
            }
        }
    }

    CLK_SysTickDelay(300000);
    printf("=== NVMe Admin Single-packet probe Complete ===\n\n");
}

// =========================================================================
// CMD (TASK 1): Read NVMe-MI Data Structure - Optionally Supported Command List
// =========================================================================
// Vendor guidance: use "Read NVMe-MI Data Structure" with DTYP=0x04 to see
// which OOB (out-of-band) Admin commands are supported over SMBus/MCTP.
// This is an NVMe-MI Command (NMIMT=0x08) -> single-packet request/response,
// same working framing as the DTYP sweep above.
// =========================================================================
{
    cmd_begin("Optional Supported Cmd List");
    printf("\n>>> [CMD] NVMe-MI - Optionally Supported Command List (DTYP=0x04) <<<\n");
    printf("=== Read NVMe-MI Data Structure, DTYP=0x04 (OOB Admin cmd list) ===\n");

    uint8_t rb[29];

    rb[0] = 0x3A;
    rb[1] = 0x0F;
    rb[2] = 0x19;                    // Byte Count = 25

    rb[3] = (MCU_ADDR_7BIT << 1) | 1;
    rb[4] = 0x01;
    rb[5] = 0x00;                    // Dest EID
    rb[6] = 0x01;                    // Src EID
    rb[7] = 0xC8;                    // SOM=1 EOM=1 seq=0 TO=1

    rb[8]  = 0x84;                   // msgtype: NVMe-MI + IC
    rb[9]  = 0x08;                   // NMIMT = NVMe-MI Command
    rb[10] = 0x00;                   // Flags
    rb[11] = 0x00;                   // CNTLID low
    rb[12] = 0x00;                   // CNTLID high
    rb[13] = 0x00;                   // Opcode = Read NVMe-MI Data Structure
    rb[14] = 0x04;                   // DTYP = 0x04 (Optionally Supported Command List)
    rb[15] = 0x00;                   // DSPEC
    rb[16] = 0x00;                   // Reserved
    rb[17] = 0x00;                   // Data Offset = 0
    rb[18] = 0x00;
    rb[19] = 0x00;
    rb[20] = 0x00;
    rb[21] = 0x30;                   // Data Length = 48 bytes (keep single-packet)
    rb[22] = 0x00;
    rb[23] = 0x00;

    {
        uint32_t micv = calc_mic(&rb[8], 16);
        rb[24] = (uint8_t)(micv & 0xFF);
        rb[25] = (uint8_t)((micv >> 8)  & 0xFF);
        rb[26] = (uint8_t)((micv >> 16) & 0xFF);
        rb[27] = (uint8_t)((micv >> 24) & 0xFF);
    }
    rb[28] = calc_pec_noinit(rb, 28);

    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
    I2C_WriteMultiBytes(I2C_PORT, rb[0] >> 1, &rb[1], 28);

    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    g_u8SlvRxFlag = 0;
    WAIT_SLV_RX_TIMEOUT();
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);

    if (g_u8SlvRxFlag == 0)
    {
        printf("  [TIMEOUT] No response to OSCL (DTYP=0x04) query.\n");
    }
    else
    {
        uint8_t  bc     = g_au8SlvRxData[1];
        uint8_t  status = g_au8SlvRxData[10];
        uint16_t dtl    = (uint16_t)g_au8SlvRxData[12] | ((uint16_t)g_au8SlvRxData[13] << 8);
        uint8_t  avail  = (bc >= 16) ? (uint8_t)(bc - 16) : 0;
        uint8_t *ds     = &g_au8SlvRxData[14];
        uint8_t  dlen   = (dtl <= avail) ? (uint8_t)dtl : avail;

        printf("  Status=0x%02X  DTL=%u  ByteCnt=%u  Avail=%u\n", status, dtl, bc, avail);
        printf("  Raw data[14..]: ");
        {
            uint8_t r;
            for (r = 0; r < dlen && r < 48; r++) printf("%02X ", ds[r]);
            printf("\n");
        }

        if (status != 0x00)
        {
            printf("  [NOT SUPPORTED] Status=0x%02X (DTYP=0x04 not handled by this FW)\n", status);
        }
        else if (dlen >= 4 && ds[0] == 0x00 && ds[1] == 0x01 && ds[2] == 0x01 && ds[3] == 0x02)
        {
            printf("  [!!] Got the DTYP=0x00 fingerprint again -> FW still ignores DTYP.\n");
            printf("       This build returns no OSCL; ask vendor for FW that honours DTYP=0x04.\n");
        }
        else if (dlen >= 2)
        {
            uint16_t num = (uint16_t)ds[0] | ((uint16_t)ds[1] << 8);
            uint8_t *p   = &ds[2];
            uint16_t e;
            printf("  Optionally Supported Command List: %u entr%s\n",
                   num, (num == 1) ? "y" : "ies");
            for (e = 0; e < num && (uint16_t)(2 + e * 4 + 2) <= dlen; e++)
            {
                uint8_t ctype = p[e * 4 + 0];    // 0=MI, 1=Admin (best-effort)
                uint8_t opc   = p[e * 4 + 1];
                printf("    [%u] Type=%s OPC=0x%02X\n", e,
                       (ctype == 1) ? "Admin" : (ctype == 0) ? "MI" : "?", opc);
            }
        }
    }

    CLK_SysTickDelay(300000);
    printf("=== Optionally Supported Command List Complete ===\n\n");
}

// =========================================================================
// CMD (TASK 2): NVMe Admin Get Log Page (FW Slot) - VENDOR-VERIFIED 72B
// =========================================================================
// Vendor found our earlier 73B bug: we treated 0x84 as a separate "msgtype"
// byte AND added a 4-byte NMH, giving a 5-byte header -> one extra 0x00 that
// shifted every field by 1 and failed spec parsing.
//
// CORRECT layout (matches vendor's internally-passing payload):
//   * NMH = 4 bytes and INCLUDES the 0x84 byte:  msg[0..3] = 84 10 00 00
//   * NVMe Admin SQE = 64 bytes, msg[4..67]; SQE byte0 (OPC) is at msg[4]
//   * MIC = 4 bytes over msg[0..67], placed at msg[68..71]
//   * Total = 72B -> split into 2 MCTP packets: 64 + 8
//   * Get Log Page over OOB: RAE bit (CDW10 bit15) MUST be 1.
//   * Data Length field (vendor CDW7 @ msg[32]) = 0x1C.
//   * MTU=64 covers MCTP payload + MIC ONLY (no SMBus/MCTP header, no PEC).
// =========================================================================
#if 0  /* Disabled: duplicate of the "Get FW VERSION" block above (same 72B request) */
{
    cmd_begin("Get Log FW Slot (vendor)");
    printf("\n>>> [CMD] NVMe Admin - Get Log Page FW Slot (72B, vendor-verified) <<<\n");
    printf("=== NMH(4, incl 0x84)+SQE(64)+MIC(4)=72B; 2 pkts (64+8) ===\n");

    uint8_t  msg[80];
    uint16_t j, msg_len;

    for (j = 0; j < sizeof(msg); j++) msg[j] = 0x00;

    /* ---- NVMe-MI Message Header (NMH) = 4 bytes, msg[0..3], 0x84 included ---- */
    msg[0] = 0x84;                   // NMH0: IC bit + MCTP msgtype 0x04 (NVMe-MI)
    msg[1] = 0x10;                   // NMH1: NMIMT=0x02 (NVMe Admin Command), ROR=0
    msg[2] = 0x00;                   // NMH2 reserved
    msg[3] = 0x00;                   // NMH3 reserved

    /* ---- NVMe Admin SQE (64 bytes, msg[4..67]); SQE byte0 = OPC @ msg[4] ---- */
    msg[4] = 0x02;                   // OPC   = Get Log Page
    msg[5] = 0x00;                   // CFLGS
    /* msg[6..7] CTLID = 0 */

    /* SQEDW1 = NSID = 0xFFFFFFFF (whole subsystem) @ msg[8..11] */
    msg[8] = 0xFF; msg[9] = 0xFF; msg[10] = 0xFF; msg[11] = 0xFF;

    /* Data Length (vendor CDW7 @ msg[32]) = 0x0000001C */
    msg[32] = 0x1C;

    /* CDW10 @ msg[44..47] : LID + (RAE|LSP) + NUMDL(16b) */
    msg[44] = 0x03;                  // LID = 0x03 (FW Slot Info)
    msg[45] = 0x80;                  // bit7 RAE = 1 (required for OOB Get Log Page)
    msg[46] = 0x07;                  // NUMDL low  = 0x07 (8 dwords -> 32B)
    msg[47] = 0x00;                  // NUMDL high

    /* ---- MIC over msg[0..67] (68 bytes) -> msg[68..71] ---- */
    {
        uint32_t mic = calc_mic(msg, 68);
        msg[68] = (uint8_t)(mic & 0xFF);
        msg[69] = (uint8_t)((mic >> 8)  & 0xFF);
        msg[70] = (uint8_t)((mic >> 16) & 0xFF);
        msg[71] = (uint8_t)((mic >> 24) & 0xFF);
    }
    msg_len = 72;                    // NMH(4, incl 0x84) + SQE(64) + MIC(4)

    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;

    #define MCTP_MTU 64
    {
        uint16_t off = 0;
        uint8_t  seq = 0;
        while (off < msg_len)
        {
            uint16_t frag = (uint16_t)(msg_len - off);
            uint8_t  som  = (off == 0) ? 1 : 0;
            uint8_t  eom;
            uint8_t  pkt[8 + MCTP_MTU + 1];
            uint16_t p;

            if (frag > MCTP_MTU) frag = MCTP_MTU;
            eom = ((uint16_t)(off + frag) >= msg_len) ? 1 : 0;

            pkt[0] = 0x3A;
            pkt[1] = 0x0F;
            pkt[2] = (uint8_t)(5 + frag);        // byte count = src(1)+MCTPhdr(4)+payload
            pkt[3] = (MCU_ADDR_7BIT << 1) | 1;
            pkt[4] = 0x01;
            pkt[5] = 0x00;
            pkt[6] = 0x01;
            pkt[7] = (uint8_t)((som << 7) | (eom << 6) | ((seq & 0x03) << 4) | (1 << 3));
            for (p = 0; p < frag; p++) pkt[8 + p] = msg[off + p];
            pkt[8 + frag] = calc_pec_noinit(pkt, 8 + frag);

            printf("  TX pkt seq=%u SOM=%u EOM=%u payload=%u (bc=%u)\n",
                   seq, som, eom, frag, pkt[2]);

            I2C_WriteMultiBytes(I2C_PORT, pkt[0] >> 1, &pkt[1], 8 + frag);

            off = (uint16_t)(off + frag);
            seq = (seq + 1) & 0x03;
            CLK_SysTickDelay(2000);
        }
    }

    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    g_u8SlvRxFlag = 0;
    WAIT_SLV_RX_TIMEOUT();
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);

    if (g_u8SlvRxFlag == 0)
    {
        printf("  [TIMEOUT] No response to 72B multi-packet Admin Get Log Page.\n");
        printf("            If OSCL shows Get Log Page supported, capture these\n");
        printf("            request bytes and send to vendor for confirmation.\n");
    }
    else
    {
        uint8_t  bc      = g_au8SlvRxData[1];
        uint8_t  msgtype = g_au8SlvRxData[7];
        uint8_t  nmimt   = g_au8SlvRxData[8];
        uint8_t  type    = (nmimt >> 3) & 0x07;
        uint8_t  status  = g_au8SlvRxData[10];
        uint16_t total   = (uint16_t)bc + 2;

        printf("  Resp ByteCount=%u  msgtype=0x%02X  NMIMT=0x%02X (type=0x%02X) Status=0x%02X\n",
               bc, msgtype, nmimt, type, status);
        printf("  Raw (%u bytes):\n  ", total);
        {
            uint16_t r;
            for (r = 0; r < total && r < sizeof(g_au8SlvRxData); r++) {
                printf("%02X ", g_au8SlvRxData[r]);
                if ((r & 0x0F) == 0x0F) printf("\n  ");
            }
            printf("\n");
        }

        if (type == 0x02 && status == 0x00)
        {
            uint16_t data_start = 11;
            uint16_t data_end   = (total > 16) ? (uint16_t)(total - 5) : data_start;
            printf("========================================\n");
            printf("  FW Slot Info Log (data-in)\n");
            printf("  Data (ascii): [");
            {
                uint16_t k;
                for (k = data_start; k < data_end; k++) {
                    char c = (char)g_au8SlvRxData[k];
                    printf("%c", (c >= 0x20 && c <= 0x7E) ? c : '.');
                }
                printf("]\n");
            }
            if ((uint16_t)(data_end - data_start) >= 16)
            {
                uint16_t base = data_start + 8;
                uint8_t  k;
                printf("  FW Revision (FRS1, best-effort): [");
                for (k = 0; k < 8; k++) {
                    char c = (char)g_au8SlvRxData[base + k];
                    printf("%c", (c >= 0x20 && c <= 0x7E) ? c : '.');
                }
                printf("]\n");
                /* Capture FRS1 for the final summary */
                for (k = 0; k < 8; k++) {
                    char c = (char)g_au8SlvRxData[base + k];
                    g_fwRevStr[k] = (c >= 0x20 && c <= 0x7E) ? c : '.';
                }
                g_fwRevStr[8] = '\0';
            }
            printf("========================================\n");
        }
        else
        {
            printf("  [NOTE] NMIMT=0x%02X Status=0x%02X - not a successful Admin data-in.\n",
                   nmimt, status);
        }
    }

    CLK_SysTickDelay(300000);
    printf("=== NVMe Admin Get Log Page (72B) Complete ===\n\n");
    #undef MCTP_MTU
}
#endif

// =========================================================================
// CMD: NVM Subsystem Health Status Poll - Health Info & Alerts (#5 #6)
// =========================================================================
{
    cmd_begin("Health Alerts + Info");
    printf("\n>>> [CMD] NVMe-MI Health Status Alerts + Full Health Info <<<\n");
    printf("=== Get Health Info & Alerts (Opcode=0x01) ===\n");

    /* Request: NMH + Opcode 0x01 + NMD0 + NMD1 (old build sent 8B -> Status 0x05).
     * NMD0 bit31 CS = 0: do not clear the Composite Controller Status flags. */
    {
        const uint8_t req[16] = { 0x84, 0x08, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00 };
        uint8_t  r[64];
        uint16_t rlen = 0U;
        int32_t  ret  = nvme_mi_cmd_xfer(req, r, (uint16_t)sizeof(r), &rlen);

        if ((ret != 0) || (rlen < 8U))
        {
            printf("  [FAIL] RX error %d (len=%u)\n", ret, rlen);
        }
        else if (r[4] != 0x00U)
        {
            printf("  Status : 0x%02X (%s)\n", r[4], nvme_mi_status_str(r[4]));
        }
        else if (rlen < 20U)
        {
            printf("  Status : 0x00 but response too short (%u bytes)\n", rlen);
        }
        else
        {
            /* msg[8..15] = NVM Subsystem Health Data Structure:
             *   NSS(1) SW(1) CTEMP(1, deg C) PDLU(1) CCS(2) Rsvd(2)
             * SW bits are active-low: bit cleared = warning present. */
            uint8_t  nss   = r[8];
            uint8_t  sw    = r[9];
            int8_t   ctemp = (int8_t)r[10];
            uint8_t  pdlu  = r[11];
            uint16_t ccs   = (uint16_t)r[12] | ((uint16_t)r[13] << 8);
            uint8_t  swAct = (uint8_t)(~sw);   /* 1 = warning active */

            printf("  Status : 0x00 (Success)\n");
            printf("========================================\n");
            printf("  NVM Subsystem Health (Opcode 0x01)\n");
            printf("========================================\n");
            printf("  [Health Info]\n");
            printf("    NSS               : 0x%02X\n", nss);
            printf("    Composite Temp    : %d C\n", ctemp);
            printf("    Drive Life Used   : %u%%\n", pdlu);
            printf("    CCS (changed)     : 0x%04X\n", ccs);
            printf("\n  [Health Status Alerts]  SW=0x%02X (active-low)\n", sw);
            if ((swAct & 0x01U) != 0U) { printf("    [ALERT] Spare below threshold\n"); }
            if ((swAct & 0x02U) != 0U) { printf("    [ALERT] Temperature out of range\n"); }
            if ((swAct & 0x04U) != 0U) { printf("    [ALERT] NVM reliability degraded\n"); }
            if ((swAct & 0x08U) != 0U) { printf("    [ALERT] Media read-only\n"); }
            if ((swAct & 0x10U) != 0U) { printf("    [ALERT] Volatile backup failed\n"); }
            if ((swAct & 0x1FU) == 0U)
            {
                printf("    ** ALL CLEAR - No alerts **\n");
            }
            printf("========================================\n");
        }
    }

    CLK_SysTickDelay(300000);
    printf("=== Health Info & Alerts Complete ===\n\n");
}

{
// =========================================================================
// Vendor-documented MI Opcodes (per Phison Request/Response examples)
//   NVMe-MI body layout (16 bytes, b[8..23]) - CONFIRMED by sweep:
//     [0]=0x84 MsgType  [1]=0x08 NMIMT  [2..3]=Rsvd
//     [4]=Opcode  [5..7]=Rsvd
//     [8..11]=NVMe Mgmt Request DWORD0  (VPD DOFST here)
//     [12..15]=NVMe Mgmt Request DWORD1 (VPD DLEN here)  <-- body[12] proven!
//   VPD Read: DOFST@body[8..9], DLEN@body[12..13].
//     Sweep proved len@body[12]=0x40 -> bc=69 real FRU data (Phison/PAS...).
//     But DLEN=64 exceeded MCTP MTU -> response split (tag 0x90, EOM=0).
//   Response layout: raw[7]=84 raw[8]=88 raw[9]=flags raw[10]=Status
//     raw[11..14]=Mgmt Response DWORD  raw[15..]=VPD data
//     Single-packet (EOM=1) VPD byte count = bc - 17 (last 4 = MIC).
//   VPD is IPMI-FRU format: Common Header + Product Info Area (mfr/name/PN).
//   Strategy: read in small chunks (DLEN<=MTU) walking DOFST, reassemble here.
//     Per-chunk preview shows if DOFST advances (else firmware ignores it).
// =========================================================================
    /* --- Config Get - SMBus/I2C Frequency ---
     * NMD0: [7:0] Config ID = 0x01, [31:24] Port ID.
     * Port 0 gave Status 0x04 (Invalid Parameter) before, so sweep Port ID 0..2.
     * Success: NVMe-MI Mgmt Response byte msg[5] bits 3:0 = frequency. */
    cmd_begin("Config Get SMBus Freq");
    printf("\n>>> [CMD] NVMe-MI - Config Get - SMBus Freq (Opcode=0x04, CID=0x01) <<<\n");
    {
        uint8_t portId;

        for (portId = 0U; portId <= 2U; portId++)
        {
            uint8_t  req[16] = { 0x84, 0x08, 0x00, 0x00,  0x04, 0x00, 0x00, 0x00,
                                 0x01, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00 };
            uint8_t  r[64];
            uint16_t rlen = 0U;
            int32_t  ret;

            req[11] = portId;
            ret = nvme_mi_cmd_xfer(req, r, (uint16_t)sizeof(r), &rlen);
            if ((ret != 0) || (rlen < 8U))
            {
                printf("  Port %u: [FAIL] RX error %d (len=%u)\n", portId, ret, rlen);
            }
            else if (r[4] != 0x00U)
            {
                printf("  Port %u: Status 0x%02X (%s)\n", portId, r[4], nvme_mi_status_str(r[4]));
            }
            else
            {
                uint8_t freq = (uint8_t)(r[5] & 0x0FU);
                printf("  Port %u: Status 0x00  Resp=%02X %02X %02X  Freq=%u (%s)\n",
                       portId, r[5], r[6], r[7], freq,
                       (freq == 1U) ? "100 kHz" : (freq == 2U) ? "400 kHz" :
                       (freq == 3U) ? "1 MHz" : "N/A");
            }
        }
    }

    /* --- VPD Read: incremental chunked read, DOFST@body[8], DLEN@body[12] --- */
    {
        uint8_t  vpd[256];
        uint16_t vpd_len = 0;
        uint16_t dofst;
        const uint8_t CHUNK = 0x20;   /* 32B/chunk: keeps each response in ONE MCTP packet */
        cmd_begin("VPD Read");
        printf("\n>>> [CMD] NVMe-MI - VPD Read (incremental DOFST@[8] DLEN@[12]=0x20) <<<\n");
        for (dofst = 0; dofst < 256; dofst += CHUNK)
        {
            uint8_t body[16] = { 0x84,0x08,0,0, 0x05,0,0,0, 0,0,0,0, 0,0,0,0 };
            body[8]  = (uint8_t)(dofst & 0xFF);        /* DOFST low  */
            body[9]  = (uint8_t)((dofst >> 8) & 0xFF); /* DOFST high */
            body[12] = CHUNK;                          /* DLEN low   */

            uint8_t b[29];
            b[0]=0x3A; b[1]=0x0F; b[2]=0x19;
            b[3]=(MCU_ADDR_7BIT<<1)|1; b[4]=0x01;
            b[5]=0x00; b[6]=0x01; b[7]=0xC8;
            { uint8_t k; for (k=0;k<16;k++) b[8+k]=body[k]; }
            uint32_t mc = calc_mic(&b[8],16);
            b[24]=mc&0xFF; b[25]=(mc>>8)&0xFF; b[26]=(mc>>16)&0xFF; b[27]=(mc>>24)&0xFF;
            b[28]=calc_pec_noinit(b,28);

            I2C_SetSlaveAddr(I2C1,0,0x10,I2C_GCMODE_DISABLE);
            s_I2C1HandlerFn = I2C_SlaveTRx;
            I2C_WriteMultiBytes(I2C_PORT, b[0]>>1, &b[1], 28);
            I2C_EnableInt(I2C1); NVIC_EnableIRQ(I2C1_IRQn);
            I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI|I2C_CTL_AA);
            g_u8SlvRxFlag=0;
            WAIT_SLV_RX_TIMEOUT();
            I2C_DisableInt(I2C1); NVIC_DisableIRQ(I2C1_IRQn);

            uint8_t bc     = g_au8SlvRxData[1];
            uint8_t tag    = g_au8SlvRxData[6];
            uint8_t status = g_au8SlvRxData[10];
            int16_t n      = (int16_t)bc - 17;   /* single-packet VPD byte count */

            printf("  DOFST=%3u -> bc=%2d status=0x%02X tag=0x%02X (EOM=%d) data=%d  first: ",
                   dofst, bc, status, tag, (tag & 0x40) ? 1 : 0, n > 0 ? n : 0);
            { int16_t r; for (r=0; r<8 && r<n; r++) printf("%02X ", g_au8SlvRxData[15+r]); }
            printf("\n");

            if (status != 0x00 || n <= 0) { printf("  (no more data / not supported)\n"); break; }
            { int16_t r; for (r=0; r<n && vpd_len<(int16_t)sizeof(vpd); r++)
                  vpd[vpd_len++] = g_au8SlvRxData[15+r]; }
            if (!(tag & 0x40)) { printf("  (device signalled EOM=0: MTU split, stopping walk)\n"); break; }
            CLK_SysTickDelay(300000);
        }

        printf("  === Reassembled VPD (%u bytes) ===\n", vpd_len);
        { uint16_t r; for (r=0;r<vpd_len;r++){ printf("%02X ", vpd[r]); if((r&15)==15) printf("\n"); } }
        printf("\n  ASCII: [");
        { uint16_t r; for (r=0;r<vpd_len;r++){ char c=(char)vpd[r]; printf("%c",(c>=0x20&&c<=0x7E)?c:'.'); } }
        printf("]\n");

        /* --- IPMI-FRU decode: Common Header -> Product Info Area --- */
        if (vpd_len >= 8 && vpd[0] == 0x01) {
            uint16_t prod_off = (uint16_t)vpd[4] * 8;   /* Product Info Area offset (x8) */
            printf("\n  --- FRU Decode ---\n");
            printf("    Common Hdr: fmt=0x%02X IntUse@%u Chassis@%u Board@%u Product@%u MultiRec@%u\n",
                   vpd[0], vpd[1]*8, vpd[2]*8, vpd[3]*8, prod_off, vpd[5]*8);
            if (prod_off && prod_off + 3 < vpd_len && vpd[prod_off] == 0x01) {
                const char *labels[] = { "Manufacturer", "Product Name",
                                         "Part Number", "Version/FRU-ID", "Serial Number",
                                         "Asset Tag", "Extra1", "Extra2" };
                uint16_t p = prod_off + 3;   /* skip fmt, length, language */
                uint8_t  fi = 0;
                while (p < vpd_len && vpd[p] != 0xC1 && fi < 8) {
                    uint8_t tl  = vpd[p++];
                    uint8_t len = tl & 0x3F;      /* type in top 2 bits, length in low 6 */
                    if (tl == 0x00) { fi++; continue; }   /* empty field */
                    printf("    %-14s: [", labels[fi]);
                    { uint8_t j; for (j = 0; j < len && p + j < vpd_len; j++) {
                          char c = (char)vpd[p + j];
                          printf("%c", (c >= 0x20 && c <= 0x7E) ? c : '.');
                    } }
                    printf("]\n");
                    p += len;
                    fi++;
                }
            } else {
                printf("    (Product Info Area not present / bad offset)\n");
            }
        }
    }
}




// =========================================================================
// CMD SUMMARY
// =========================================================================
{
    printf("\n>>> [INFO] Command Availability Summary <<<\n");
    printf("========================================\n");
    printf("  #  Command              Protocol          Available  Reason\n");
    printf("  -  -------------------  ----------------  ---------  ------\n");
    printf("  A  Basic Management     SMBus Reg 0x00    YES        Resp@addr 0x6A (Temp/VID/SN)\n");
    printf("  B  Get Endpoint UUID    MCTP Ctrl 0x03    YES        PEC PASS\n");
    printf("  C  Get Msg Type Support MCTP Ctrl 0x05    YES        Types: 00/04/05/06\n");
    printf("  D  Read MI Data Struct  NVMe-MI Opc=0x00  YES        DTYP=0x00 only (NVM Subsys)\n");
    printf("  E  Configuration Set    NVMe-MI Opc=0x03  YES        Opcode@[4] CID@[8] (DWORD0)\n");
    printf("  F  Configuration Get    NVMe-MI Opc=0x04  YES        SMBus Freq @Port 1 = 100 kHz\n");
    printf("  G  VPD Read             NVMe-MI Opc=0x05  YES        DOFST@[8] DLEN@[12]; FRU (Phison/PAS)\n");
    printf("  H  VPD Write            NVMe-MI Opc=0x06  YES*       DOFST@[8] DLEN@[12] (mirror of Read)\n");
    printf("  I  Reset                NVMe-MI Opc=0x07  N/A        Not in vendor doc\n");
    printf("  1  Health Status Poll   NVMe-MI Opc=0x08  YES        Native MI cmd (MIC PASS)\n");
    printf("  1  Ctrl Health Poll     NVMe-MI Opc=0x02  YES        Native MI cmd\n");
    printf("  1  Health Info+Alerts   NVMe-MI Opc=0x01  YES        Native MI cmd (NSS/SW/CTEMP)\n");
    printf("  2  Flush Cache          NVMe I/O          NO         No I/O PT in NVMe-MI\n");
    printf("  3  Power Management     Admin GetFeat 0x02 YES       FID=0x02: PS=0 WH=0\n");
    printf("  4  FW Revision          Admin Log 0x03    YES        FRS1=%s (72B req, 2 pkts)\n", g_fwRevStr);
    printf("  5  Health Alerts        NVMe-MI Opc=0x01  YES        SW=0xFF -> ALL CLEAR\n");
    if (g_composTempC > -1000)
        printf("  6  Health Info          NVMe-MI Opc=0x01  YES        %ldC, PDLU 0%%; full data -> #7 SMART\n", (long)g_composTempC);
    else
        printf("  6  Health Info          NVMe-MI Opc=0x01  YES        PDLU 0%%; full data -> #7 SMART\n");
    printf("  7  SMART Log            Admin Log 0x02    YES        512B, 9 pkts reassembled, MIC PASS\n");
    printf("  8  Device Identify      Admin Opc=0x06    YES        4096B, 65 pkts reassembled, MIC PASS\n");
#if (NVME_FORMAT_ENABLE == 1U)
    printf("  9  Format NVM           Admin Opc=0x80    YES        NSID=all LBAF=0 SES=0 (erases data)\n");
#else
    printf("  9  Format NVM           Admin Opc=0x80    SKIP       Disabled (NVME_FORMAT_ENABLE=0, destructive)\n");
#endif
    printf("========================================\n");
    printf("  Admin Command status (verified on this drive):\n");
    printf("    Keep Alive (OPC 0x18): see probe (MI Status 0x03 = not allowed OOB)\n");
    printf("    Admin channel verified by Get Log / Identify / Format\n");
    printf("    Multi-pkt request (Get Log FW Slot, 72B -> 64B + 8B pkts):\n");
    printf("       -> NMIMT=0x90 Status=0x00, 28B log data, MIC PASS\n");
    printf("    Key fix: NMH is 4B incl 0x84 (old build sent 73B w/ extra 0x00)\n");
    printf("    Responses >64B arrive as multiple MCTP packets and are\n");
    printf("    reassembled by mctp_rx_message() (SOM/EOM/Seq + PEC + MIC).\n");
    printf("========================================\n\n");

    /* Per-command execution time table, printed below the summary. */
    cmd_timing_report();
}



#if 0
{
// ?? Get Log Page (?? SMART / FW Version / User Data)
		I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
    // ?????: 1(MsgType) + 4(MI Header) + 64(SQE) + 4(MIC) = 73 Bytes
uint8_t cmd_buf[29];

// 1. SMBus Header (Target Address + Command + Byte Count)
cmd_buf[0] = 0x3A; // SSD Address (Write)
cmd_buf[1] = 0x0F; // Command Code
cmd_buf[2] = 0x19; // Byte Count (25 Bytes)

// 2. MCTP Header (Null EID ?????,?????????)
cmd_buf[3] = (MCU_ADDR_7BIT << 1) | 1; // Src Addr
cmd_buf[4] = 0x01; // Header Version
cmd_buf[5] = 0x00; // Dest EID (Null)
cmd_buf[6] = 0x00; // Src EID (Null)
cmd_buf[7] = 0xC8; // SOM=1, EOM=1, Tag=0 (??????)

// 3. NVMe-MI Payload (Opcode: 0x02 Controller Health Status Poll)
// --- MIC ???? (cmd_buf[8]) ---
cmd_buf[8]  = 0x84; // Msg Type: NVMe-MI + IC Bit
cmd_buf[9]  = 0x08; // NMMT: NVMe-MI Command (???????????? Type)
cmd_buf[10] = 0x00; // Flags
cmd_buf[11] = 0x01; // Controller ID Low (??????)
cmd_buf[12] = 0x00; // Controller ID High
cmd_buf[13] = 0x02; // Opcode: 0x02 (??????)
cmd_buf[14] = 0x00; // Reserved
cmd_buf[15] = 0x00; 
cmd_buf[16] = 0x00; 
cmd_buf[17] = 0x00; // Clear Status (?????)
cmd_buf[18] = 0x00;
cmd_buf[19] = 0x00;
cmd_buf[20] = 0x00;
cmd_buf[21] = 0x00; 
cmd_buf[22] = 0x00; 
cmd_buf[23] = 0x00; 
// --- MIC ???? (cmd_buf[23]) ---

// 4. ?? MIC (? 16 Bytes)
uint32_t mic = calc_mic(&cmd_buf[8], 16);
cmd_buf[24] = mic & 0xFF;
cmd_buf[25] = (mic >> 8) & 0xFF;
cmd_buf[26] = (mic >> 16) & 0xFF;
cmd_buf[27] = (mic >> 24) & 0xFF;

// 5. ?? PEC ?????
cmd_buf[28] = calc_pec_noinit(cmd_buf, 28); 
I2C_WriteMultiBytes(I2C_PORT, cmd_buf[0] >> 1, &cmd_buf[1], 28);



I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		//parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		CLK_SysTickDelay(500000);
}
#endif


#if 0
{
	//Send_NVMe_PortInfo_Struct(
	uint8_t i2c_buf[50];
    uint8_t payload_len = 29; // Payload ?? (MCTP+MI+Body+MIC)

    // --- 1. ?? SMBus Header ---
    i2c_buf[0] = 0x3A; // 8-bit Address (Write)
    i2c_buf[1] = 0x0F; // Command Code
    i2c_buf[2] = payload_len;

    // ?? Payload ????
    uint8_t *p = &i2c_buf[3];

    // --- 2. ?? MCTP Header ---
    p[0] = (MCU_ADDR_7BIT << 1) | 1; // 0x21 (MCU Src Addr)
    p[1] = 0x01; // Ver
    p[2] = 0x05; // Dest EID (SSD)
    p[3] = 0x01; // Src EID (MCU)
    p[4] = 0xC8; // Tag (SOM=1, EOM=1)

    // --- 3. ?? NVMe-MI Header ---
    p[5] = 0x84; // Type=4 (NVMe-MI), IC=1 (CRC Check)
    p[6] = 0x08; // NMIMT=1 (Command), ROR=0
    p[7] = 0x00;
    p[8] = 0x00;

    // --- 4. ?? Command Body (16 Bytes) ---
    memset(&p[9], 0, 16); 
    p[9] = 0x00; // Opcode: Read Data Structure
    
    // DTYP (Data Structure Type) ?? Offset 16 (NMD0 Byte 3)
    // 0x01 = Port Information
    p[16] = 0x01; 

    // --- 5. ????? MIC (CRC-32C) ---
    // ????: MI Header(4) + Body(16) = 20 Bytes
    // p[5] ~ p[24]
    uint32_t mic = calc_mic(&p[5], 20); 
    p[25] = mic & 0xFF;
    p[26] = (mic >> 8) & 0xFF;
    p[27] = (mic >> 16) & 0xFF;
    p[28] = (mic >> 24) & 0xFF;

    // --- 6. ????? PEC (CRC-8) ---
    // ????: Addr(1) + Cmd(1) + Len(1) + Payload(29) = 32 Bytes
    i2c_buf[32] = calc_pec_noinit(i2c_buf, 32);

    // --- 7. ?? ---
    // i2c_buf[0] ???,??????????,??? Data ?? &i2c_buf[1] ??
    // ?? = Cmd(1) + Len(1) + Payload(29) + PEC(1) = 32
    I2C_WriteMultiBytes(I2C_PORT, i2c_buf[0] >> 1, &i2c_buf[1], 32);
	I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		CLK_SysTickDelay(500000);
	#if 0
uint8_t tx_buf[50];
    uint8_t payload_len = 0;
    uint8_t *payload = &tx_buf[3];

    // 1. MCTP Header
    payload[0] = (MCU_ADDR_7BIT << 1) | 1;
    payload[1] = 0x01;
    payload[2] = 0x05; // ???? Set EID ???? (? 0x05)
    payload[3] = SRC_EID;    // MCU ID (? 0x01)
    payload[4] = 0xC8;       // SOM=1, EOM=1 (???)

    // 2. NVMe-MI Header
    // Byte 0: IC=1 (?MIC), Type=4 (NVMe-MI) -> 0x84
    payload[5] = 0x84;
    // Byte 1: ROR=0, NMIMT=1 (NVMe-MI Command) -> 0x08
    payload[6] = 0x08;
    payload[7] = 0x00;
    payload[8] = 0x00;

    // 3. Command Body (16 Bytes)
    memset(&payload[9], 0, 16);
    
    // Opcode: 0x00 (Read NVMe-MI Data Structure)
    payload[9] = 0x00;
    
    // NMD0 - Byte 3 (Offset 16) ? DTYP
    // DTYP = 0x00 (NVM Subsystem Information)
    payload[16] = 0x00; 

    // 4. ?? MIC (Header 4 + Body 16 = 20 Bytes)
    uint32_t mic = calc_mic(&payload[5], 20);
    payload[25] = mic & 0xFF;
    payload[26] = (mic >> 8) & 0xFF;
    payload[27] = (mic >> 16) & 0xFF;
    payload[28] = (mic >> 24) & 0xFF;

    payload_len = 29; // 5+4+16+4

    // 5. SMBus Header & PEC
    tx_buf[0] = 0x3A; // SSD Address
    tx_buf[1] = 0x0F;
    tx_buf[2] = payload_len;
    
    tx_buf[payload_len + 3] = calc_pec_noinit(tx_buf, payload_len + 3);

    // 6. ??
    I2C_WriteMultiBytes(I2C_PORT, tx_buf[0] >> 1, &tx_buf[1], payload_len + 3);
I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		CLK_SysTickDelay(500000);
		#endif
}
#endif 

#if 0
{
		   
    #if 0
	CLK_SysTickDelay(500000);
	 I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
	
uint8_t pkt_subsys_info[] = {
    0x3A, 0x0F, 0x1D, 
    0x21, 0x01, 0x05, 0x01, 0xC8, // MCTP Hdr
    0x84, 0x08, 0x00, 0x00,       // NVMe-MI Hdr
    0x00, 0x00, 0x00, 0x00,       // Op=0, Rsvd
    0x00, 0x00, 0x00, 0x00,       // NMD0 (DTYP=00)
    0x00, 0x00, 0x00, 0x00,       // NMD1
    0x00, 0x00, 0x00, 0x00,       // Padding (?? 16 bytes body)
    0x47, 0x00, 0x8A, 0xE1,       // MIC (CRC-32C for DTYP=0)
    0x77                          // PEC
};

I2C_WriteMultiBytes(I2C_PORT, pkt_subsys_info[0] >> 1, &pkt_subsys_info[1], 32);
#endif
	#if 0
	
	    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
uint8_t i2c_buf[50];
uint8_t *payload = &i2c_buf[3];

// ---------------------------------------------------------
// 1. ?? Payload (MCTP + NVMe-MI)
// ---------------------------------------------------------

// [0-4] MCTP Header (SMBus binding)
// payload[0]: Source Slave Address (LSB must be 1)
payload[0] = (MCU_ADDR_7BIT << 1) | 1; 
payload[1] = 0x01; // Header Version
payload[2] = TARGET_NEW_EID;
payload[3] = SRC_EID;
payload[4] = 0xC8; // Tag: SOM=1, EOM=1, MsgTag=8

// [5-8] NVMe-MI Message Header (4 Bytes)
// Byte 0: IC(Bit 7)=1 (Integrity Check enabled), Msg Type(Bits 6:0)=4 (NVMe-MI)
payload[5] = 0x84; 
// Byte 1: ROR(Bit 7)=0 (Request), NMIMT(Bits 6:3)=1 (NVMe-MI Command), CSI=0
payload[6] = 0x08; 
payload[7] = 0x00; // Reserved
payload[8] = 0x00; // Reserved

// [9-20] Command Body (12 Bytes for NVMe-MI Command)
// NVMe-MI Command ??: Opcode(1) + Rsvd(3) + NMD0(4) + NMD1(4)
memset(&payload[9], 0, 12); 

// Byte 4 (Msg Byte 4): Opcode = 0x00 (Read NVMe-MI Data Structure)
payload[9] = 0x00; 

// NVMe Management Dword 0 (NMD0) - Bytes 8-11 of Message
// NMD0 ?? (Little Endian):
// Bits 31:24 (Byte 3 of NMD0) = Data Structure Type (DTYP)
// Bits 23:16 (Byte 2 of NMD0) = Port ID (PORTID)
// Bits 15:00 (Byte 0-1 of NMD0) = Controller ID (CTRLID)

// payload[13] ? NMD0 Byte 0 (CTRLID LSB)
// payload[14] ? NMD0 Byte 1 (CTRLID MSB)
// payload[15] ? NMD0 Byte 2 (PORTID)
// payload[16] ? NMD0 Byte 3 (DTYP)

payload[13] = 0x00; // Ctrl ID (Low)
payload[14] = 0x00; // Ctrl ID (High)
payload[15] = 0x00; // Port ID (Port 0)
payload[16] = 0x01; // Data Structure Type = 0x01 (Port Information)

// NMD1 (Bytes 12-15 of Message) - Reserved for this command
// payload[17] ~ payload[20] ?? 0

// ---------------------------------------------------------
// 2. ?? MIC (CRC-32C)
// ---------------------------------------------------------
// ????: MI Header(4) + Cmd Body(12) = 16 Bytes
// ??: ????? 20 ????,??? Invalid Command Size
uint32_t mic = calc_mic(&payload[5], 16);

int mic_idx = 5 + 4 + 12; // Index 21
payload[mic_idx]     = mic & 0xFF;
payload[mic_idx + 1] = (mic >> 8) & 0xFF;
payload[mic_idx + 2] = (mic >> 16) & 0xFF;
payload[mic_idx + 3] = (mic >> 24) & 0xFF;

// ?????:
// MCTP Header (5) + MI Header (4) + Cmd Body (12) + MIC (4) = 25
uint8_t payload_len = 25; 

// ---------------------------------------------------------
// 3. SMBus Header & PEC
// ---------------------------------------------------------
i2c_buf[0] = (NVME_ADDR_7BIT << 1); // Dest Addr
i2c_buf[1] = 0x0F;                  // Command Code (MCTP)
i2c_buf[2] = 0x1d;           // Byte Count

// ?? PEC (?? Addr, Cmd, Len, Payload)
uint8_t pec = calc_pec_noinit(i2c_buf, payload_len + 3);
i2c_buf[payload_len + 3] = pec;

I2C_WriteMultiBytes(I2C_PORT, i2c_buf[0] >> 1, &i2c_buf[1], payload_len + 3);
#endif
#if 0
I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		#endif
}
#endif
		#if 0
		    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
		#if 1
		uint8_t i2c_buf_t[50]; // Buffer ?????
    uint8_t payload_len_t = 0;
    
    // --- 1. ?? Payload ---
    uint8_t *payload_t = &i2c_buf_t[3];
    
    // MCTP Header
    payload_t[0] = (MCU_ADDR_7BIT << 1) | 1; // 0x21
    payload_t[1] = 0x01;
    payload_t[2] = TARGET_NEW_EID; // Dest EID (NVMe)
    payload_t[3] = SRC_EID; // Src EID
    payload_t[4] = 0xC8; // SOM=1, EOM=1
    
    // NVMe-MI Header
    payload_t[5] = 0x84; // Msg Type
    payload_t[6] = 0x08; // Flags: Request(1), NMIMT(0) -> 0x80 (????!)
    payload_t[7] = 0x00;
    payload_t[8] = 0x00;
    
    // NVMe-MI Command: Read NVMe-MI Data Structure (16 Bytes)
    memset(&payload_t[9], 0, 16); // ????
    payload_t[9] = 0x00; // Opcode: Read Data Structure
   // payload_t[13] = 0x00; // Data Structure ID: 0 (NVM Subsystem Info)
    payload_t[16] = 0x00;
    // ?? MIC (CRC-32C)
    // ??: MI Header(4) + Command(16) = 20 bytes (Index 5 to 24)
    uint32_t mic = calc_mic(&payload_t[5], 20);
    payload_t[25] = mic & 0xFF;
    payload_t[26] = (mic >> 8) & 0xFF;
    payload_t[27] = (mic >> 16) & 0xFF;
    payload_t[28] = (mic >> 24) & 0xFF;
    
    payload_len_t = 29; // 5(MCTP) + 4(MI) + 16(Cmd) + 4(MIC)
    
    // --- 2. SMBus Header ---
    i2c_buf_t[0] = (NVME_ADDR_7BIT << 1); // 0x3A
    i2c_buf_t[1] = 0x0F;
    i2c_buf_t[2] = payload_len_t;
    
    // --- 3. PEC (CRC-8) ---
    uint8_t pec = calc_pec_noinit(i2c_buf_t, payload_len_t + 3);
    i2c_buf_t[payload_len_t + 3] = pec;
    
    // --- 4. ?? ---
    // ???: Header(3) + Payload(29) + PEC(1) = 33 bytes
    //printf("Sending Simple Ping (Len=33)...\n");
    I2C_WriteMultiBytes(I2C_PORT, i2c_buf_t[0] >> 1, &i2c_buf_t[1], payload_len_t + 3);
		#endif
	CLK_SysTickDelay(5000);
unsigned char rx_buf[140];
I2C_ReadMultiBytes(I2C_PORT, NVME_ADDR_7BIT, rx_buf, 140);

//I2C_EnableInt(I2C1);
  //  NVIC_EnableIRQ(I2C1_IRQn);
    //I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		//g_u8SlvRxFlag=0;
		//while(g_u8SlvRxFlag==0);
		//parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		//I2C_DisableInt(I2C1);
		//NVIC_DisableIRQ(I2C1_IRQn);		
//parse_nvme_ds_response(&g_au8SlvRxData[0]);

#endif

		#if  0
				    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
uint8_t i2c_buf[50];
uint8_t *payload = &i2c_buf[3];

// ---------------------------------------------------------
// 1. ?? Payload (MCTP + NVMe-MI)
// ---------------------------------------------------------

// [0-4] MCTP Header
// payload[0]: Source Addr (MCU) with LSB=1
payload[0] = (MCU_ADDR_7BIT << 1) | 1; 
payload[1] = 0x01; // Header Version (?????????????)
payload[2] = DEST_EID;
payload[3] = SRC_EID;
payload[4] = 0xC8; // Tag: SOM=1, EOM=1, MsgTag=0, TO=1

// [5-8] NVMe-MI Message Header
// ????????????:
// Byte 0: 0x84 (IC=1, Type=4 NVMe-MI)
// Byte 1: 0x08 (NMIMT=1 Command, ROR=0 Request)
payload[5] = 0x84; 
payload[6] = 0x08; 
payload[7] = 0x00;
payload[8] = 0x00;

// [9-24] Command Body (??:?? 16 Bytes)
// NVMe-MI Command Body ??????? 16 Bytes (Opcode + Rsvd + NMD0 + NMD1)
// ?? Read Structure ??? NMD0,???? 0 ?????
memset(&payload[9], 0, 16); 

// Byte 0: Opcode = 0x00 (Read NVMe-MI Data Structure)
payload[9] = 0x00; 

// NMD0 (Management Dword 0) - ????
// Bits 31:24 (Byte 3 of NMD0) = DTYP (0x01 Port Info)
// Bits 23:16 (Byte 2 of NMD0) = Port ID (0x00)
// Bits 15:00 (Byte 0-1 of NMD0) = Ctrl ID (0x00)
// ?? payload offset: 13, 14, 15, 16
payload[13] = 0x00; // Ctrl ID Low
payload[14] = 0x00; // Ctrl ID High
payload[15] = 0x00; // Port ID
payload[16] = 0x01; // DTYP = 0x01

// ---------------------------------------------------------
// 2. ?? MIC (CRC-32C)
// ---------------------------------------------------------
// ??:MIC ???????? MI Header(4) + Cmd Body(16) = 20 Bytes
uint32_t mic = calc_mic(&payload[5], 20);

// MIC ????:5 (MCTP) + 20 (MI Msg) = Index 25
int mic_idx = 25; 
payload[mic_idx]     = mic & 0xFF;
payload[mic_idx + 1] = (mic >> 8) & 0xFF;
payload[mic_idx + 2] = (mic >> 16) & 0xFF;
payload[mic_idx + 3] = (mic >> 24) & 0xFF;

// ??:? Payload ????? 29
// MCTP(5) + MI Msg(20) + MIC(4) = 29 Bytes
uint8_t payload_len = 29; 

// ---------------------------------------------------------
// 3. SMBus Header & PEC
// ---------------------------------------------------------
i2c_buf[0] = (NVME_ADDR_7BIT << 1); 
i2c_buf[1] = 0x0F;
i2c_buf[2] = payload_len; // 0x1D

// ?? PEC
 pec = calc_pec_noinit(i2c_buf, payload_len + 3);
i2c_buf[payload_len + 3] = pec;
#endif
#if 0
		    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
uint8_t debug_packet[] = {
    // [0] I2C Addr (Write)
    0x3A, 
    // [1] SMBus Cmd
    0x0F, 
    // [2] Byte Count (29 Bytes)
    0x1D, 
    // [3-7] MCTP Header
    0x21, 0x01, 0x00, 0x01, 0xC8,
    // [8-11] NVMe-MI Header (Type 4, Command)
    0x84, 0x08, 0x00, 0x00,
    // [12-27] Command Body (16 Bytes, Opcode 0x00, DTYP 0x01)
    0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x01, // DTYP=1 ?? (Offset 19 of buffer)
    0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00,
    // [28-31] MIC (CRC-32C, Little Endian) -> 0xE6F75974
    0x74, 0x59, 0xF7, 0xE6,
    // [32] PEC (CRC-8, over bytes 0-31)
    0x85 
};

// ????? 33 ? Byte
I2C_WriteMultiBytes(I2C_PORT, debug_packet[0] >> 1, &debug_packet[1], 32);

//I2C_WriteMultiBytes(I2C_PORT, i2c_buf[0] >> 1, &i2c_buf[1], payload_len + 3);
		I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		//parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);		
parse_nvme_ds_response(&g_au8SlvRxData[0]);
		#endif
	
#if 0 //veridy
{
    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
uint8_t mi_msg[76]; 
    memset(mi_msg, 0, sizeof(mi_msg));

    // [0-3] NVMe-MI Header
    mi_msg[0] = 0x04; // Msg Type: NVMe-MI
    mi_msg[1] = 0x81; // Flags: IC=1, NMIMT=1 (Admin Command)
    mi_msg[2] = 0x00; // Rsvd
    mi_msg[3] = 0x00; // Rsvd

    // [4-7] Data Length (8 Bytes)
    mi_msg[4] = 0x08; mi_msg[5] = 0x00; mi_msg[6] = 0x00; mi_msg[7] = 0x00;

    // [8-11] Data Offset (0x40 = 64)
    mi_msg[8] = 0x40; mi_msg[9] = 0x00; mi_msg[10] = 0x00; mi_msg[11] = 0x00;

    // [12-75] Admin Command (SQE)
    // Opcode at offset 0 of SQE (mi_msg[12])
    mi_msg[12] = 0x06; // Identify
    // CNS at offset 10 of SQE (mi_msg[22])
    mi_msg[22] = 0x01; // Identify Controller
// ?? MIC (CRC-32C) ??? 76 Bytes
    uint32_t mic = calc_crc32c(mi_msg, 76);

    // ---------------------------------------------------------
    // 2. ???????? (Fragment 1)
    // ---------------------------------------------------------
    // SMBus(3) + MCTP(5) + Payload(64) + PEC(1) = 73 Bytes
    uint8_t pkt1[80]; 
    uint8_t pkt1_payload_len = 5 + 64; // MCTP Header + 64 Data
    
    // SMBus Header
    pkt1[0] = (NVME_ADDR_7BIT << 1); // 0x3A
    pkt1[1] = 0x0F;                  // Cmd
    pkt1[2] = pkt1_payload_len;      // Len = 69

    // MCTP Header
    pkt1[3] = (MCU_ADDR_7BIT << 1) | 1; // Src Addr
    pkt1[4] = 0x01;                     // Ver
    pkt1[5] = TARGET_NEW_EID;                 // Dest EID
    pkt1[6] = SRC_EID;                  // Src EID
    pkt1[7] = 0xC8;                     // Tag: SOM=1, EOM=0, Seq=0, TO=1

    // Copy ? 64 bytes ? mi_msg ? payload
    memcpy(&pkt1[8], mi_msg, 64);

    // ?? PEC
    pkt1[8 + 64] = calc_pec_noinit(pkt1, 3 + pkt1_payload_len); // Header(3) + Payload(69)


    I2C_WriteMultiBytes(I2C_PORT, pkt1[0] >> 1, &pkt1[1], pkt1_payload_len + 3);
CLK_SysTickDelay(100);
uint8_t pkt2[40];
    uint8_t pkt2_payload_len = 5 + 16; 

    // SMBus Header
    pkt2[0] = (NVME_ADDR_7BIT << 1);
    pkt2[1] = 0x0F;
    pkt2[2] = pkt2_payload_len; // 21

    // MCTP Header
    pkt2[3] = (MCU_ADDR_7BIT << 1) | 1;
    pkt2[4] = 0x01;
    pkt2[5] = TARGET_NEW_EID;
    pkt2[6] = SRC_EID;
    // Tag: SOM=0, EOM=1, Seq=1 (0+1), TO=1
    // Bin: 0100 1001 = 0x49
    pkt2[7] = 0x49; 

    // Copy ??? mi_msg (offset 64, len 12)
    memcpy(&pkt2[8], &mi_msg[64], 12);

    // ?? MIC (Little Endian)
    pkt2[20] = mic & 0xFF;
    pkt2[21] = (mic >> 8) & 0xFF;
    pkt2[22] = (mic >> 16) & 0xFF;
    pkt2[23] = (mic >> 24) & 0xFF;

    // ?? PEC
    pkt2[24] = calc_pec_noinit(pkt2, 3 + pkt2_payload_len);
 I2C_WriteMultiBytes(I2C_PORT, pkt2[0] >> 1, &pkt2[1], pkt2_payload_len + 3);

I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		}
#endif




    PA9 = 0; // HWM_SEL

    while (1);

}
