/***************************************************************************//**
 * @file     main.c
 * @brief    ISP tool main function
 * @version  0x32
 *
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @copyright Copyright (C) 2021 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "NuMicro.h"
#include "hid_transfer.h"
#include "targetdev.h"

#define PLL_CLOCK   200000000
#define ap_rom_size 256*1024
#define g_ckbase  (ap_rom_size - 8)

volatile uint32_t totallen = 0, cksum = 0, code_check_flash = 0;
__attribute__((aligned(4)))  uint8_t aprom_buf[FMC_FLASH_PAGE_SIZE];
int32_t g_FMC_i32ErrCode = 0;

void ProcessHardFault(void);
void SH_Return(void);
void SendChar_ToUART(void);
int32_t SYS_Init(void);

void ProcessHardFault(void) {}
void SH_Return(void) {}
void SendChar_ToUART(void) {}

uint32_t CLK_GetPLLClockFreq(void)
{
    return PLL_CLOCK;
}


static uint16_t Checksum(unsigned char *buf, int len)
{
    int i;
    uint16_t c;

    for (c = 0, i = 0; i < len; i++)
    {
        c += buf[i];
    }

    return (c);
}

static uint16_t CalCheckSum(uint32_t start, uint32_t len)
{
    int i;
    uint16_t lcksum = 0;

    for (i = 0; i < len; i += FMC_FLASH_PAGE_SIZE)
    {
        ReadData(start + i, start + i + FMC_FLASH_PAGE_SIZE, (uint32_t *)aprom_buf);

        if (len - i >= FMC_FLASH_PAGE_SIZE)
            lcksum += Checksum(aprom_buf, FMC_FLASH_PAGE_SIZE);
        else
            lcksum += Checksum(aprom_buf, len - i);
    }

    return lcksum;

}

int32_t SYS_Init(void)
{
    uint32_t volatile i;
    uint32_t u32TimeOutCnt;

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Enable HIRC and HXT clock */
    CLK->PWRCTL |= CLK_PWRCTL_HIRCEN_Msk | CLK_PWRCTL_HXTEN_Msk;

    /* Wait for HIRC and HXT clock ready */
    u32TimeOutCnt = SystemCoreClock; /* 1 second time-out */

    while (!(CLK->STATUS & (CLK_STATUS_HIRCSTB_Msk | CLK_STATUS_HXTSTB_Msk)))
    {
        if (--u32TimeOutCnt == 0)
            return -1;
    }

    /* Set HCLK clock source as HIRC first */
    CLK->CLKSEL0 = (CLK->CLKSEL0 & (~CLK_CLKSEL0_HCLKSEL_Msk)) | CLK_CLKSEL0_HCLKSEL_HIRC;

    /* Disable PLL clock before setting PLL frequency */
    CLK->PLLCTL |= CLK_PLLCTL_PD_Msk;

    /* Set PLL clock as 200MHz from HIRC */
    CLK->PLLCTL = CLK_PLLCTL_200MHz_HIRC;

    /* Wait for PLL clock ready */
    u32TimeOutCnt = SystemCoreClock; /* 1 second time-out */

    while (!(CLK->STATUS & CLK_STATUS_PLLSTB_Msk))
    {
        if (--u32TimeOutCnt == 0)
            return -1;
    }

    /* Set power level by HCLK working frequency */
    SYS->PLCTL = (SYS->PLCTL & (~SYS_PLCTL_PLSEL_Msk)) | SYS_PLCTL_PLSEL_PL0;

    /* Set flash access cycle by HCLK working frequency */
    FMC->CYCCTL = (FMC->CYCCTL & (~FMC_CYCCTL_CYCLE_Msk)) | (8);

    /* Set PCLK0 and PCLK1 to HCLK/2 */
    CLK->PCLKDIV = (CLK_PCLKDIV_APB0DIV_DIV2 | CLK_PCLKDIV_APB1DIV_DIV2);

    /* Select HCLK clock source as PLL and HCLK source divider as 1 */
    CLK->CLKDIV0 = (CLK->CLKDIV0 & (~CLK_CLKDIV0_HCLKDIV_Msk)) | CLK_CLKDIV0_HCLK(1);
    CLK->CLKSEL0 = (CLK->CLKSEL0 & (~CLK_CLKSEL0_HCLKSEL_Msk)) | CLK_CLKSEL0_HCLKSEL_PLL;

    /* Update System Core Clock */
    PllClock        = 200000000;
    SystemCoreClock = 200000000;
    CyclesPerUs     = SystemCoreClock / 1000000;  /* For CLK_SysTickDelay() */

    /* Select HSUSBD */
    SYS->USBPHY &= ~SYS_USBPHY_HSUSBROLE_Msk;

    /* Enable USB PHY */
    SYS->USBPHY = (SYS->USBPHY & ~(SYS_USBPHY_HSUSBROLE_Msk | SYS_USBPHY_HSUSBACT_Msk)) | SYS_USBPHY_HSUSBEN_Msk;

    for (i = 0; i < 0x1000; i++);  // delay > 10 us

    SYS->USBPHY |= SYS_USBPHY_HSUSBACT_Msk;

    /* Enable HSUSBD module clock */
    CLK->AHBCLK0 |= CLK_AHBCLK0_HSUSBDCKEN_Msk;

    /* Enable GPIO B clock */
    CLK->AHBCLK0 |= CLK_AHBCLK0_GPBCKEN_Msk;

    return 0;
}
// ?? USB ??????????
// Header (2) + 96-bit UID ? Hex (24 chars * 2 bytes/char) = 50 bytes
#define USB_SERIAL_STR_LEN  50

