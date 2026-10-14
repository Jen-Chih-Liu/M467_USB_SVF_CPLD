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
#include "g_def.h"
extern volatile unsigned char fan_address_0x40_d ;
extern volatile unsigned char fan_address_0x42_d ;
extern volatile unsigned char fan_address_0x44_d ;
extern volatile unsigned char fan_address_0x46_d ;
volatile uint8_t bmc_report[1024] __attribute__((aligned(4))) = {0};
volatile uint8_t bmc_report1[1024] __attribute__((aligned(4))) = {0};
extern volatile uint8_t string_received;
extern volatile uint8_t svf_string_rcvbuf[8192] __attribute__((aligned(4)));
#define SVF_BUF_LENGTH 8192
extern volatile uint8_t usb_rcvbuf[1024] __attribute__((aligned(4)));
extern volatile uint16_t buffer_index;
extern volatile uint8_t response_buff[1024] __attribute__((aligned(4)));
volatile uint32_t total_line = 0;
volatile uint32_t false_line = 0;
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
volatile uint32_t i2c4_idle_timer = 0 ; /* ms counter: time since last I2C4 packet */

volatile unsigned char i2c_monitor_flag = 1;
volatile unsigned char reset_var = 0xa5;

volatile unsigned char mux_TCA9548_flag = 0;
int getbyte_usb_fun()
{
    if (svf_string_rcvbuf[pos] == '\0') return -1;

    return svf_string_rcvbuf[pos++];
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
    CLK_EnableModuleClock(I2C2_MODULE);
    CLK_EnableModuleClock(USCI0_MODULE);
    CLK_EnableModuleClock(I2C4_MODULE);
    CLK_EnableModuleClock(TMR0_MODULE);

    CLK_SetModuleClock(TMR0_MODULE, CLK_CLKSEL1_TMR0SEL_HXT, 0);

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Set multi-function pins for UART0 RXD and TXD */
    SET_UART3_RXD_PB14() ; // for dump message
    SET_UART3_TXD_PB15() ; // for dump message

    /* Set I2C0 multi-function pins */
    //CPLD0
    SET_I2C0_SDA_PB4();
    SET_I2C0_SCL_PB5();
    PB->SMTEN |= GPIO_SMTEN_SMTEN4_Msk | GPIO_SMTEN_SMTEN5_Msk ;

    //NVME0
    SET_I2C1_SDA_PB0();
    SET_I2C1_SCL_PB1();
    PB->SMTEN |= GPIO_SMTEN_SMTEN0_Msk | GPIO_SMTEN_SMTEN1_Msk ;
#if 0
    //CPLD 2
    SET_I2C2_SDA_PA10();
    SET_I2C2_SCL_PA11();
    PA->SMTEN |= GPIO_SMTEN_SMTEN10_Msk | GPIO_SMTEN_SMTEN11_Msk ;

    //NVME0
    SET_USCI0_DAT0_PB13(); //I2C DATA
    SET_USCI0_CLK_PB12(); //I2C CLK
    PB->SMTEN |= GPIO_SMTEN_SMTEN12_Msk | GPIO_SMTEN_SMTEN13_Msk ;
#endif

    //for bmc master
    SET_I2C4_SDA_PF4();
    SET_I2C4_SCL_PF5();
    PF->SMTEN |= GPIO_SMTEN_SMTEN4_Msk | GPIO_SMTEN_SMTEN5_Msk ;

    /* Enable EADC0 module clock */
    CLK_EnableModuleClock(EADC0_MODULE);

    /* Set EADC0 clock divider as 12 */
    CLK_SetModuleClock(EADC0_MODULE, CLK_CLKSEL0_EADC0SEL_PLL_DIV2, CLK_CLKDIV0_EADC0(12));

    SET_EADC0_CH7_PB7();
    GPIO_DISABLE_DIGITAL_PATH(PB, BIT7);
    /* Lock protected registers */

    SystemCoreClockUpdate();
    SYS_LockReg();
}

/**
 * @brief  I2C0 Interface Initialization.
 * @param  None
 * @return None
 */
void I2C0_Init(void)
{

    /* Open I2C0 and set clock to 100k */
    I2C_Open(I2C0, 100000);

    /* Get I2C0 Bus Clock */
    printf("I2C clock %d Hz\n", I2C_GetBusClockFreq(I2C0));
}

void I2C1_Init(void)
{

    /* Open I2C1 and set clock to 100k */
    I2C_Open(I2C1, 100000);

    /* Get I2C1 Bus Clock */
    printf("I2C clock %d Hz\n", I2C_GetBusClockFreq(I2C1));

}

void UI2C0_Init(void)
{
    /* Open USCI_I2C0 and set clock to 100k */
    UI2C_Open(UI2C0, 100000);

    /* Get UI2C0 Bus Clock */
    printf("UI2C clock %d Hz\n", UI2C_GetBusClockFreq(UI2C0));

    /* Set UI2C1 Slave Addresses */
    // UI2C_SetSlaveAddr(UI2C0, 0, 0x15, UI2C_GCMODE_DISABLE);   /* Slave Address : 0x15 */

    //UI2C_ENABLE_PROT_INT(UI2C0, (UI2C_PROTIEN_ACKIEN_Msk | UI2C_PROTIEN_NACKIEN_Msk | UI2C_PROTIEN_STORIEN_Msk | UI2C_PROTIEN_STARIEN_Msk));
    //NVIC_EnableIRQ(USCI0_IRQn);
}


/**
 * @brief  I2C2 Interface Initialization.
 * @param  None
 * @return None
 */
void I2C2_Init(void)
{

    /* Open I2C0 and set clock to 100k */
    I2C_Open(I2C2, 100000);

    /* Get I2C0 Bus Clock */
    printf("I2C clock %d Hz\n", I2C_GetBusClockFreq(I2C2));

}




void I2C4_Init(void)
{

    /* Open I2C0 and set clock to 100k */
    I2C_Open(I2C4, 100000);

    /* Get I2C0 Bus Clock */
   // printf("I2C clock %d Hz\n", I2C_GetBusClockFreq(I2C4));


    if (fan_address_0x40_d == 1)
        I2C_SetSlaveAddr(I2C4, 0, NCT7363Y_ADDR_0, I2C_GCMODE_DISABLE);

    if (fan_address_0x42_d == 1)
        I2C_SetSlaveAddr(I2C4, 1, NCT7363Y_ADDR_1, I2C_GCMODE_DISABLE);

    if (fan_address_0x44_d == 1)
        I2C_SetSlaveAddr(I2C4, 2, NCT7363Y_ADDR_2, I2C_GCMODE_DISABLE);

    if (fan_address_0x46_d == 1)
        I2C_SetSlaveAddr(I2C4, 3, NCT7363Y_ADDR_3, I2C_GCMODE_DISABLE);

    I2C_EnableInt(I2C4);
    NVIC_EnableIRQ(I2C4_IRQn);
}
typedef void (*I2C_FUNC)(uint32_t u32Status);
static I2C_FUNC s_I2C4HandlerFn = NULL;




void I2C4_IRQHandler(void)
{
    uint32_t u32Status;

    u32Status = I2C_GET_STATUS(I2C4);

    if (I2C_GET_TIMEOUT_FLAG(I2C4))
    {
        /* Clear I2C4 Timeout Flag */
        I2C_ClearTimeoutFlag(I2C4);
        //g_u8TimeoutFlag = 1;
    }
    else
    {
        if (s_I2C4HandlerFn != NULL)
            s_I2C4HandlerFn(u32Status);
    }
}

