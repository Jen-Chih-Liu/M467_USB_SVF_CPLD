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


volatile uint8_t g_u8SlvRxFlag = 0;
volatile uint8_t g_u8SlvDataLen;

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
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if (u32Status == 0x80)                 /* Previously address with own SLA address
                                                   Data has been received; ACK has been returned*/
    {
        u8data = (unsigned char) I2C_GET_DATA(I2C1);

        g_au8SlvRxData[g_u8SlvDataLen++] = u8data;


        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    }

    else if (u32Status == 0x88)                 /* Previously address with own SLA address
                                                   Data has been received; ACK has been returned*/
    {
        u8data = (unsigned char) I2C_GET_DATA(I2C1);

        g_au8SlvRxData[g_u8SlvDataLen++] = u8data;


        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    }

    else if (u32Status == 0xA0)                 /* A STOP or repeated START has been received while still
                                                   addressed as Slave/Receiver*/
    {
			g_u8SlvRxFlag = 1;
        g_u8SlvDataLen = 0;
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
                           resp[1 + t] == 0x04 ? "NVMe-MI"      : "Vendor/Unknown");
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
        uint8_t  nmimt        = nmimt_raw & 0x0F;   /* lower 4 bits */
        uint8_t  ror          = (nmimt_raw >> 7) & 0x01;
        uint8_t  mi_flags     = raw[9];
        uint8_t  status       = raw[10];
        uint16_t ctrl_id      = (uint16_t)raw[11] | ((uint16_t)raw[12] << 8);

        printf("  NVMe-MI Hdr  : 0x%02X\n", nmimt_raw);
        printf("    ROR (Response) : %d\n", ror);
        printf("    NMIMT          : 0x%02X => %s\n", nmimt,
               nmimt == 0x08 ? "NVMe-MI Command Response" :
               nmimt == 0x09 ? "NVMe Admin Command Response" : "Unknown");
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

            if (nmimt == 0x08) /* NVMe-MI Command Response */
            {
                /* Determine original command from context (NMIMT=0x08 covers multiple opcodes) */
                /* NVM Subsystem Health Status Poll Response */
                if (resp_len >= 8)
                {
                    printf("  --- NVMe-MI Response Body (NMIMT=0x08) ---\n");
                    /* Try to interpret as Health Status (Opcode 0x01 response) */
                    printf("  Composite Temp : %d C\n", (int8_t)resp[0]);
                    printf("  PDLU           : %d%%\n", resp[1]);
                    printf("  Spare          : %d%%\n", resp[2]);
                    printf("  CritWarn       : 0x%02X\n", resp[3]);
                    printf("  CSTS           : 0x%04X\n",
                           (uint16_t)resp[4] | ((uint16_t)resp[5] << 8));
                    printf("  NSSR           : 0x%04X\n",
                           (uint16_t)resp[6] | ((uint16_t)resp[7] << 8));
                }

                /* Print remaining raw bytes */
                printf("  Raw (hex): ");
                for (uint8_t i = 0; i < resp_len && i < 32; i++)
                    printf("%02X ", resp[i]);
                printf("\n");
            }
            else if (nmimt == 0x09) /* NVMe Admin Command Response */
            {
                printf("  --- NVMe Admin Cmd Response (NMIMT=0x09) ---\n");
                if (resp_len >= 4)
                {
                    uint32_t dw0 = (uint32_t)resp[0] | ((uint32_t)resp[1] << 8)  |
                                   ((uint32_t)resp[2] << 16) | ((uint32_t)resp[3] << 24);
                    printf("  DW0 (Status Field): 0x%08X\n", dw0);
                    uint16_t sf  = (uint16_t)(dw0 >> 17);
                    uint8_t  sc  = (uint8_t)((dw0 >> 17) & 0xFF);
                    uint8_t  sct = (uint8_t)((dw0 >> 25) & 0x07);
                    printf("    Status Code (SC) : 0x%02X (%s)\n", sc,
                           sc == 0x00 ? "Successful Completion" : "Error");
                    printf("    Status Code Type : 0x%02X (%s)\n", sct,
                           sct == 0x00 ? "Generic Command" :
                           sct == 0x01 ? "Command Specific" :
                           sct == 0x02 ? "Media/Data Integrity" : "Vendor");
                    (void)sf;
                }

                /* Print data payload */
                if (resp_len > 4)
                {
                    printf("  Admin Response Data (hex):\n  ");
                    for (uint8_t i = 4; i < resp_len && i < 60; i++)
                    {
                        printf("%02X ", resp[i]);
                        if ((i - 3) % 16 == 0) printf("\n  ");
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
    Set_USB_SerialNumber_From_UID();
    HSUSBD_Open(&gsHSInfo, HID_ClassRequest, NULL);
    HSUSBD_SetVendorRequest(HID_VendorRequest);

    /* Endpoint configuration */
    HID_Init();

    /* Enable HSUSBD interrupt */
    NVIC_EnableIRQ(USBD20_IRQn);
    HSUSBD_DISABLE_USB();

    HSUSBD_ENABLE_USB();
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
                if (bc >= 8 && (mt == 0x00 || mt == 0x84 || mt == 0x04)) {
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
    printf("\n>>> [CMD] NVMe-MI - Get Log Page / SMART Data (Opcode=0x08, NMIMT=0x08) <<<\n");
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
cmd_buf[9] = 0x08;        // NMMT: NVMe Admin Command
cmd_buf[10] = 0x00;        // Reserved
cmd_buf[11] = 0x00;       // Controller ID Low
cmd_buf[12] = 0x02;       // Controller ID High (ID: 0x0002)
cmd_buf[13] = 0x00;       // Admin Opcode (Get Log Page)
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
#if 0
//Get FW VERSION
		I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    
    s_I2C1HandlerFn = I2C_SlaveTRx;
// =========================================================================
// ?? A:??????????? 72 Bytes NVMe-MI ??
// =========================================================================
uint8_t mi_msg[72];
memset(mi_msg, 0, sizeof(mi_msg));

// 1. NVMe-MI Header (4 Bytes)
mi_msg[0] = 0x84; // Msg Type: NVMe-MI (0x04) + IC Bit (0x80)
mi_msg[1] = 0x10; // NMMT: 0x01 (NVMe Admin Command)
mi_msg[2] = 0x00; // Flags
mi_msg[3] = 0x01; // Reserved

// 2. NVMe SQE (64 Bytes,?? mi_msg[4] ~ mi_msg[67])
mi_msg[4]  = 0x06; // SQE Dword 0: Opcode = 0x06 (Identify)
// NVMe-MI ??:?? Offset ? Length ?????? 4096 Bytes
mi_msg[28] = 0x00; // SQE Dword 6: Data Offset = 64 (FW Version ?????)
mi_msg[32] = 0x00; // SQE Dword 7: Data Length = 8  (??? 8 Bytes)
mi_msg[44] = 0x01; // SQE Dword 10: CNS = 0x01 (Identify Controller)

// 3. ?? MIC (??? 68 Bytes ????)
uint32_t mic = calc_mic(mi_msg, 68);
mi_msg[68] = mic & 0xFF;
mi_msg[69] = (mic >> 8) & 0xFF;
mi_msg[70] = (mic >> 16) & 0xFF;
mi_msg[71] = (mic >> 24) & 0xFF;

// =========================================================================
// ?? B:???????? MCTP ?? (SOM=1, EOM=0)
// =========================================================================
uint8_t pkt1[64];
pkt1[0] = 0x3A; // SSD Address (Write)
pkt1[1] = 0x0F; // Command Code
pkt1[2] = 60;   // SMBus Byte Count (1 Byte SrcAddr + 4 Bytes MCTP + 55 Bytes Payload)

// MCTP Header
pkt1[3] = (MCU_ADDR_7BIT << 1) | 1; // Src Addr
pkt1[4] = 0x01; // Header Version
pkt1[5] = 0x00; // Dest EID (Null EID)
pkt1[6] = 0x01; // Src EID (????????)
pkt1[7] = 0x88; // SOM=1, EOM=0, PktSeq=0, TO=1, MsgTag=0 (??? 1000 1000)

// ??? 55 Bytes ? NVMe-MI Payload
memcpy(&pkt1[8], &mi_msg[0], 55);

// ?? PEC ??? I2C
pkt1[63] = calc_pec_noinit(pkt1, 63);
I2C_WriteMultiBytes(I2C_PORT, pkt1[0] >> 1, &pkt1[1], 63);
		CLK_SysTickDelay(500000);
// --- ?????? 1~2ms ? Delay,? SSD ??????? ---

// =========================================================================
// ?? C:???????? MCTP ?? (SOM=0, EOM=1)
// =========================================================================
uint8_t pkt2[26];
pkt2[0] = 0x3A; // SSD Address (Write)
pkt2[1] = 0x0F; // Command Code
pkt2[2] = 22;   // SMBus Byte Count (1 Byte SrcAddr + 4 Bytes MCTP + 17 Bytes Payload)

// MCTP Header
pkt2[3] = (MCU_ADDR_7BIT << 1) | 1; 
pkt2[4] = 0x01; 
pkt2[5] = 0x00; 
pkt2[6] = 0x01; 
pkt2[7] = 0x58; // SOM=0, EOM=1, PktSeq=1, TO=1, MsgTag=0 (??? 0101 1000)

// ????? 17 Bytes NVMe-MI Payload (?? MIC)
memcpy(&pkt2[8], &mi_msg[55], 17);

// ?? PEC ??? I2C
pkt2[25] = calc_pec_noinit(pkt2, 25);
I2C_WriteMultiBytes(I2C_PORT, pkt2[0] >> 1, &pkt2[1], 25);

	
	
	
I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
		g_u8SlvRxFlag=0;
		WAIT_SLV_RX_TIMEOUT();
		//parse_mctp_smbus_response(g_au8SlvRxData, g_au8SlvRxData[1] + 2);
		I2C_DisableInt(I2C1);
		NVIC_DisableIRQ(I2C1_IRQn);	
		CLK_SysTickDelay(500000);
		#endif
}

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
    printf("  - No Support: NVMe Basic Management (SMBus Reg 0x00)\n");
    printf("  - CNTLID=0x01 returns Status=0x01 (only CNTLID=0x00 valid)\n");
    printf("  => Vendor info (VID/SN) NOT retrievable via NVMe-MI SMBus\n");
    printf("  => Use NVMe Admin Identify via PCIe for full device info\n");
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
    printf("\n>>> [CMD] NVMe-MI - Controller Health Status Poll (Opcode=0x02) <<<\n");
    printf("=== Controller Health Status Poll ===\n");

    uint8_t chsp_buf[24];

    // SMBus Header
    // Byte Count = src_addr(1) + MCTP_hdr(4) + NVMe-MI_payload(8) + MIC(4) = 17 = 0x11
    chsp_buf[0] = 0x3A;
    chsp_buf[1] = 0x0F;
    chsp_buf[2] = 0x11;  // Byte Count = 17

    // MCTP Header
    chsp_buf[3] = (MCU_ADDR_7BIT << 1) | 1;
    chsp_buf[4] = 0x01;
    chsp_buf[5] = 0x00;  // Dest EID
    chsp_buf[6] = 0x01;  // Src EID
    chsp_buf[7] = 0xC8;  // SOM=1, EOM=1

    // NVMe-MI Payload: Controller Health Status Poll
    chsp_buf[8]  = 0x84;  // Msg Type: NVMe-MI + IC bit
    chsp_buf[9]  = 0x08;  // NMIMT: NVMe-MI Command
    chsp_buf[10] = 0x00;  // Flags
    chsp_buf[11] = 0x00;  // CNTLID Low = 0
    chsp_buf[12] = 0x00;  // CNTLID High
    chsp_buf[13] = 0x02;  // Opcode: 0x02 = Controller Health Status Poll
    chsp_buf[14] = 0x00;  // Reserved
    chsp_buf[15] = 0x00;  // Reserved

    // MIC (covers bytes [8..15], 8 bytes)
    uint32_t chsp_mic = calc_mic(&chsp_buf[8], 8);
    chsp_buf[16] = (uint8_t)(chsp_mic & 0xFF);
    chsp_buf[17] = (uint8_t)((chsp_mic >> 8)  & 0xFF);
    chsp_buf[18] = (uint8_t)((chsp_mic >> 16) & 0xFF);
    chsp_buf[19] = (uint8_t)((chsp_mic >> 24) & 0xFF);

    // PEC
    chsp_buf[20] = calc_pec_noinit(chsp_buf, 20);

    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
    I2C_WriteMultiBytes(I2C_PORT, chsp_buf[0] >> 1, &chsp_buf[1], 20);

    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    g_u8SlvRxFlag = 0;
    while (g_u8SlvRxFlag == 0);
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);

    uint8_t chsp_status = g_au8SlvRxData[10];
    printf("  Status : 0x%02X (%s)\n", chsp_status,
           chsp_status == 0x00 ? "Success" : "Error");

    // Raw dump
    printf("  Raw   : ");
    {
        uint8_t r;
        for (r = 0; r < g_au8SlvRxData[1] + 2 && r < 32; r++)
            printf("%02X ", g_au8SlvRxData[r]);
        printf("\n");
    }

    if (chsp_status == 0x00)
    {
        uint8_t byte_cnt_chsp = g_au8SlvRxData[1];
        uint16_t csts   = (uint16_t)g_au8SlvRxData[11] |
                          ((uint16_t)g_au8SlvRxData[12] << 8);
        uint16_t temp_k = (byte_cnt_chsp >= 15) ?
                          ((uint16_t)g_au8SlvRxData[13] |
                           ((uint16_t)g_au8SlvRxData[14] << 8)) : 0;
        int temp_c = (temp_k >= 273) ? ((int)temp_k - 273) : 0;
        // MIC is 4 bytes at end; data at raw[15] valid only when ByteCount >= 18
        uint8_t spare    = (byte_cnt_chsp >= 18) ? g_au8SlvRxData[15] : 0xFF;
        uint8_t spare_th = (byte_cnt_chsp >= 19) ? g_au8SlvRxData[16] : 0xFF;
        uint8_t pdlu     = (byte_cnt_chsp >= 20) ? g_au8SlvRxData[17] : 0xFF;
        uint8_t cw       = (byte_cnt_chsp >= 21) ? g_au8SlvRxData[18] : 0xFF;

        printf("========================================\n");
        printf("  Controller Health Status Poll\n");
        printf("  (ByteCount=%d, body=%d bytes)\n", byte_cnt_chsp,
               byte_cnt_chsp >= 9 ? byte_cnt_chsp - 9 : 0);
        printf("========================================\n");
        printf("  CSTS (0x%04X):\n", csts);
        printf("    Ready              : %s\n", (csts & 0x0001) ? "Yes" : "No");
        printf("    Controller Fatal   : %s\n", (csts & 0x0002) ? "YES - FATAL" : "No");
        printf("    Shutdown Status    : %s\n",
               ((csts >> 2) & 0x3) == 0 ? "Normal" :
               ((csts >> 2) & 0x3) == 1 ? "Shutdown Occurring" :
               ((csts >> 2) & 0x3) == 2 ? "Shutdown Complete" : "?");

        printf("  Composite Temp      : ");
        if (temp_k == 0)     printf("No Data\n");
        else                 printf("%d C  (%d K)\n", temp_c, temp_k);

        if (spare != 0xFF)   printf("  Available Spare     : %d%%\n", spare);
        else                 printf("  Available Spare     : N/A (short response)\n");
        if (spare_th != 0xFF) printf("  Spare Threshold     : %d%%\n", spare_th);
        if (pdlu != 0xFF)    printf("  Drive Life Used     : %d%%\n", pdlu);
        if (cw != 0xFF) {
            printf("  SMART Critical Warn : 0x%02X", cw);
            if (cw == 0x00) printf(" (No Warnings)\n");
            else {
                printf("\n");
                if (cw & 0x01) printf("    [!] Spare below threshold\n");
                if (cw & 0x02) printf("    [!] Temperature above threshold\n");
                if (cw & 0x04) printf("    [!] NVM reliability degraded\n");
                if (cw & 0x08) printf("    [!] Media read-only\n");
                if (cw & 0x10) printf("    [!] Volatile backup failed\n");
            }
        }
        printf("========================================\n");
    }
    else
    {
        printf("  [NOTE] Controller Health Status Poll not supported (Status!=0)\n");
        printf("         Use NVM Subsystem Health Poll (Opcode=0x01) instead.\n");
    }

    CLK_SysTickDelay(300000);
    printf("=== Controller Health Status Poll Complete ===\n\n");
}

// =========================================================================
// CMD: Get Log Page - Firmware Slot Information (Admin PT, Log ID=0x03)
// =========================================================================
{
    printf("\n>>> [CMD] NVMe Admin PT - Get Log Page (Log ID=0x03 FW Slot Info) <<<\n");
    printf("=== Get FW Revision via Admin Passthrough ===\n");

    uint8_t fw_buf[29];

    fw_buf[0] = 0x3A;
    fw_buf[1] = 0x0F;
    fw_buf[2] = 0x19;
    fw_buf[3] = (MCU_ADDR_7BIT << 1) | 1;
    fw_buf[4] = 0x01;
    fw_buf[5] = 0x00;
    fw_buf[6] = 0x01;
    fw_buf[7] = 0xC8;
    fw_buf[8]  = 0x84;
    fw_buf[9]  = 0x08;
    fw_buf[10] = 0x00;
    fw_buf[11] = 0x00;
    fw_buf[12] = 0x00;
    fw_buf[13] = 0x06;  // NVMe-MI Opcode: Admin Passthrough
    fw_buf[14] = 0x02;  // NVMe Admin Opcode: Get Log Page
    fw_buf[15] = 0x00;
    fw_buf[16] = 0x03;  // Log ID = 0x03 (Firmware Slot Info)
    fw_buf[17] = 0x0F;  // NUMD = 15
    fw_buf[18] = 0x00;
    fw_buf[19] = 0x00;
    fw_buf[20] = 0x00;
    fw_buf[21] = 0x00;
    fw_buf[22] = 0x00;
    fw_buf[23] = 0x00;

    uint32_t fw_mic = calc_mic(&fw_buf[8], 16);
    fw_buf[24] = (uint8_t)(fw_mic & 0xFF);
    fw_buf[25] = (uint8_t)((fw_mic >> 8)  & 0xFF);
    fw_buf[26] = (uint8_t)((fw_mic >> 16) & 0xFF);
    fw_buf[27] = (uint8_t)((fw_mic >> 24) & 0xFF);
    fw_buf[28] = calc_pec_noinit(fw_buf, 28);

    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
    I2C_WriteMultiBytes(I2C_PORT, fw_buf[0] >> 1, &fw_buf[1], 28);

    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    g_u8SlvRxFlag = 0;
    while (g_u8SlvRxFlag == 0);
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);

    uint8_t fw_nmimt  = g_au8SlvRxData[8];
    uint8_t fw_status = g_au8SlvRxData[10];

    printf("  NMIMT response : 0x%02X (%s)\n", fw_nmimt,
           fw_nmimt == 0x89 ? "Admin PT Response [GOOD]" :
           fw_nmimt == 0x88 ? "NVMe-MI Response [Admin PT ignored!]" : "?");
    printf("  Status         : 0x%02X\n", fw_status);
    printf("  Raw: ");
    {
        uint8_t r;
        for (r = 0; r < g_au8SlvRxData[1] + 2 && r < 32; r++)
            printf("%02X ", g_au8SlvRxData[r]);
        printf("\n");
    }

    if (fw_nmimt == 0x89 && fw_status == 0x00)
    {
        uint8_t *log_data = &g_au8SlvRxData[12];
        uint8_t  log_len  = g_au8SlvRxData[1] > 12 ? g_au8SlvRxData[1] - 12 : 0;
        printf("========================================\n");
        printf("  FW Slot Information Log\n");
        printf("========================================\n");
        if (log_len >= 1)
            printf("  Active FW Slot : %d\n", log_data[0] & 0x07);
        if (log_len >= 16) {
            printf("  FW Slot 1 Rev  : [");
            uint8_t fi;
            for (fi = 0; fi < 8; fi++) {
                char c = (char)log_data[8 + fi];
                printf("%c", (c >= 0x20 && c <= 0x7E) ? c : '.');
            }
            printf("]\n");
        }
        printf("========================================\n");
    }
    else
    {
        printf("  [RESULT] Admin Passthrough NOT working on this device.\n");
        printf("           FW Revision NOT retrievable via NVMe-MI SMBus.\n");
        printf("  [CONCLUSION] Commands 3/4/7 require working Admin PT - NOT supported.\n");
    }

    CLK_SysTickDelay(300000);
    printf("=== FW Revision Attempt Complete ===\n\n");
}