// ????:???????? USB ?? (???? RAM ?,??? 4-byte aligned ??????)
uint8_t g_u8StringSerial[USB_SERIAL_STR_LEN] __attribute__((aligned(4))) = {
    USB_SERIAL_STR_LEN, // bLength
    0x03                // bDescriptorType (STRING)
    // ????????????
};
__STATIC_INLINE uint32_t FMC_ReadUID_J(uint8_t u8Index)
{
    int32_t i32TimeOutCnt = FMC_TIMEOUT_READ;

    g_FMC_i32ErrCode = 0;

    FMC->ISPCMD = FMC_ISPCMD_READ_UID;
    FMC->ISPADDR = ((uint32_t)u8Index << 2u);
    FMC->ISPDAT = 0u;
    FMC->ISPTRG = 0x1u;
    while(FMC->ISPTRG & FMC_ISPTRG_ISPGO_Msk)   /* Waiting for ISP Done */
    {
        if( i32TimeOutCnt-- <= 0)
        {
            g_FMC_i32ErrCode = -1;
            return 0xFFFFFFFF;
        }
    }

    return FMC->ISPDAT;
}
void Set_USB_SerialNumber_From_UID(void)
{
    uint32_t u32UID[3];
    uint32_t i, j;
    uint8_t *pDesc = &g_u8StringSerial[2]; // ?? Header,?????????
    uint8_t u8Nibble;
    char cHex;

    // 1. ?? Nuvoton M031 ? Unique ID
    // ??:??? SYS ????????????? (UID ???????)
	    for (i = 0; i < 3; i++)
    {
    u32UID[i] =FMC_ReadUID_J(i);

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
}
/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function                                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
int main(void)
{
    /* Unlock write-protected registers */
    SYS_UnlockReg();

    /* Init System, peripheral clock and multi-function I/O */
    if (SYS_Init() < 0)
    {
        goto _APROM;
    }

    /* Enable ISP */
    CLK->AHBCLK0 |= CLK_AHBCLK0_ISPCKEN_Msk;
    FMC->ISPCTL |= FMC_ISPCTL_ISPEN_Msk | FMC_ISPCTL_APUEN_Msk | FMC_ISPCTL_ISPFF_Msk;

		Set_USB_SerialNumber_From_UID();
		
    /* Get APROM and Data Flash size */
    g_u32ApromSize = ap_rom_size;
    FMC_Read_User(g_ckbase, &totallen);
    FMC_Read_User(g_ckbase + 4, &cksum);

    //GetDataFlashInfo(&g_u32DataFlashAddr, &g_u32DataFlashSize);
#if 0

    if (DetectPin != 0)
    {
        goto _APROM;
    }

#endif
    /* Open HSUSBD controller */
    HSUSBD_Open(NULL, NULL, NULL);

    /* Endpoint configuration */
    HID_Init();

    /* Enable HSUSBD interrupt */
    // NVIC_EnableIRQ(USBD20_IRQn);

    /* Start transaction */
    HSUSBD->OPER = HSUSBD_OPER_HISPDEN_Msk;   /* high-speed */
    HSUSBD_CLR_SE0();

#if 1

    if ((inpw(&SYS->RSTSTS) & 0x3) == 0x00) //check reset flag and por flag is clear
    {
        goto _LDROM_LOOP;
    }

#endif
#if 0

    if ((SYS->RSTSTS & SYS_RSTSTS_CPURF_Msk) == SYS_RSTSTS_CPURF_Msk)
    {
        SYS->RSTSTS &= SYS->RSTSTS;
        goto _LDROM_LOOP;
    }

#endif

    if (totallen > g_u32ApromSize)
    {
        goto _LDROM_LOOP;
    }

    code_check_flash = CalCheckSum(0x0, totallen);

    if (code_check_flash == (cksum & 0xffff))
    {
        goto _APROM;
    }

_LDROM_LOOP:

    while (1)
    {
        /* Polling HSUSBD interrupt flag */
        USBD20_IRQHandler();

        if (g_u8UsbDataReady == TRUE)
        {
            ParseCmd((uint8_t *)g_u8UsbRcvBuff, 64);
            EPA_Handler();
            g_u8UsbDataReady = FALSE;
        }
    }

_APROM:
    /* Reset system and boot from APROM */
    FMC_SetVectorPageAddr(FMC_APROM_BASE);
    NVIC_SystemReset();

    /* Trap the CPU */
    while (1);
}