volatile unsigned char fan_address_index = 0;
volatile unsigned char fan_reg_index = 0;
volatile unsigned char fan_address_0x40[0xff] = {0};
volatile unsigned char fan_address_0x42[0xff] = {0};
volatile unsigned char fan_address_0x44[0xff] = {0};
volatile unsigned char fan_address_0x46[0xff] = {0};
volatile unsigned char g_au8SlvData[32] = {0};
volatile uint8_t g_u8SlvDataLen = 0;
volatile uint8_t g_u8SlvTRxAbortFlag = 0;
void I2C_SlaveTRx(uint32_t u32Status)
{
    uint8_t u8data;

    if (u32Status == 0x60)                      /* Own SLA+W has been receive; ACK has been return */
    {
        g_u8SlvDataLen = 0;
        fan_address_index = I2C_GET_DATA(I2C4) >> 1;
        i2c4_idle_timer = 0; /* Reset watchdog on every new I2C4 transaction */
        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if (u32Status == 0x80)                 /* Previously address with own SLA address
                                                   Data has been received; ACK has been returned*/
    {
        u8data = (unsigned char) I2C_GET_DATA(I2C4);

        if (g_u8SlvDataLen < sizeof(g_au8SlvData)) /* Guard against g_au8SlvData[32] overflow */
            g_au8SlvData[g_u8SlvDataLen++] = u8data;

        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if (u32Status == 0xA8)                 /* Own SLA+R has been receive; ACK has been return */
    {
        fan_address_index = I2C_GET_DATA(I2C4) >> 1;
        i2c4_idle_timer = 0; /* Reset watchdog on read transaction */

        if (fan_address_index == (NCT7363Y_ADDR_0))
        {
            I2C_SET_DATA(I2C4, fan_address_0x40[fan_reg_index]);
        }
        else if (fan_address_index == (NCT7363Y_ADDR_1))
        {
            I2C_SET_DATA(I2C4, fan_address_0x42[fan_reg_index]);
        }
        else if (fan_address_index == (NCT7363Y_ADDR_2))
        {
            I2C_SET_DATA(I2C4, fan_address_0x44[fan_reg_index]);
        }
        else if (fan_address_index == (NCT7363Y_ADDR_3))
        {
            I2C_SET_DATA(I2C4, fan_address_0x46[fan_reg_index]);
        }
        else
        {
            I2C_SET_DATA(I2C4, 0xff); // Return 0xFF for undefined addresses
        }

        if (fan_reg_index < 0xFE)
        {
            fan_reg_index++;
        }

        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if (u32Status == 0xB8)                 /* Data byte in I2CDAT has been transmitted ACK has been received */
    {
        if (fan_address_index == (NCT7363Y_ADDR_0))
        {
            I2C_SET_DATA(I2C4, fan_address_0x40[fan_reg_index]);
        }
        else if (fan_address_index == (NCT7363Y_ADDR_1))
        {
            I2C_SET_DATA(I2C4, fan_address_0x42[fan_reg_index]);
        }
        else if (fan_address_index == (NCT7363Y_ADDR_2))
        {
            I2C_SET_DATA(I2C4, fan_address_0x44[fan_reg_index]);
        }
        else if (fan_address_index == (NCT7363Y_ADDR_3))
        {
            I2C_SET_DATA(I2C4, fan_address_0x46[fan_reg_index]);
        }
        else
        {
            I2C_SET_DATA(I2C4, 0xff); // Return 0xFF for undefined addresses
        }

        if (fan_reg_index < 0xFE)
        {
            fan_reg_index++;
        }

        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI_AA);
    }
    else if (u32Status == 0xC0)                 /* Data byte or last data in I2CDAT has been transmitted
                                                   Not ACK has been received */
    {
        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if (u32Status == 0x88)                 /* Previously addressed with own SLA address; NOT ACK has
                                                   been returned */
    {
        g_u8SlvDataLen = 0;
        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if (u32Status == 0xA0)                 /* A STOP or repeated START has been received while still
                                             addressed as Slave/Receiver*/
    {
        if (g_u8SlvDataLen == 2) /* SMBus Write Byte: 1 command byte + 1 data byte */
        {
            if (fan_address_index == (NCT7363Y_ADDR_0))
            {
                fan_address_0x40[g_au8SlvData[0]] = g_au8SlvData[1];
            }
            else if (fan_address_index == (NCT7363Y_ADDR_1))
            {
                fan_address_0x42[g_au8SlvData[0]] = g_au8SlvData[1];
            }
            else if (fan_address_index == (NCT7363Y_ADDR_2))
            {
                fan_address_0x44[g_au8SlvData[0]] = g_au8SlvData[1];
            }
            else if (fan_address_index == (NCT7363Y_ADDR_3))
            {
                fan_address_0x46[g_au8SlvData[0]] = g_au8SlvData[1];
            }
        }
        else if (g_u8SlvDataLen == 1)
        {
            fan_reg_index = g_au8SlvData[0];
        }

        g_u8SlvDataLen = 0;
        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else
    {
        printf("[SlaveTRx] Status [0x%x] Unexpected abort!!\n", u32Status);

        if (u32Status == 0x68)              /* Slave receive arbitration lost, clear SI */
        {
            I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI_AA);
        }
        else if (u32Status == 0xB0)         /* Address transmit arbitration lost, clear SI  */
        {
            I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI_AA);
        }
        else                                /* Slave bus error, stop I2C and clear SI */
        {
            I2C_SET_CONTROL_REG(I2C4, I2C_CTL_STO_SI);
            I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI);
        }

        g_u8SlvTRxAbortFlag = 1;
    }

    I2C_WAIT_SI_CLEAR(I2C4);
}




/**
 * @brief  Timer0 Interrupt Handler.
 * @param  None
 * @return None
 */
void TMR0_IRQHandler(void)
{
    if (TIMER_GetIntFlag(TIMER0) == 1)
    {
        /* Clear Timer0 time-out interrupt flag */
        TIMER_ClearIntFlag(TIMER0);

        timer0_count++;
        i2c4_idle_timer++; /* Count ms for I2C4 watchdog */
    }
}

volatile uint32_t g_u32AdcIntFlag = 0;
volatile uint32_t g_u32clpd_lost = 0;
void EADC00_IRQHandler(void)
{
    g_u32AdcIntFlag = 1;
    EADC_CLR_INT_FLAG(EADC0, EADC_STATUS2_ADIF0_Msk);      /* Clear the A/D ADINT0 interrupt flag */
}

/**
 * @brief  Write a single register to NCT7363Y via I2C0.
 */
static void NCT7363Y_WriteReg(uint8_t dev_addr_8bit, uint8_t reg, uint8_t val)
{
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = val;
    I2C_WriteMultiBytes(I2C0, dev_addr_8bit >> 1, buf, 2);
}

/**
 * @brief  Set all active NCT7363Y fan ICs to full speed (PWM duty = 0xFF).
 *         Called when I2C4 has been idle for 30 seconds and PC14 == 0.
 *         NCT7363Y PWM duty output registers: 0x30~0x3F (PWM1~PWM16).
 */
void NCT7363Y_SetFullSpeed(void)
{
    uint8_t pwm_reg;

    /* PWM duty registers 0x30~0x3F correspond to PWM channel 1~16 */
    for (pwm_reg = 0x90; pwm_reg <= 0xAE; pwm_reg++)
    {
        if (fan_address_0x40_d == 1)
            fan_address_0x40[pwm_reg] = 0xFF; // Set new value in current array

        // NCT7363Y_WriteReg(NCT7363Y_ADDR_0, pwm_reg, 0xFF);
        if (fan_address_0x42_d == 1)
            fan_address_0x42[pwm_reg] = 0xFF; // Set new value in current array

        //  NCT7363Y_WriteReg(NCT7363Y_ADDR_1, pwm_reg, 0xFF);

        if (fan_address_0x44_d == 1)
            fan_address_0x44[pwm_reg] = 0xFF; // Set new value in current array

        // NCT7363Y_WriteReg(NCT7363Y_ADDR_2, pwm_reg, 0xFF);

        if (fan_address_0x46_d == 1)
            fan_address_0x46[pwm_reg] = 0xFF; // Set new value in current array

        //   NCT7363Y_WriteReg(NCT7363Y_ADDR_3, pwm_reg, 0xFF);
    }

    printf("[Watchdog] I2C4 idle 30s, NCT7363Y set to full speed!\n");
}

uint32_t I2C_WriteMultiBytes_detect(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t data[], uint32_t u32wLen)
{
    uint8_t u8Xfering = 1u, u8Err = 0u, u8Ctrl = 0u;
    uint32_t u32txLen = 0u, u32TimeOutCount = 0u;

    g_I2C_i32ErrCode = 0;

    I2C_START(i2c);                                                     /* Send START */

    while (u8Xfering && (u8Err == 0u))
    {
        u32TimeOutCount = I2C_TIMEOUT / 10;
        I2C_WAIT_READY(i2c)
        {
            if (--u32TimeOutCount == 0)
            {
                g_I2C_i32ErrCode = I2C_ERR_TIMEOUT;
                u8Err = 1u;
                break;
            }
        }

        switch (I2C_GET_STATUS(i2c))
        {
            case 0x08u:
                I2C_SET_DATA(i2c, (uint8_t)((u8SlaveAddr << 1u) | 0x00u));  /* Write SLA+W to Register I2CDAT */
                u8Ctrl = I2C_CTL_SI;                                        /* Clear SI */
                break;

            case 0x18u:                                                     /* Slave Address ACK */
            case 0x28u:
                if (u32txLen < u32wLen)
                {
                    I2C_SET_DATA(i2c, data[u32txLen++]);                    /* Write Data to I2CDAT */
                }
                else
                {
                    u8Ctrl = I2C_CTL_STO_SI;                                /* Clear SI and send STOP */
                    u8Xfering = 0u;
                }

                break;

            case 0x20u:                                                     /* Slave Address NACK */
            case 0x30u:                                                     /* Master transmit data NACK */
                u8Ctrl = I2C_CTL_STO_SI;                                    /* Clear SI and send STOP */
                u8Err = 1u;
                break;

            case 0x38u:                                                     /* Arbitration Lost */
            default:                                                        /* Unknown status */
                g_u32clpd_lost = 1;
                I2C_SET_CONTROL_REG(i2c, I2C_CTL_STO_SI);                   /* Clear SI and send STOP */
                u8Ctrl = I2C_CTL_SI;
                u8Err = 1u;
                break;
        }

        I2C_SET_CONTROL_REG(i2c, u8Ctrl);                               /* Write controlbit to I2C_CTL register */
    }

    u32TimeOutCount = I2C_TIMEOUT / 10;

    while ((i2c)->CTL0 & I2C_CTL0_STO_Msk)
    {
        if (--u32TimeOutCount == 0)
        {
            g_I2C_i32ErrCode = I2C_ERR_TIMEOUT;
            break;
        }
    }

    return u32txLen;                                                    /* Return bytes length that have been transmitted */
}

int I2C_WriteReg_test(uint8_t i2c_addr, uint8_t reg, uint8_t data)
{
    uint8_t temp_buf[2];
    temp_buf[0] = reg;
    temp_buf[1] = data;

    // The project's I2C functions use a 7-bit address.
    if (I2C_WriteMultiBytes(I2C0, i2c_addr, temp_buf, 2) == 2)
    {
        return 0; // Success
    }

    return -1; // Fail
}

int I2C_ReadReg_test(uint8_t i2c_addr, uint8_t reg, uint8_t *data)
{
    // Set the register pointer first
    if (I2C_WriteByte(I2C0, i2c_addr, reg) != 0)
    {
        return -1; // Fail to write register address
    }

    // Then read the data
    *data = I2C_ReadByte(I2C0, i2c_addr);
    return 0; // Success
}

uint8_t I2C_WriteByte_detect(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t data)
{
    uint32_t u32txLen = I2C_WriteMultiBytes_detect(i2c, u8SlaveAddr, &data, 1);

    if (u32txLen == 1u)
    {
        return 0;   /* Write data success */
    }
    else
    {
        return 1;   /* Write data fail, or bus occurs error events */
    }
}
extern  const uint8_t s_au8_TCA9548_Addr[];
extern const uint8_t s_au8_PCA9848_Addr[];
volatile uint8_t temp_w_buf[4] = {0};
volatile uint8_t temp_r_buf[4] = {0};
extern  uint8_t SelectMuxChannel(I2C_T *i2c_bus, uint8_t u8MuxAddr, uint8_t u8Channel);

/*--------------------------------------------------------------------------*/
/* DWT cycle-counter based profiling (Cortex-M4)                            */
/* Resolution = 1 CPU cycle (5 ns @ 200 MHz). CYCCNT is 32-bit and wraps    */
/* after ~21 s at 200 MHz, which is far longer than any function measured.  */
/*--------------------------------------------------------------------------*/
static void DWT_ProfileInit(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; /* Enable trace subsystem */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;           /* Enable cycle counter   */
}

/* Measure 'code', print cycles and microseconds with a label. */
#define PROFILE_US(label, code)                                             \
    do {                                                                    \
        uint32_t _t0 = DWT->CYCCNT;                                          \
        code;                                                               \
        uint32_t _cyc = DWT->CYCCNT - _t0;                                   \
        printf("%-24s %10lu cyc  %8lu us\n\r", (label),                      \
               (unsigned long)_cyc,                                         \
               (unsigned long)(_cyc / (SystemCoreClock / 1000000UL)));      \
    } while (0)

/**
 * @brief  Main function.
 * @param  None
 * @return N/A
 */
int main(void)
{
    int i;
    unsigned char  data_len;
    /* Init System, peripheral clock and multi-function I/O */
    SYS_Init();

    /* Enable DWT cycle counter for function profiling */
    DWT_ProfileInit();

    /* Init UART to 115200-8n1 for print message */
    UART_Open(UART3, 115200);
    GPIO_SetPullCtl(PB, BIT14, GPIO_PUSEL_PULL_UP);

    NVIC_EnableIRQ(UART3_IRQn);
    UART_EnableInt(UART3, UART_INTEN_RDAIEN_Msk);
    // Generate a unique USB serial number from the MCU's UID.
    Set_USB_SerialNumber_From_UID();
    HSUSBD_Open(&gsHSInfo, HID_ClassRequest, NULL);
    HSUSBD_SetVendorRequest(HID_VendorRequest);

    /* Endpoint configuration */
    HID_Init();

    /* Enable HSUSBD interrupt */
    NVIC_EnableIRQ(USBD20_IRQn);
    HSUSBD_DISABLE_USB();

    /* Enable USB */
    HSUSBD_ENABLE_USB();
    // Configure JTAG GPIOs for fast slew rate and enable Schmitt triggers for stable input.
    GPIO_SetSlewCtl(PA, BIT7, GPIO_SLEWCTL_FAST);
    GPIO_SetSlewCtl(PA, BIT6, GPIO_SLEWCTL_FAST);
    GPIO_SetSlewCtl(PC, BIT1, GPIO_SLEWCTL_FAST);
    GPIO_SetSlewCtl(PC, BIT0, GPIO_SLEWCTL_FAST);
    PA->SMTEN |= GPIO_SMTEN_SMTEN7_Msk | GPIO_SMTEN_SMTEN6_Msk ;
    PC->SMTEN |= GPIO_SMTEN_SMTEN1_Msk | GPIO_SMTEN_SMTEN0_Msk ;

    GPIO_SetMode(PC, BIT14, GPIO_MODE_OUTPUT);
    PC14 = 0;
    printf("\n\nCPU @ %dHz\n", SystemCoreClock);
    printf("inital scan:%d\n\r", xsvftool_esp_scan());
    printf("jtag id 0x%x\n\r", xsvftool_esp_id());

    // Store the scanned CPLD JTAG ID into the bmc_report buffer.
    bmc_report[cpld_jtag_id] = (xsvftool_esp_id() >> 24) & 0xff;
    bmc_report[cpld_jtag_id + 1] = (xsvftool_esp_id() >> 16) & 0xff;
    bmc_report[cpld_jtag_id + 2] = (xsvftool_esp_id() >> 8) & 0xff;
    bmc_report[cpld_jtag_id + 3] = (xsvftool_esp_id() >> 0) & 0xff;

    printf("inital scan:%d\n\r", xsvftool_esp_scan());
    bmc_report1[cpld_jtag_id] = (xsvftool_esp_id() >> 24) & 0xff;
    bmc_report1[cpld_jtag_id + 1] = (xsvftool_esp_id() >> 16) & 0xff;
    bmc_report1[cpld_jtag_id + 2] = (xsvftool_esp_id() >> 8) & 0xff;
    bmc_report1[cpld_jtag_id + 3] = (xsvftool_esp_id() >> 0) & 0xff;

    I2C0_Init();
    I2C1_Init();

    GPIO_SetMode(PA, BIT10, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PA, BIT11, GPIO_MODE_OUTPUT);
    PA10 = 1;
    PA11 = 1;
    GPIO_SetMode(PA, BIT10, GPIO_MODE_INPUT);
    GPIO_SetMode(PA, BIT11, GPIO_MODE_INPUT);
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();

    if (PA10 != 1 || PA11 != 1)
    {
        g_u32clpd_lost = 1;
        goto cpld1_init;
    }

    GPIO_SetMode(PA, BIT10, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PA, BIT11, GPIO_MODE_OUTPUT);
    PA10 = 0;
    PA11 = 0;
    GPIO_SetMode(PA, BIT10, GPIO_MODE_INPUT);
    GPIO_SetMode(PA, BIT11, GPIO_MODE_INPUT);
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();

    if (PA10 == 0 || PA11 == 0)
    {
        g_u32clpd_lost = 1;
    }


cpld1_init:

    if (g_u32clpd_lost == 0)
    {
        SYS_UnlockReg();
        //CPLD 2
        SET_I2C2_SDA_PA10();
        SET_I2C2_SCL_PA11();
        PA->SMTEN |= GPIO_SMTEN_SMTEN10_Msk | GPIO_SMTEN_SMTEN11_Msk ;

        //NVME0
        SET_USCI0_DAT0_PB13(); //I2C DATA
        SET_USCI0_CLK_PB12(); //I2C CLK
        PB->SMTEN |= GPIO_SMTEN_SMTEN12_Msk | GPIO_SMTEN_SMTEN13_Msk ;

        I2C2_Init();
        UI2C0_Init();
        SYS_LockReg();
    }

    if (SelectMuxChannel(I2C1, s_au8_PCA9848_Addr[0], 1) == 1)
    {
        mux_TCA9548_flag = 0;
    }

    if (SelectMuxChannel(I2C1, s_au8_TCA9548_Addr[0], 1) == 1)
    {
        mux_TCA9548_flag = 1;
    }

    //I2C_WriteByte_detect(I2C2, cpld_adr, cpld_ver);
    //eeprom test
#if 0
    temp_w_buf[0] = 0xf3;
    temp_w_buf[1] = 0x01;
    temp_w_buf[2] = I2C_WriteMultiBytes(I2C0, cpld_adr, &temp_w_buf[0], 2);

    if (I2C_WriteByte(I2C0, cpld_adr, 0xf0) == 0)
    {
        temp_w_buf[2] = I2C_ReadByte(I2C0, cpld_adr);
    }

    EEPROM_WriteData(0x00, &temp_w_buf[0], 2);
    //temp_w_buf[0]=0xf3;
    //temp_w_buf[1]=0x00;
    // I2C_WriteMultiBytes(I2C0, cpld_adr,temp_w_buf, 2);
    EEPROM_ReadData(0x00, &temp_r_buf[0], 2);

    //2
    temp_w_buf[0] = 0xf3;
    temp_w_buf[1] = 0x01;
    temp_w_buf[2] = I2C_WriteMultiBytes(I2C2, cpld_adr, &temp_w_buf[0], 2);

    if (I2C_WriteByte(I2C2, cpld_adr, 0xf0) == 0)
    {
        temp_w_buf[2] = I2C_ReadByte(I2C2, cpld_adr);
    }

    EEPROM_WriteData_1(0x00, &temp_w_buf[0], 2);
    //temp_w_buf[0]=0xf3;
    //temp_w_buf[1]=0x00;
    // I2C_WriteMultiBytes(I2C0, cpld_adr,temp_w_buf, 2);
    EEPROM_ReadData_1(0x00, &temp_r_buf[0], 2);




    nvm_mi_read();
#endif
    //test
    //CPLD_read();
    //nvm_mi_read();
#if 0
    printf("temperature sensor read %f celsius\n\r",
           show_temperature(bmc_report[map_tempersensor_high], bmc_report[map_tempersensor_low]));
    show_cpld_information(&bmc_report[0]);
    uint8_t nvme_i;

    // Loop through all detected NVMe drives and print their information.
    for (nvme_i = 0; nvme_i < bmc_report[cpld_hdd_amount]; nvme_i++)
    {
        // The hardware supports up to 24 drives. Stop if index goes beyond.
        if (nvme_i >= 24) break;

        print_nvme_basic_management_info(&bmc_report[NVME_MEM_OFFSET + (nvme_i * NVME_READ_COUNT)]);
    }

#endif
    PC14 = 1;
    HSUSBD_Start();
    // Initialize the USB response buffer to zero.
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


    PC14 = 0;
    unsigned char data_loa = 0;

    if (I2C_ReadReg_test(NCT7363Y_ADDR_0, 0, &data_loa) == 0)
    {
        fan_address_0x40_d = 1;
    }

    if (I2C_ReadReg_test(NCT7363Y_ADDR_1, 0, &data_loa) == 0)
    {
        fan_address_0x42_d = 1;
    }

    if (I2C_ReadReg_test(NCT7363Y_ADDR_2, 0, &data_loa) == 0)
    {
        fan_address_0x44_d = 1;
    }

    if (I2C_ReadReg_test(NCT7363Y_ADDR_3, 0, &data_loa) == 0)
    {
        fan_address_0x46_d = 1;
    }

    FanIC_Backup_init();
    PC14 = 1;
    I2C4_Init();
    s_I2C4HandlerFn = I2C_SlaveTRx;
    I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    //smbus sel


    //9848 SMBus reset
    GPIO_SetMode(PB, BIT6, GPIO_MODE_OUTPUT);
    EADC_Open(EADC0, EADC_CTL_DIFFEN_SINGLE_END);
    EADC_ConfigSampleModule(EADC0, 0, EADC_ADINT0_TRIGGER, 7);
    EADC_CLR_INT_FLAG(EADC0, EADC_STATUS2_ADIF0_Msk);
    EADC_ENABLE_INT(EADC0, BIT0);//Enable sample module  A/D ADINT0 interrupt.
    EADC_ENABLE_SAMPLE_MODULE_INT(EADC0, 0, BIT0);//Enable sample module 0 interrupt.
    NVIC_EnableIRQ(EADC00_IRQn);
    g_u32AdcIntFlag = 0;
    EADC_START_CONV(EADC0, BIT0);

    while (g_u32AdcIntFlag == 0);

    unsigned int adc_val = EADC_GET_CONV_DATA(EADC0, 0);
    NVIC_DisableIRQ(EADC00_IRQn);
    EADC_Close(EADC0);

    // Main application loop.
    while (1)
    {

        // Periodic task executed every 500ms.
        if (timer0_count > 500)
        {
#if 1

            // If monitoring is enabled, read data from all sensors.
            if (i2c_monitor_flag == 1)
            {
                if (PC14 == 0)
                {
                    static uint8_t s_u8ProfileDone = 0; /* Run DWT profiling only once */

                    if (s_u8ProfileDone == 0)
                    {
                        PROFILE_US("FanIC_BackupRegisters",   FanIC_BackupRegisters());
                        PROFILE_US("FanIC_CompareAndRestore", FanIC_CompareAndRestore());
                        PROFILE_US("CPLD_read",               CPLD_read());          // Read CPLD status.
                        //fan_read();           // Read fan speed and duty cycle.
                        PROFILE_US("tempersensor_read",       tempersensor_read());  // Read temperature sensor.
                        PROFILE_US("nvm_mi_read",             nvm_mi_read());        // Read NVMe drive information.

                        if (g_u32clpd_lost == 0)
                        {
                            PROFILE_US("CPLD_read1",  CPLD_read1());          // Read CPLD status.
                            PROFILE_US("nvm_mi_read_1", nvm_mi_read_1());
                        }

                        s_u8ProfileDone = 1; /* Disable profiling after first measurement */
                    }
                    else
                    {
                        FanIC_BackupRegisters();
                        FanIC_CompareAndRestore();
                        CPLD_read();          // Read CPLD status.
                        //fan_read();           // Read fan speed and duty cycle.
                        tempersensor_read();  // Read temperature sensor.
                        nvm_mi_read();        // Read NVMe drive information.

                        if (g_u32clpd_lost == 0)
                        {
                            CPLD_read1();          // Read CPLD status.
                            nvm_mi_read_1();
                        }
                    }
                }


            }


#endif
            timer0_count = 0;
        }

        /* Enable I2C4 interrupt when PC14 == 0, disable when PC14 == 1 */
        {
            static uint8_t pc14_last = 0xFF; /* 0xFF = invalid, forces first update */
            uint8_t pc14_cur = (uint8_t)PC14;

            if (pc14_cur != pc14_last)
            {
                if (pc14_cur == 0)
                {
									    SET_I2C4_SDA_PF4();
                      SET_I2C4_SCL_PF5();
    I2C4_Init();
    s_I2C4HandlerFn = I2C_SlaveTRx;
    I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);                   
                }
                else
                {
                    NVIC_DisableIRQ(I2C4_IRQn);
                    I2C_DisableInt(I2C4);
									I2C_Close(I2C4);
									    SET_GPIO_PF4();
                      SET_GPIO_PF5();
									    GPIO_SetMode(PF, BIT4, GPIO_MODE_INPUT);
									GPIO_SetMode(PF, BIT5, GPIO_MODE_INPUT);
                }

                pc14_last = pc14_cur;
            }
        }

        /* I2C4 watchdog: if no packet received for 30 seconds and PC14 == 0,
         * force all NCT7363Y fan ICs to full speed as a safety measure. */
        if (i2c4_idle_timer >= 30000 && PC14 == 0)
        {
            NCT7363Y_SetFullSpeed();
            i2c4_idle_timer = 0; /* Reset to avoid repeated calls every ms */
        }




        // If the "dumplog" command is received via UART, print all collected data.
        if (g_u8DumpLogFlag == 1)
        {
#if 1
            printf("temperature sensor read %f celsius\n\r",
                   show_temperature(bmc_report[map_tempersensor_high], bmc_report[map_tempersensor_low]));
            show_cpld_information(&bmc_report[0]);
            uint8_t nvme_i;

            // Loop through all detected NVMe drives and print their information.
            for (nvme_i = 0; nvme_i < bmc_report[cpld_hdd_amount]; nvme_i++)
            {
                // The hardware supports up to 24 drives. Stop if index goes beyond.
                if (nvme_i >= 25) break;

                print_nvme_basic_management_info(&bmc_report[NVME_MEM_OFFSET + (nvme_i * NVME_READ_COUNT)]);
            }

#endif
            g_u8DumpLogFlag = 0;
        }

        // Process incoming USB commands.
        if (string_received == TRUE)
        {
            //PA4=0;
            __set_PRIMASK(1);
            //svf command parser

            /* -------------------------------------------------------
             * 0xa2 : Continuation packet for a multi-packet SVF command.
             *        Payload bytes [1..1023] are appended to
             *        svf_string_rcvbuf[] starting at buffer_index.
             *        No SVF processing yet �V just accumulate.
             * ------------------------------------------------------- */
            if (usb_rcvbuf[0] == 0xa2)
            {
                uint16_t k;

                for (k = 0; k < 1023; k++)
                {
                    if ((uint32_t)(buffer_index + k) >= SVF_BUF_LENGTH - 1)
                        break; /* guard against overflow */

                    svf_string_rcvbuf[buffer_index + k] = usb_rcvbuf[k + 1];
                }

                buffer_index = (uint16_t)(buffer_index + k);
                /* No response sent �V host does not read between 0xa2 packets */
            }

            /* -------------------------------------------------------
             * 0xa1 : Final (or only) packet for an SVF command.
             *        Appends payload to svf_string_rcvbuf starting at
             *        buffer_index, then processes the complete command.
             * ------------------------------------------------------- */
            if (usb_rcvbuf[0] == 0xa1)
            {
#if 1
                uint16_t terminator_pos = 0;
                uint16_t write_start    = buffer_index; /* where to append */

                // Copy the SVF command string from the USB buffer until a ';' is found.
                for (terminator_pos = 0; terminator_pos < 1023; terminator_pos++)
                {
                    if ((uint32_t)(write_start + terminator_pos) >= SVF_BUF_LENGTH - 1)
                        break; /* guard against overflow */

                    svf_string_rcvbuf[write_start + terminator_pos] = usb_rcvbuf[terminator_pos + 1];

                    if (svf_string_rcvbuf[write_start + terminator_pos] == ';')
                    {
                        buffer_index = (uint16_t)(write_start + terminator_pos);
                        break;
                    }
                }

                svf_string_rcvbuf[buffer_index + 1] = 0x00;
#endif

                pos = 0;
                // Call the SVF player function to execute the command packet.
                retval = xsvftool_esp_svf_packet(getbyte_usb_fun, total_line /*??*/, final /*0 or 1*/, report);

                if ((retval != 0) && (cpld_false_flag != 0))
                {
                    cpld_false_flag = 1;
                    false_line = total_line;
                }

                // If the SVF player returns an error and it's the first error, record the line number.
                if ((retval != 0) && (cpld_false_flag == 0))
                {
                    false_line = total_line;
                    response_buff[0] = (total_line) & 0xff;
                    response_buff[1] = (total_line >> 8) & 0xff;
                    response_buff[2] = (total_line >> 16) & 0xff;
                    response_buff[3] = (total_line >> 24) & 0xff;
                    cpld_false_flag = 1;
                }

                memset((void *)svf_string_rcvbuf, 0x0, (uint32_t)(buffer_index + 2));
                buffer_index = 0;
                total_line++;

                // Prepare and send the response (error line number or 0) back to the host.
                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }

            // Command 0xa0: Initialize SVF programming states.
            if (usb_rcvbuf[0] == 0xa0)
            {
                total_line = 0;
                false_line = 0;
                cpld_false_flag = 0;
                buffer_index = 0; /* reset accumulation buffer in case a previous transfer was interrupted */
                memset((void *)svf_string_rcvbuf, 0x0, SVF_BUF_LENGTH);
                response_buff[0] = 0;
                response_buff[1] = 0;
                response_buff[2] = 0;
                response_buff[3] = 0;

                // Prepare and send a cleared response buffer.
                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }

#if 1

            // Command 0xb0: Get firmware version.
            if (usb_rcvbuf[0] == 0xb0)
            {
                response_buff[0] = 0x26;
                response_buff[1] = 0x10;
                response_buff[2] = 0x12;
                response_buff[3] = 0x01;

                // Prepare and send the version number response.
                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }

            // Command 0xb1: Boot to LDROM for ISP (In-System Programming).
            if ((usb_rcvbuf[0] == 0xb1)
                    && (usb_rcvbuf[1] == 0x5a)
                    && (usb_rcvbuf[2] == 0xa5)
               )
            {
                SYS_UnlockReg();
                outpw(&SYS->RSTSTS, 3);//clear bit
                /* Enable ISP */
                CLK->AHBCLK0 |= CLK_AHBCLK0_ISPCKEN_Msk;
                FMC->ISPCTL |= FMC_ISPCTL_ISPEN_Msk;
                /* Reset system and boot from LDPROM */
                FMC_SetVectorPageAddr(FMC_LDROM_BASE);
                NVIC_SystemReset();
            }

            // Command 0xb2: Perform a chip reset and boot from APROM (normal operation).
            if ((usb_rcvbuf[0] == 0xb2)
                    && (usb_rcvbuf[1] == 0x55)
                    && (usb_rcvbuf[2] == 0xaa)
               )
            {
                SYS_UnlockReg();
                /* Enable ISP */
                CLK->AHBCLK0 |= CLK_AHBCLK0_ISPCKEN_Msk;
                FMC->ISPCTL |= FMC_ISPCTL_ISPEN_Msk;
                /* Reset system and boot from LDPROM */
                FMC_SetVectorPageAddr(FMC_APROM_BASE);
                NVIC_SystemReset();
            }

            // Command 0xb2, subcommand 0x5a: Set the reset variable.
            if ((usb_rcvbuf[0] == 0xb2)
                    && (usb_rcvbuf[1] == 0x5a)
               )
            {
                reset_var = usb_rcvbuf[2];
            }

            // Command 0xb2, subcommand 0x6a: Get the reset variable.
            if ((usb_rcvbuf[0] == 0xb2)
                    && (usb_rcvbuf[1] == 0x6a)
               )
            {
                response_buff[0] = 0xb2;
                response_buff[1] = reset_var;



                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }

            // Command 0xb3: Get LED GPIO pin definitions.
            if ((usb_rcvbuf[0] == 0xb3)

               )
            {
                response_buff[0] = 0xb3;
                response_buff[1] = GLED_AMB_N_R;
                response_buff[2] = GLED_GRN_N_R;


                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }


#endif

            // Command 0xc0: Read all BMC report data.
            if (usb_rcvbuf[0] == 0xc0)
            {
                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = bmc_report[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);

            }


            // Command 0xc1: programming eeprom
            if (usb_rcvbuf[0] == 0xc1)
            {
                //switch to bmc
                temp_w_buf[0] = 0xf3;
                temp_w_buf[1] = 0x01;
                I2C_WriteMultiBytes(I2C0, cpld_adr, temp_w_buf, 2);
                EEPROM_WriteData(0x00, &usb_rcvbuf[1], 256);
                temp_w_buf[0] = 0xf3;
                temp_w_buf[1] = 0x00;
                I2C_WriteMultiBytes(I2C0, cpld_adr, temp_w_buf, 2);
            }

            //command 0xc2 read: eeprom
            if (usb_rcvbuf[0] == 0xc2)
            {
                //switch to bmc
                temp_w_buf[0] = 0xf3;
                temp_w_buf[1] = 0x01;
                I2C_WriteMultiBytes(I2C0, cpld_adr, temp_w_buf, 2);

                response_buff[0] = 0xc2;
                EEPROM_ReadData(0x00, &response_buff[1], 256);

                temp_w_buf[0] = 0xf3;
                temp_w_buf[1] = 0x00;
                I2C_WriteMultiBytes(I2C0, cpld_adr, temp_w_buf, 2);

                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);


            }


            // Command 0xc5: programming eeprom1, I2C 2
            if (usb_rcvbuf[0] == 0xc5)
            {
                if (g_u32clpd_lost == 0)
                {
                    temp_w_buf[0] = 0xf3;
                    temp_w_buf[1] = 0x01;
                    I2C_WriteMultiBytes(I2C2, cpld_adr, &temp_w_buf[0], 2);

                    EEPROM_WriteData_1(0x00, &usb_rcvbuf[1], 256);


                    temp_w_buf[0] = 0xf3;
                    temp_w_buf[1] = 0x00;
                    I2C_WriteMultiBytes(I2C2, cpld_adr, &temp_w_buf[0], 2);

                }
            }

            //command 0xc6 read: eeprom1, I2C 2
            if (usb_rcvbuf[0] == 0xc6)
            {
                response_buff[0] = 0x00;

                if (g_u32clpd_lost == 0)
                {
                    temp_w_buf[0] = 0xf3;
                    temp_w_buf[1] = 0x01;
                    I2C_WriteMultiBytes(I2C2, cpld_adr, &temp_w_buf[0], 2);

                    response_buff[0] = 0xc6;
                    EEPROM_ReadData_1(0x00, &response_buff[1], 256);

                    temp_w_buf[0] = 0xf3;
                    temp_w_buf[1] = 0x00;
                    I2C_WriteMultiBytes(I2C2, cpld_adr, &temp_w_buf[0], 2);

                }

                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }


            if (usb_rcvbuf[0] == 0xc4)
            {
                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = bmc_report1[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);

            }

            // Command 0xd0: I2C write.
            if (usb_rcvbuf[0] == 0xd0)
            {
                if (usb_rcvbuf[1] == 0)
                    i2c_write_bytes = I2C_WriteMultiBytes(I2C0, usb_rcvbuf[2] >> 1, &usb_rcvbuf[4], usb_rcvbuf[3]);

                if (usb_rcvbuf[1] == 1)
                    i2c_write_bytes = I2C_WriteMultiBytes(I2C2, usb_rcvbuf[2] >> 1, &usb_rcvbuf[4], usb_rcvbuf[3]);
            }

            // Command 0xd1: Acknowledge I2C write.
            if (usb_rcvbuf[0] == 0xd1)
            {

                response_buff[0] = 0xd1;
                response_buff[1] = i2c_write_bytes;

                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }

            // Command 0xd2: I2C read.
            if (usb_rcvbuf[0] == 0xd2)
            {
                if (usb_rcvbuf[1] == 0)
                    i2c_read_bytes = I2C_ReadMultiBytes(I2C0, usb_rcvbuf[2] >> 1, i2c_read_report, usb_rcvbuf[3]);


                if (usb_rcvbuf[1] == 1)
                    i2c_read_bytes = I2C_ReadMultiBytes(I2C2, usb_rcvbuf[2] >> 1, i2c_read_report, usb_rcvbuf[3]);
            }

            // Command 0xd3: Acknowledge I2C read and return data.
            if (usb_rcvbuf[0] == 0xd3)
            {

                response_buff[0] = 0xd3;
                response_buff[1] = i2c_read_bytes;

                for (i = 0; i < i2c_read_bytes; i++)
                {
                    response_buff[i + 2] = i2c_read_report[i];
                }

                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }


            // Command 0xd4: I2C write-then-read transaction.
            if (usb_rcvbuf[0] == 0xd4)
            {
                if (usb_rcvbuf[1] == 0)
                {
                    i2c_write_bytes = I2C_WriteMultiBytes(I2C0, usb_rcvbuf[2] >> 1, &usb_rcvbuf[5], usb_rcvbuf[3]);
                    i2c_read_bytes = I2C_ReadMultiBytes(I2C0, usb_rcvbuf[2] >> 1, i2c_read_report, usb_rcvbuf[4]);
                }

                if (usb_rcvbuf[1] == 1)
                {
                    i2c_write_bytes = I2C_WriteMultiBytes(I2C2, usb_rcvbuf[2] >> 1, &usb_rcvbuf[5], usb_rcvbuf[3]);
                    i2c_read_bytes = I2C_ReadMultiBytes(I2C2, usb_rcvbuf[2] >> 1, i2c_read_report, usb_rcvbuf[4]);
                }

                // Prepare a response with write/read status and the data read.
                response_buff[0] = 0xd4;
                response_buff[1] = i2c_write_bytes;
                response_buff[2] = i2c_read_bytes;

                for (i = 0; i < i2c_read_bytes; i++)
                {
                    response_buff[i + 3] = i2c_read_report[i];
                }

                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);

            }

#if 0

            if (usb_rcvbuf[0] == 0xd5) //i2c write and read
            {

                response_buff[0] = 0xd5;
                response_buff[1] = i2c_write_bytes;
                response_buff[2] = i2c_read_bytes;

                for (i = 0; i < i2c_read_bytes; i++)
                {
                    response_buff[i + 3] = i2c_read_report[i];
                }

                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }

#endif

            // Command 0xda: Enable or disable the I2C monitoring task.
            if (usb_rcvbuf[0] == 0xda)
            {
                i2c_monitor_flag = usb_rcvbuf[1];
                printf("mf=0x%x\n\r", i2c_monitor_flag);
#if 0

                if (i2c_monitor_flag == 1)
                {
                    CPLD_read_AFTER();
                }

#endif
            }


            // Command 0xdb: Get the status of the I2C monitoring flag.
            if (usb_rcvbuf[0] == 0xdb)
            {

                response_buff[0] = 0xdb;
                response_buff[1] = i2c_monitor_flag;

                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }


            //set pc14
            if (usb_rcvbuf[0] == 0xdc)
            {

                if (usb_rcvbuf[1] == 40)
                {
                    PC14 = usb_rcvbuf[2];

                    if (PC14 == 0)
                    {
                        CPLD_read();

                        if (g_u32clpd_lost == 0)
                        {
                            CPLD_read1();
                        }
                    }
                }

                if (usb_rcvbuf[1] == 48)
                {
                    PB6 = usb_rcvbuf[2];
                }


            }

            // Command 0xdb: Get the status of the I2C monitoring flag.
            if (usb_rcvbuf[0] == 0xdd)
            {

                response_buff[0] = 0xdd;

                if (usb_rcvbuf[1] == 40)
                    response_buff[1] = PC14;

                if (usb_rcvbuf[1] == 48)
                    response_buff[1] = PB6;

                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }

            /* -----------------------------------------------------------------
             * Command 0xe0 : m463 NVMe-MI single-command execution.
             *   Request  : [0]=0xe0 [1]=NVMe index (0-based) [2]=CommandID(1..19)
             *              [3..6]=Dword0 (little-endian) [7..10]=Dword1 (little-endian)
             *   Response : [0]=0xe0 [1]=CommandID [2]=Status(0=OK,1=Fail)
             *              [3]=Data byte count (success only)
             *              [4..]=raw data bytes (success) OR
             *                    null-terminated ASCII reason (fail)
             *
             * CommandID map (matches host m463 NVMEMI list):
             *   1 Basic Management     2 Get Endpoint UUID   3 Get Msg Type Support
             *   4 Read MI Data Struct  5 Configuration Set   6 Configuration Get
             *   7 VPD Read             8 VPD Write           9 Reset (unsupported)
             *  10 Health Status Poll  11 Ctrl Health Poll   12 Health Info+Alerts
             *  13 Power Management    14 FW Revision        15 Health Alerts
             *  16 Health Info         17 SMART Log          18 Device Identify
             *  19 Format NVM
             *
             * Only CommandID 1 (Basic Management) is implemented in production
             * firmware; it returns the 32-byte Basic Management structure cached
             * in bmc_report[]. CommandID 2..19 are MCTP/NVMe-MI over-SMBus
             * commands whose full reference implementation lives in
             * main_mctp_test.c and has not yet been ported here, so they return
             * Status=Fail with a reason string.
             * ----------------------------------------------------------------- */
            if (usb_rcvbuf[0] == 0xe0)
            {
                uint8_t nvme_index = usb_rcvbuf[1];
                uint8_t cmd_id     = usb_rcvbuf[2];
                uint8_t status     = 0x01; /* default Fail */
                uint8_t data_cnt   = 0;

                /* NVMe-MI Management Request Dword0/Dword1 (little-endian).
                 * Parsed here so ported command implementations can consume them
                 * (e.g. VPD DOFST/DLEN, Config ID/Port, DTYP). Currently unused
                 * by the CommandID 1 path and the not-implemented skeleton. */
                uint32_t dword0 = (uint32_t)usb_rcvbuf[3]
                                | ((uint32_t)usb_rcvbuf[4] << 8)
                                | ((uint32_t)usb_rcvbuf[5] << 16)
                                | ((uint32_t)usb_rcvbuf[6] << 24);
                uint32_t dword1 = (uint32_t)usb_rcvbuf[7]
                                | ((uint32_t)usb_rcvbuf[8] << 8)
                                | ((uint32_t)usb_rcvbuf[9] << 16)
                                | ((uint32_t)usb_rcvbuf[10] << 24);

                for (i = 0; i < 1024; i++)
                {
                    response_buff[i] = 0;
                }

                response_buff[0] = 0xe0;
                response_buff[1] = cmd_id;

                if (cmd_id == 1)
                {
                    /* Basic Management - data cached in bmc_report[]. */
                    if (nvme_index < 24)
                    {
                        uint16_t base = (uint16_t)(NVME_MEM_OFFSET + (nvme_index * NVME_READ_COUNT));
                        data_cnt = NVME_READ_COUNT;

                        for (i = 0; i < data_cnt; i++)
                        {
                            response_buff[4 + i] = bmc_report[base + i];
                        }

                        status = 0x00; /* Success */
                    }
                    else
                    {
                        const char *reason = "NVMe index out of range (0..23)";
                        uint16_t k;

                        for (k = 0; reason[k] != '\0' && k < 200; k++)
                        {
                            response_buff[4 + k] = (uint8_t)reason[k];
                        }

                        status = 0x01; /* Fail */
                    }
                }
                else if (cmd_id == 4  || cmd_id == 5  || cmd_id == 6 ||
                         cmd_id == 7  || cmd_id == 10 || cmd_id == 11 ||
                         cmd_id == 12 || cmd_id == 15 || cmd_id == 16)
                {
                    /* Native single-slot NVMe-MI Command Set commands executed
                     * over MCTP/SMBus by nvme_mi_native_cmd() in nvme_mi.c.
                     * All use NMIMT=1h (NVMe-MI Command); only the opcode and
                     * the Management Request DWORD0/DWORD1 differ:
                     *   CmdID  4 : Read NVMe-MI Data Structure     (Opcode 0x00)
                     *              Dword0 [15:0]=CTRLID, [23:16]=PORTID, [31:24]=DTYP
                     *   CmdID  6 : Configuration Get               (Opcode 0x04)
                     *              Dword0 [7:0]=Config ID, [31:24]=Port ID
                     *   CmdID  7 : VPD Read                        (Opcode 0x05)
                     *              Dword0 [15:0]=DOFST, Dword1 [7:0]=DLEN
                     *   CmdID 10 : NVM Subsystem Health Status Poll(Opcode 0x01)
                     *              Dword1 bit31=Clear Status
                     *   CmdID 11 : Controller Health Status Poll   (Opcode 0x02)
                     *              Dword0 [15:0]=SCTLID, [23:16]=MAXRENT, [31:24]=flags
                     *   CmdID 12 : Health Info + Alerts            (Opcode 0x01)
                     *              Same NVM Subsystem Health Status Poll response;
                     *              carries Composite Controller Status (alerts) +
                     *              temperature + drive-life. Dword1 bit31=Clear
                     *   CmdID 15 : Health Alerts                    (Opcode 0x01)
                     *   CmdID 16 : Health Info                      (Opcode 0x01)
                     *              Both are NVM Subsystem Health Status Poll
                     *              variants (alert flags / health fields).
                     *   CmdID  5 : Configuration Set                (Opcode 0x03)
                     *              Dword0 [7:0]=Config ID, [31:8]=value.
                     *              WRITE: changes device config (host controls
                     *              the exact Config ID + value via Dword0/1). */
                    uint8_t  opcode;
                    uint8_t  mi_status = 0xFF;
                    uint16_t out_len   = 0;
                    uint32_t d0        = dword0;
                    uint32_t d1        = dword1;
                    int32_t  rc;

                    switch (cmd_id)
                    {
                        case 4:  opcode = 0x00; break; /* Read MI Data Structure */
                        case 10: opcode = 0x01; break; /* NVM Subsys Health Poll */
                        case 12: opcode = 0x01; break; /* Health Info + Alerts   */
                        case 15: opcode = 0x01; break; /* Health Alerts          */
                        case 16: opcode = 0x01; break; /* Health Info            */
                        case 11: opcode = 0x02; break; /* Ctrl Health Poll       */
                        case 5:  opcode = 0x03; break; /* Configuration Set      */
                        case 6:  opcode = 0x04; break; /* Configuration Get      */
                        default: opcode = 0x05; break; /* 7 = VPD Read           */
                    }

                    /* VPD Read: default DLEN to 0x20 (one MCTP packet) when the
                     * host does not specify Dword1, so a bare "NVMEMI 7 <idx>"
                     * still returns a chunk of VPD. */
                    if (cmd_id == 7 && d1 == 0)
                        d1 = 0x20;

                    /* Controller Health Status Poll: default to the reference
                     * request (MAXRENT=1, ALL controllers + all include flags,
                     * SCTLID=0 => Dword0 = 0x87010000) when the host passes 0,
                     * so a bare "NVMEMI 11 <idx>" returns one controller entry. */
                    if (cmd_id == 11 && d0 == 0)
                        d0 = 0x87010000;

                    rc = nvme_mi_native_cmd((uint8_t)nvme_index, opcode, d0, d1,
                                            (uint8_t *)&response_buff[4], 200, &out_len, &mi_status);

                    if (rc != 0)
                    {
                        char rbuf[64];
                        uint16_t k;

                        sprintf(rbuf, "NVMe-MI transport error (%d)", (int)rc);
                        for (i = 0; i < 1024; i++)
                            response_buff[i] = 0;
                        response_buff[0] = 0xe0;
                        response_buff[1] = cmd_id;
                        for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)rbuf[k];
                        status = 0x01; /* Fail */
                    }
                    else if (mi_status != 0x00)
                    {
                        char rbuf[64];
                        uint16_t k;

                        sprintf(rbuf, "NVMe-MI Status 0x%02X", mi_status);
                        for (i = 0; i < 1024; i++)
                            response_buff[i] = 0;
                        response_buff[0] = 0xe0;
                        response_buff[1] = cmd_id;
                        for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)rbuf[k];
                        status = 0x01; /* Fail */
                    }
                    else
                    {
                        data_cnt = (out_len > 200) ? 200 : (uint8_t)out_len;
                        status = 0x00; /* Success */
                    }
                }
                else if (cmd_id == 14 || cmd_id == 17 || cmd_id == 18)
                {
                    /* NVMe Admin passthrough commands over NVMe-MI (NMIMT=2h),
                     * executed by nvme_mi_admin_cmd() in nvme_mi.c:
                     *   CmdID 14 : FW Revision   - Get Log Page, FW Slot Info
                     *              (LID 0x03, 64B). FW rev string = data[8..15].
                     *   CmdID 17 : SMART Log     - Get Log Page, SMART / Health
                     *              (LID 0x02, 512B).
                     *   CmdID 18 : Device Identify - Identify Controller
                     *              (Opcode 0x06, CNS 0x01, 4096B).
                     * The response can exceed one USB frame, so Dword0 is used
                     * as a byte offset into the returned data for host paging;
                     * each call returns up to 200 bytes from that offset. */
                    uint8_t  opcode;
                    uint32_t nsid  = 0;
                    uint32_t cdw10 = 0;
                    uint32_t cdw11 = 0;
                    uint32_t dlen  = 0;
                    uint16_t out_off   = (uint16_t)dword0; /* paging byte offset */
                    uint8_t  mi_status = 0xFF;
                    uint16_t out_len   = 0;
                    uint16_t total_len = 0;
                    int32_t  rc;

                    switch (cmd_id)
                    {
                        case 14: /* Get Log Page - FW Slot Info (LID 0x03, 64B) */
                            opcode = 0x02;
                            nsid   = 0xFFFFFFFF;
                            dlen   = 64;
                            cdw10  = 0x000F8003; /* LID=0x03, RAE=1, NUMDL=0x000F (64B) */
                            break;
                        case 17: /* Get Log Page - SMART / Health (LID 0x02, 512B) */
                            opcode = 0x02;
                            nsid   = 0xFFFFFFFF;
                            dlen   = 512;
                            cdw10  = 0x007F8002; /* LID=0x02, RAE=1, NUMDL=0x007F (512B) */
                            break;
                        default: /* 18: Identify Controller (CNS 0x01, 4096B) */
                            opcode = 0x06;
                            nsid   = 0;
                            dlen   = 4096;
                            cdw10  = 0x00000001; /* CNS=0x01 (Identify Controller) */
                            break;
                    }

                    rc = nvme_mi_admin_cmd((uint8_t)nvme_index, opcode, nsid,
                                           cdw10, cdw11, dlen, out_off,
                                           (uint8_t *)&response_buff[4], 200,
                                           &out_len, &total_len, &mi_status);

                    if (rc != 0)
                    {
                        char rbuf[64];
                        uint16_t k;

                        sprintf(rbuf, "NVMe Admin transport error (%d)", (int)rc);
                        for (i = 0; i < 1024; i++)
                            response_buff[i] = 0;
                        response_buff[0] = 0xe0;
                        response_buff[1] = cmd_id;
                        for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)rbuf[k];
                        status = 0x01; /* Fail */
                    }
                    else if (mi_status != 0x00)
                    {
                        char rbuf[64];
                        uint16_t k;

                        sprintf(rbuf, "NVMe Admin Status 0x%02X", mi_status);
                        for (i = 0; i < 1024; i++)
                            response_buff[i] = 0;
                        response_buff[0] = 0xe0;
                        response_buff[1] = cmd_id;
                        for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)rbuf[k];
                        status = 0x01; /* Fail */
                    }
                    else
                    {
                        data_cnt = (out_len > 200) ? 200 : (uint8_t)out_len;
                        status = 0x00; /* Success */
                    }
                }
                else if (cmd_id == 2 || cmd_id == 3)
                {
                    /* MCTP Control commands (message type 0x00, no MIC),
                     * executed by nvme_mi_mctp_ctrl_cmd() in nvme_mi.c:
                     *   CmdID 2 : Get Endpoint UUID        (Cmd Code 0x03)
                     *             Response = 16-byte RFC4122 UUID.
                     *   CmdID 3 : Get Message Type Support (Cmd Code 0x05)
                     *             Response = type count + supported MCTP
                     *             message types (e.g. 00/04/05/06). */
                    uint8_t  cmd_code  = (cmd_id == 2) ? 0x03 : 0x05;
                    uint8_t  comp_code = 0xFF;
                    uint16_t out_len   = 0;
                    int32_t  rc;

                    rc = nvme_mi_mctp_ctrl_cmd((uint8_t)nvme_index, cmd_code,
                                               (uint8_t *)&response_buff[4], 200,
                                               &out_len, &comp_code);

                    if (rc != 0)
                    {
                        char rbuf[64];
                        uint16_t k;

                        sprintf(rbuf, "MCTP Control transport error (%d)", (int)rc);
                        for (i = 0; i < 1024; i++)
                            response_buff[i] = 0;
                        response_buff[0] = 0xe0;
                        response_buff[1] = cmd_id;
                        for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)rbuf[k];
                        status = 0x01; /* Fail */
                    }
                    else if (comp_code != 0x00)
                    {
                        char rbuf[64];
                        uint16_t k;

                        sprintf(rbuf, "MCTP Control Completion 0x%02X", comp_code);
                        for (i = 0; i < 1024; i++)
                            response_buff[i] = 0;
                        response_buff[0] = 0xe0;
                        response_buff[1] = cmd_id;
                        for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)rbuf[k];
                        status = 0x01; /* Fail */
                    }
                    else
                    {
                        data_cnt = (out_len > 200) ? 200 : (uint8_t)out_len;
                        status = 0x00; /* Success */
                    }
                }
                else if (cmd_id == 13)
                {
                    /* Power Management - NVMe Admin Get Features (opcode 0x0A,
                     * FID 0x02, SEL 0 = current), executed by
                     * nvme_mi_admin_get_feature() in nvme_mi.c. Get Features
                     * transfers no data; the value is returned in the CQE
                     * DWORD0: Power State in bits [4:0], Workload Hint in
                     * bits [7:5]. The 4-byte DWORD0 (little-endian) is returned
                     * to the host as the response data. */
                    uint32_t dw0       = 0;
                    uint8_t  mi_status = 0xFF;
                    uint8_t  sc        = 0xFF;
                    int32_t  rc;

                    rc = nvme_mi_admin_get_feature((uint8_t)nvme_index, 0x02, 0x00,
                                                   &dw0, &mi_status, &sc);

                    if (rc != 0)
                    {
                        char rbuf[64];
                        uint16_t k;

                        sprintf(rbuf, "NVMe Admin transport error (%d)", (int)rc);
                        for (i = 0; i < 1024; i++)
                            response_buff[i] = 0;
                        response_buff[0] = 0xe0;
                        response_buff[1] = cmd_id;
                        for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)rbuf[k];
                        status = 0x01; /* Fail */
                    }
                    else if (mi_status != 0x00)
                    {
                        char rbuf[64];
                        uint16_t k;

                        sprintf(rbuf, "NVMe Admin Status 0x%02X", mi_status);
                        for (i = 0; i < 1024; i++)
                            response_buff[i] = 0;
                        response_buff[0] = 0xe0;
                        response_buff[1] = cmd_id;
                        for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)rbuf[k];
                        status = 0x01; /* Fail */
                    }
                    else if (sc != 0x00)
                    {
                        char rbuf[64];
                        uint16_t k;

                        sprintf(rbuf, "NVMe Status 0x%02X", sc);
                        for (i = 0; i < 1024; i++)
                            response_buff[i] = 0;
                        response_buff[0] = 0xe0;
                        response_buff[1] = cmd_id;
                        for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)rbuf[k];
                        status = 0x01; /* Fail */
                    }
                    else
                    {
                        response_buff[4] = (uint8_t)(dw0 & 0xFF);
                        response_buff[5] = (uint8_t)((dw0 >> 8) & 0xFF);
                        response_buff[6] = (uint8_t)((dw0 >> 16) & 0xFF);
                        response_buff[7] = (uint8_t)((dw0 >> 24) & 0xFF);
                        data_cnt = 4;
                        status = 0x00; /* Success */
                    }
                }
                else if (cmd_id == 8)
                {
                    /* VPD Write - native NVMe-MI Command Set write (Opcode 0x06).
                     *   Dword0 [15:0] = DOFST (write offset)
                     *   Dword1 [7:0]  = DLEN  (number of data bytes)
                     * The host carries the write payload in the USB request
                     * after the two DWORDs: usb_rcvbuf[11] = data byte count,
                     * usb_rcvbuf[12..] = the data bytes. DLEN is taken from the
                     * carried byte count so the wire DLEN always matches the
                     * payload actually appended. WRITE: modifies the drive's
                     * VPD/FRU contents. */
                    uint8_t  wr_data[180];
                    uint8_t  wr_len    = usb_rcvbuf[11];
                    uint16_t dofst     = (uint16_t)(dword0 & 0xFFFF);
                    uint8_t  out_buf[8];
                    uint16_t out_len   = 0;
                    uint8_t  mi_status = 0xFF;
                    int32_t  rc;
                    uint16_t k;

                    if (wr_len > sizeof(wr_data))
                        wr_len = sizeof(wr_data);
                    for (k = 0; k < wr_len; k++)
                        wr_data[k] = usb_rcvbuf[12 + k];

                    rc = nvme_mi_native_write_cmd((uint8_t)nvme_index, 0x06,
                                                  (uint32_t)dofst, (uint32_t)wr_len,
                                                  wr_data, wr_len,
                                                  out_buf, (uint16_t)sizeof(out_buf),
                                                  &out_len, &mi_status);

                    if (rc != 0)
                    {
                        char rbuf[64];

                        sprintf(rbuf, "NVMe-MI transport error (%d)", (int)rc);
                        for (i = 0; i < 1024; i++)
                            response_buff[i] = 0;
                        response_buff[0] = 0xe0;
                        response_buff[1] = cmd_id;
                        for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)rbuf[k];
                        status = 0x01; /* Fail */
                    }
                    else if (mi_status != 0x00)
                    {
                        char rbuf[64];

                        sprintf(rbuf, "NVMe-MI Status 0x%02X", mi_status);
                        for (i = 0; i < 1024; i++)
                            response_buff[i] = 0;
                        response_buff[0] = 0xe0;
                        response_buff[1] = cmd_id;
                        for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)rbuf[k];
                        status = 0x01; /* Fail */
                    }
                    else
                    {
                        /* Success: echo any response payload (usually empty). */
                        for (k = 0; k < out_len && k < 200; k++)
                            response_buff[4 + k] = out_buf[k];
                        data_cnt = (uint8_t)out_len;
                        status = 0x00; /* Success */
                    }
                }
                else if (cmd_id == 19)
                {
                    /* Format NVM - NVMe Admin Format NVM (opcode 0x80).
                     * !!! DESTRUCTIVE: erases all user data on the drive. !!!
                     *   Dword0 = CDW10 (LBAF/MSET/PI/PIL/SES; 0 = default)
                     *   Dword1 = safety confirmation, MUST equal 0x464D5421
                     *            ("FMT!") or the command is refused. This guards
                     *            against accidental erasure from a stray call.
                     * NSID is fixed to 0xFFFFFFFF (all namespaces). The result
                     * is a CQE; SC/SCT are decoded by nvme_mi_admin_format_nvm(). */
                    const uint32_t FORMAT_CONFIRM = 0x464D5421U; /* "FMT!" */

                    if (dword1 != FORMAT_CONFIRM)
                    {
                        const char *reason =
                            "Format NVM refused: set Dword1=0x464D5421 to confirm erase";
                        uint16_t k;

                        for (k = 0; reason[k] != '\0' && k < 200; k++)
                            response_buff[4 + k] = (uint8_t)reason[k];
                        status = 0x01; /* Fail */
                    }
                    else
                    {
                        uint8_t  mi_status = 0xFF;
                        uint8_t  sc        = 0xFF;
                        int32_t  rc;

                        rc = nvme_mi_admin_format_nvm((uint8_t)nvme_index, 0xFFFFFFFFU,
                                                      dword0, &mi_status, &sc);

                        if (rc == -2)
                        {
                            /* The request was sent, but the drive did not send any
                             * MCTP reply within the full retry budget. This is
                             * expected when CDW10 requests a real data erase
                             * (SES=1/2): the controller can stay busy erasing for
                             * much longer than a quick metadata-only reformat
                             * (SES=0), so it never gets a chance to ACK before we
                             * give up waiting. Report this distinctly (not a hard
                             * failure) so the host can poll for completion instead
                             * of assuming the drive rejected the command. */
                            char rbuf[160];
                            uint16_t k;

                            sprintf(rbuf,
                                   "NVMe Admin transport error (-2): drive did not "
                                   "reply within the format retry budget; the erase "
                                   "may still be running in the background");
                            for (i = 0; i < 1024; i++)
                                response_buff[i] = 0;
                            response_buff[0] = 0xe0;
                            response_buff[1] = cmd_id;
                            for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                                response_buff[4 + k] = (uint8_t)rbuf[k];
                            status = 0x01; /* Fail (host reinterprets as InProgress) */
                        }
                        else if (rc != 0)
                        {
                            char rbuf[64];
                            uint16_t k;

                            sprintf(rbuf, "NVMe Admin transport error (%d)", (int)rc);
                            for (i = 0; i < 1024; i++)
                                response_buff[i] = 0;
                            response_buff[0] = 0xe0;
                            response_buff[1] = cmd_id;
                            for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                                response_buff[4 + k] = (uint8_t)rbuf[k];
                            status = 0x01; /* Fail */
                        }
                        else if (mi_status != 0x00)
                        {
                            char rbuf[64];
                            uint16_t k;

                            sprintf(rbuf, "NVMe Admin Status 0x%02X", mi_status);
                            for (i = 0; i < 1024; i++)
                                response_buff[i] = 0;
                            response_buff[0] = 0xe0;
                            response_buff[1] = cmd_id;
                            for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                                response_buff[4 + k] = (uint8_t)rbuf[k];
                            status = 0x01; /* Fail */
                        }
                        else if (sc != 0x00)
                        {
                            char rbuf[64];
                            uint16_t k;

                            sprintf(rbuf, "NVMe Status 0x%02X", sc);
                            for (i = 0; i < 1024; i++)
                                response_buff[i] = 0;
                            response_buff[0] = 0xe0;
                            response_buff[1] = cmd_id;
                            for (k = 0; rbuf[k] != '\0' && k < 200; k++)
                                response_buff[4 + k] = (uint8_t)rbuf[k];
                            status = 0x01; /* Fail */
                        }
                        else
                        {
                            /* Success: report the SC (0) as a single status byte. */
                            response_buff[4] = sc;
                            data_cnt = 1;
                            status = 0x00; /* Success */
                        }
                    }
                }
                else if (cmd_id >= 2 && cmd_id <= 19)
                {
                    const char *reason =
                        "NVMe-MI command not implemented in firmware (see main_mctp_test.c)";
                    uint16_t k;

                    for (k = 0; reason[k] != '\0' && k < 200; k++)
                    {
                        response_buff[4 + k] = (uint8_t)reason[k];
                    }

                    status = 0x01; /* Fail */
                }
                else
                {
                    const char *reason = "Invalid CommandID (must be 1..19)";
                    uint16_t k;

                    for (k = 0; reason[k] != '\0' && k < 200; k++)
                    {
                        response_buff[4 + k] = (uint8_t)reason[k];
                    }

                    status = 0x01; /* Fail */
                }

                response_buff[2] = status;
                response_buff[3] = data_cnt;

                for (i = 0; i < 1024; i++)
                {
                    HSUSBD->EP[EPA].EPDAT_BYTE = response_buff[i];
                }

                HSUSBD->EP[EPA].EPTXCNT = 1024;
                HSUSBD_ENABLE_EP_INT(EPA, HSUSBD_EPINTEN_INTKIEN_Msk);
            }

#if 0

            if (i2c_monitor_flag == 1)
            {
                CPLD_read_AFTER();
            }

#endif

            // Clear flags and buffers for the next command.
            string_received = 0;
            usb_rcvbuf[0] = 0x0; //clear command
            // Re-enable interrupts.
            __set_PRIMASK(0);
        }

    }

}