// =========================================================================
// CMD: NVM Subsystem Health Status Poll - Health Info & Alerts (#5 #6)
// =========================================================================
{
    printf("\n>>> [CMD] NVMe-MI Health Status Alerts + Full Health Info <<<\n");
    printf("=== Get Health Info & Alerts (Opcode=0x01) ===\n");

    uint8_t hs_buf[24];

    // Byte Count = src_addr(1) + MCTP_hdr(4) + NVMe-MI_payload(8) + MIC(4) = 17 = 0x11
    hs_buf[0] = 0x3A;
    hs_buf[1] = 0x0F;
    hs_buf[2] = 0x11;
    hs_buf[3] = (MCU_ADDR_7BIT << 1) | 1;
    hs_buf[4] = 0x01;
    hs_buf[5] = 0x00;
    hs_buf[6] = 0x01;
    hs_buf[7] = 0xC8;
    hs_buf[8]  = 0x84;
    hs_buf[9]  = 0x08;
    hs_buf[10] = 0x00;
    hs_buf[11] = 0x00;
    hs_buf[12] = 0x00;
    hs_buf[13] = 0x01;  // Opcode: NVM Subsystem Health Status Poll
    hs_buf[14] = 0x00;
    hs_buf[15] = 0x00;

    uint32_t hs_mic = calc_mic(&hs_buf[8], 8);
    hs_buf[16] = (uint8_t)(hs_mic & 0xFF);
    hs_buf[17] = (uint8_t)((hs_mic >> 8)  & 0xFF);
    hs_buf[18] = (uint8_t)((hs_mic >> 16) & 0xFF);
    hs_buf[19] = (uint8_t)((hs_mic >> 24) & 0xFF);
    hs_buf[20] = calc_pec_noinit(hs_buf, 20);

    I2C_SetSlaveAddr(I2C1, 0, 0x10, I2C_GCMODE_DISABLE);
    s_I2C1HandlerFn = I2C_SlaveTRx;
    I2C_WriteMultiBytes(I2C_PORT, hs_buf[0] >> 1, &hs_buf[1], 20);

    I2C_EnableInt(I2C1);
    NVIC_EnableIRQ(I2C1_IRQn);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI | I2C_CTL_AA);
    g_u8SlvRxFlag = 0;
    while (g_u8SlvRxFlag == 0);
    I2C_DisableInt(I2C1);
    NVIC_DisableIRQ(I2C1_IRQn);

    uint8_t hs_status = g_au8SlvRxData[10];
    printf("  Status : 0x%02X\n", hs_status);

    if (hs_status == 0x00 && g_au8SlvRxData[1] >= 14)
    {
        uint8_t  hs_bc    = g_au8SlvRxData[1];
        uint16_t hs_csts  = (uint16_t)g_au8SlvRxData[11] |
                            ((uint16_t)g_au8SlvRxData[12] << 8);
        uint16_t temp_k   = (hs_bc >= 15) ?
                            ((uint16_t)g_au8SlvRxData[13] |
                             ((uint16_t)g_au8SlvRxData[14] << 8)) : 0;
        int temp_c = (temp_k >= 273) ? ((int)temp_k - 273) : 0;
        // MIC is 4 bytes at end; data at raw[15] valid only when ByteCount >= 18
        uint8_t spare    = (hs_bc >= 18) ? g_au8SlvRxData[15] : 0xFF;
        uint8_t spare_th = (hs_bc >= 19) ? g_au8SlvRxData[16] : 0xFF;
        uint8_t pdlu     = (hs_bc >= 20) ? g_au8SlvRxData[17] : 0xFF;
        uint8_t cw       = (hs_bc >= 21) ? g_au8SlvRxData[18] : 0xFF;

        printf("========================================\n");
        printf("  NVMe-MI Health Info & Alerts  (ByteCount=%d)\n", hs_bc);
        printf("========================================\n");
        printf("  [Health Info]\n");
        printf("    Composite Temp    : ");
        if (temp_k == 0)  printf("No Data\n");
        else              printf("%d C  (%d K)\n", temp_c, temp_k);
        printf("    CSTS              : 0x%04X (%s)\n", hs_csts,
               (hs_csts & 0x0001) ? "Ready" : "Not Ready");
        if (spare != 0xFF)    printf("    Available Spare   : %d%%\n", spare);
        else                  printf("    Available Spare   : N/A\n");
        if (spare_th != 0xFF) printf("    Spare Threshold   : %d%%\n", spare_th);
        if (pdlu != 0xFF)     printf("    Drive Life Used   : %d%%\n", pdlu);
        else                  printf("    Drive Life Used   : N/A\n");

        printf("\n  [Health Status Alerts]\n");
        uint8_t alert = 0;
        if (cw != 0xFF && cw != 0x00) {
            alert = 1;
            printf("    SMART Critical Warnings (0x%02X):\n", cw);
            if (cw & 0x01) printf("    [ALERT] Spare below threshold\n");
            if (cw & 0x02) printf("    [ALERT] Temperature above threshold\n");
            if (cw & 0x04) printf("    [ALERT] NVM reliability degraded\n");
            if (cw & 0x08) printf("    [ALERT] Media read-only\n");
            if (cw & 0x10) printf("    [ALERT] Volatile backup failed\n");
        }
        if (spare != 0xFF && spare_th != 0xFF && spare <= spare_th) {
            alert = 1;
            printf("    [ALERT] Spare %d%% <= Threshold %d%%\n", spare, spare_th);
        }
        if (pdlu != 0xFF && pdlu >= 90) {
            alert = 1;
            printf("    [WARN]  Drive life %d%% >= 90%%\n", pdlu);
        }
        if (temp_k > 0 && temp_c >= 70) {
            alert = 1;
            printf("    [WARN]  Temperature %d C >= 70 C\n", temp_c);
        }
        if (!alert)
            printf("    ** ALL CLEAR - No alerts **\n");
        printf("========================================\n");
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
    /* --- Config Get - SMBus Freq (confirmed working, CID in DWORD0 @body[8]) --- */
    {
        uint8_t body[16] = { 0x84,0x08,0,0, 0x04,0,0,0, 0x01,0,0,0, 0,0,0,0 };
        uint8_t b[29];
        b[0]=0x3A; b[1]=0x0F; b[2]=0x19;
        b[3]=(MCU_ADDR_7BIT<<1)|1; b[4]=0x01;
        b[5]=0x00; b[6]=0x01; b[7]=0xC8;
        { uint8_t k; for (k=0;k<16;k++) b[8+k]=body[k]; }
        uint32_t mc = calc_mic(&b[8],16);
        b[24]=mc&0xFF; b[25]=(mc>>8)&0xFF; b[26]=(mc>>16)&0xFF; b[27]=(mc>>24)&0xFF;
        b[28]=calc_pec_noinit(b,28);
        printf("\n>>> [CMD] NVMe-MI - Config Get - SMBus Freq (Opcode=0x04) <<<\n");
        I2C_SetSlaveAddr(I2C1,0,0x10,I2C_GCMODE_DISABLE);
        s_I2C1HandlerFn = I2C_SlaveTRx;
        I2C_WriteMultiBytes(I2C_PORT, b[0]>>1, &b[1], 28);
        I2C_EnableInt(I2C1); NVIC_EnableIRQ(I2C1_IRQn);
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI|I2C_CTL_AA);
        g_u8SlvRxFlag=0;
        WAIT_SLV_RX_TIMEOUT();
        I2C_DisableInt(I2C1); NVIC_DisableIRQ(I2C1_IRQn);
        printf("  ByteCnt=%d Status=0x%02X  Mgmt Response: %02X %02X %02X\n",
               g_au8SlvRxData[1], g_au8SlvRxData[10],
               g_au8SlvRxData[11], g_au8SlvRxData[12], g_au8SlvRxData[13]);
        CLK_SysTickDelay(300000);
    }

    /* --- VPD Read: incremental chunked read, DOFST@body[8], DLEN@body[12] --- */
    {
        uint8_t  vpd[256];
        uint16_t vpd_len = 0;
        uint16_t dofst;
        const uint8_t CHUNK = 0x20;   /* 32B/chunk: keeps each response in ONE MCTP packet */
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
