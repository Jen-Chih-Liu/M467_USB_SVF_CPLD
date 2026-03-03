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
volatile uint8_t bmc_report[1024] __attribute__((aligned(4))) = {0};
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

volatile unsigned char i2c_monitor_flag = 1;
volatile unsigned char reset_var = 0xa5;
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
    SET_I2C0_SDA_PB4();
    SET_I2C0_SCL_PB5();
    PB->SMTEN |= GPIO_SMTEN_SMTEN4_Msk | GPIO_SMTEN_SMTEN5_Msk ;

    SET_I2C1_SDA_PB0();
    SET_I2C1_SCL_PB1();
    PB->SMTEN |= GPIO_SMTEN_SMTEN0_Msk | GPIO_SMTEN_SMTEN1_Msk ;



    SET_I2C4_SDA_PF4();
    SET_I2C4_SCL_PF5();
    PF->SMTEN |= GPIO_SMTEN_SMTEN4_Msk | GPIO_SMTEN_SMTEN5_Msk ;


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


void I2C4_Init(void)
{

    /* Open I2C0 and set clock to 100k */
    I2C_Open(I2C4, 100000);

    /* Get I2C0 Bus Clock */
    printf("I2C clock %d Hz\n", I2C_GetBusClockFreq(I2C4));
	
	
		I2C_SetSlaveAddr(I2C4, 0, 0x44, I2C_GCMODE_DISABLE);   /* Slave Address : 0x15 */
    I2C_SetSlaveAddr(I2C4, 1, 0x46, I2C_GCMODE_DISABLE);   /* Slave Address : 0x35 */
    
    I2C_EnableInt(I2C4);
    NVIC_EnableIRQ(I2C4_IRQn);
}
typedef void (*I2C_FUNC)(uint32_t u32Status);
static I2C_FUNC s_I2C4HandlerFn = NULL;




void I2C4_IRQHandler(void)
{
    uint32_t u32Status;

    u32Status = I2C_GET_STATUS(I2C4);

    if(I2C_GET_TIMEOUT_FLAG(I2C4))
    {
        /* Clear I2C4 Timeout Flag */
        I2C_ClearTimeoutFlag(I2C4);
        //g_u8TimeoutFlag = 1;
    }
    else
    {
        if(s_I2C4HandlerFn != NULL)
            s_I2C4HandlerFn(u32Status);
    }
}

volatile unsigned char fan_address_index=0;
volatile unsigned char fan_reg_index=0;
volatile unsigned char fan_address_0x44[0xff]={0};
volatile unsigned char fan_address_0x46[0xff]={0};
volatile unsigned char g_au8SlvData[32]={0};
volatile uint8_t g_u8SlvDataLen = 0;
volatile uint8_t g_u8SlvTRxAbortFlag = 0;
void I2C_SlaveTRx(uint32_t u32Status)
{
    uint8_t u8data;
    if(u32Status == 0x60)                       /* Own SLA+W has been receive; ACK has been return */
    {
        g_u8SlvDataLen = 0;
        fan_address_index=I2C_GET_DATA(I2C4)>>1;
        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if(u32Status == 0x80)                 /* Previously address with own SLA address
                                                   Data has been received; ACK has been returned*/
    {
        u8data = (unsigned char) I2C_GET_DATA(I2C4);

        g_au8SlvData[g_u8SlvDataLen++] = u8data;
                
        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if(u32Status == 0xA8)                  /* Own SLA+R has been receive; ACK has been return */
    {
        fan_address_index=I2C_GET_DATA(I2C4)>>1;
        if (fan_address_index==(0x44>>1))
        {           
            I2C_SET_DATA(I2C4, fan_address_0x44[fan_reg_index]);        
        } 
        else if (fan_address_index==(0x46>>1))
        {
            I2C_SET_DATA(I2C4, fan_address_0x46[fan_reg_index]);
        }
        fan_reg_index++;
        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if(u32Status == 0xB8)                  /* Data byte in I2CDAT has been transmitted ACK has been received */
    {
         if (fan_address_index==(0x44>>1))
        {           
            I2C_SET_DATA(I2C4, fan_address_0x44[fan_reg_index]);        
        } 
        else if (fan_address_index==(0x46>>1))
        {
            I2C_SET_DATA(I2C4, fan_address_0x46[fan_reg_index]);
        }
        fan_reg_index++;
        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI_AA);
    }
    else if(u32Status == 0xC0)                 /* Data byte or last data in I2CDAT has been transmitted
                                                   Not ACK has been received */
    {
        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if(u32Status == 0x88)                 /* Previously addressed with own SLA address; NOT ACK has
                                                   been returned */
    {
        g_u8SlvDataLen = 0;
        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else if(u32Status == 0xA0)                 /* A STOP or repeated START has been received while still
                                             addressed as Slave/Receiver*/
    {
        if (g_u8SlvDataLen==3)
        {
            if (fan_address_index==(0x44>>1))
            {
                fan_address_0x44[g_au8SlvData[0]]=g_au8SlvData[1];
                fan_address_0x44[g_au8SlvData[0]+1]=g_au8SlvData[2];
            }
            else if (fan_address_index==(0x46>>1))
            {
                fan_address_0x46[g_au8SlvData[0]]=g_au8SlvData[1];
                fan_address_0x46[g_au8SlvData[0]+1]=g_au8SlvData[2];
            }   
        }
            else if (g_u8SlvDataLen==1)
            {
                fan_reg_index = g_au8SlvData[0];
            }
        
        g_u8SlvDataLen = 0;
        I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
    }
    else
    {
        printf("[SlaveTRx] Status [0x%x] Unexpected abort!!\n", u32Status);
        if(u32Status == 0x68)               /* Slave receive arbitration lost, clear SI */
        {
            I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI_AA);
        }
        else if(u32Status == 0xB0)          /* Address transmit arbitration lost, clear SI  */
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
 * @brief  I2C1 Interface Initialization.
 * @param  None
 * @return None
 */
void I2C1_Init(void)
{

    /* Open I2C0 and set clock to 100k */
    I2C_Open(I2C1, 100000);

    /* Get I2C0 Bus Clock */
    printf("I2C clock %d Hz\n", I2C_GetBusClockFreq(I2C1));

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
    }
}


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

    printf("\n\nCPU @ %dHz\n", SystemCoreClock);
    printf("inital scan:%d\n\r", xsvftool_esp_scan());
    printf("jtag id 0x%x\n\r", xsvftool_esp_id());
    // Store the scanned CPLD JTAG ID into the bmc_report buffer.
    bmc_report[cpld_jtag_id] = (xsvftool_esp_id() >> 24) & 0xff;
    bmc_report[cpld_jtag_id + 1] = (xsvftool_esp_id() >> 16) & 0xff;
    bmc_report[cpld_jtag_id + 2] = (xsvftool_esp_id() >> 8) & 0xff;
    bmc_report[cpld_jtag_id + 3] = (xsvftool_esp_id() >> 0) & 0xff;

    I2C0_Init();
    I2C1_Init();

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
    // Initialize the fan controller.
    fan_inital();
    FanIC_Backup_init();
    I2C4_Init();
    s_I2C4HandlerFn = I2C_SlaveTRx;
    I2C_SET_CONTROL_REG(I2C4, I2C_CTL_SI | I2C_CTL_AA);
        
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
                FanIC_BackupRegisters();
                FanIC_CompareAndRestore();
                CPLD_read();          // Read CPLD status.
                //fan_read();           // Read fan speed and duty cycle.
                tempersensor_read();  // Read temperature sensor.
                nvm_mi_read();        // Read NVMe drive information.
            }


#endif
            timer0_count = 0;
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
                // The hardware supports up to 16 drives. Stop if index goes beyond.
                if (nvme_i >= 16) break;

                print_nvme_basic_management_info(&bmc_report[NVME_MEM_OFFSET + (nvme_i * NVME_READ_COUNT)]);
            }

#endif
            g_u8DumpLogFlag = 0;
        }

        // Process incoming USB commands.
        if (string_received == TRUE)
        {
            // Disable interrupts to process the command atomically.
            __set_PRIMASK(1);

            // --- SVF Command Parser ---
            // Command 0xa1: Process an SVF command packet.
            if (usb_rcvbuf[0] == 0xa1)
            {
#if 1
                uint16_t terminator_pos = 0;

                // Copy the SVF command string from the USB buffer until a ';' is found.
                for (terminator_pos = 0; terminator_pos < 1023; terminator_pos++)
                {
                    svf_string_rcvbuf[terminator_pos] =  usb_rcvbuf[terminator_pos + 1];

                    if (svf_string_rcvbuf[terminator_pos] == ';')
                    {
                        buffer_index = terminator_pos;
                        break;
                    }

                }

                svf_string_rcvbuf[buffer_index + 1] = 0x00;
#endif

                pos = 0;
                // Call the SVF player function to execute the command packet.
                retval = xsvftool_esp_svf_packet(getbyte_usb_fun, total_line /*??*/, final /*0 or 1*/, report);

                // If the SVF player returns an error and it's the first error, record the line number.
                if ((retval != 0) && (cpld_false_flag == 0))
                {
                    response_buff[0] = (total_line) & 0xff;
                    response_buff[1] = (total_line >> 8) & 0xff;
                    response_buff[2] = (total_line >> 16) & 0xff;
                    response_buff[3] = (total_line >> 24) & 0xff;
                    cpld_false_flag = 1;
                }

                memset(svf_string_rcvbuf, 0x0, buffer_index); // Clear the processed command.
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
                cpld_false_flag = 0;
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
                response_buff[1] = 0x01;
                response_buff[2] = 0x14;
                response_buff[3] = 0x02;

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

            // Command 0xd0: I2C write.
            if (usb_rcvbuf[0] == 0xd0)
            {
                if (usb_rcvbuf[1] == 0)
                    i2c_write_bytes = I2C_WriteMultiBytes(I2C0, usb_rcvbuf[2] >> 1, &usb_rcvbuf[4], usb_rcvbuf[3]);

                if (usb_rcvbuf[1] == 1)
                    i2c_write_bytes = I2C_WriteMultiBytes(I2C1, usb_rcvbuf[2] >> 1, &usb_rcvbuf[4], usb_rcvbuf[3]);
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
                    i2c_read_bytes = I2C_ReadMultiBytes(I2C1, usb_rcvbuf[2] >> 1, i2c_read_report, usb_rcvbuf[3]);
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
                    i2c_write_bytes = I2C_WriteMultiBytes(I2C1, usb_rcvbuf[2] >> 1, &usb_rcvbuf[5], usb_rcvbuf[3]);
                    i2c_read_bytes = I2C_ReadMultiBytes(I2C1, usb_rcvbuf[2] >> 1, i2c_read_report, usb_rcvbuf[4]);
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

            // Clear flags and buffers for the next command.
            string_received = 0;
            usb_rcvbuf[0] = 0x0; //clear command
            // Re-enable interrupts.
            __set_PRIMASK(0);


        }
    }

}
