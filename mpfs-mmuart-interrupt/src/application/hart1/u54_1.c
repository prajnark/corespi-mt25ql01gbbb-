/*******************************************************************************
 * Copyright 2019-2022 Microchip FPGA Embedded Systems Solutions.
 *
 * SPDX-License-Identifier: MIT
 *
 * Application code running on U54_1
 *
 * Example project demonstrating the use of polled and local interrupt driven
 * transmission and reception over MMUART. Please refer README.md in the root
 * folder of this example project
 */

#include <stdio.h>
#include <string.h>
#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"
#include "inc/common.h"

#include "drivers/mss/CoreSPI/core_spi.h"
#include "drivers/mss/mt25ql01gbbb/mt25ql01gbbb.h"

#define BUFFER_A_SIZE   3000

#define FLASH_MANUFACTURER_ID   (uint8_t)0x20
#define FLASH_DEVICE_ID         (uint8_t)0xBB


/***************************************************************************//**
 * Soft-processor clock definition
 * This is the only clock brought over from the Mi-V Soft processor Libero design.
 */

#define SYS_CLK_FREQ                    50000000UL
#define NO_PARITY       0x00
#define DATA_8_BITS     0x01u
#define BAUD_VALUE_115200               ((SYS_CLK_FREQ / (16 * 115200)) - 1)


spi_instance_t g_spi0;


static uint8_t g_flash_wr_buf[BUFFER_A_SIZE];
static uint8_t g_flash_rd_buf[BUFFER_A_SIZE];

/******************************************************************************
 * Instruction message. This message will be transmitted over the UART when
 * the program starts.
 *****************************************************************************/

const uint8_t g_message1[] =
        "\r\n\r\n\r\n **** PolarFire SoC MSS MMUART example ****\r\n\r\n\r\n";

const uint8_t g_message55[] =
"\r\n\r\n\
******************* This is my print CoreSPI Example Project ********************************\r\n\
\r\n\r\n\
This example project demonstrates the use of the CoreSPI driver\r\n\
with Mi-V soft processor.\r\n";

#define RX_BUFF_SIZE    16U
uint8_t g_rx_buff1[RX_BUFF_SIZE] = { 0 };
volatile uint32_t count_sw_ints_h1 = 0U;
volatile uint8_t g_rx_size1 = 0U;
static volatile uint32_t irq_cnt = 0;
uint8_t info_string1[100];

/* This is the handler function for the UART RX interrupt.
 * In this example project UART1 local interrupt is enabled on hart1.
 */
void uart1_rx_handler(mss_uart_instance_t *this_uart) 
{
    uint32_t hart_id = read_csr(mhartid);
    int8_t info_string1[50];

    irq_cnt++;
    sprintf(info_string1, "UART1 Interrupt count = 0x%x \r\n\r\n", irq_cnt);

    /* This will execute when interrupt from hart 1 is raised */
    g_rx_size1 = MSS_UART_get_rx(this_uart, g_rx_buff1, sizeof(g_rx_buff1));

    MSS_UART_polled_tx(&g_mss_uart1_lo, info_string1,
            strlen(info_string1));
}

/* Main function for the hart1(U54 processor).
 * Application code running on hart1 is placed here.
 * MMUART1 local interrupt is enabled on hart1.
 * In the respective U54 harts, local interrupts of the corresponding MMUART
 * are enabled. e.g. in U54_1.c local interrupt of MMUART1 is enabled. */

/***************************************************************************//**
 * Compare the data of write buffer and read buffer
 */
static uint8_t verify_write(uint8_t* write_buff, uint8_t* read_buff, uint16_t size)
{
    uint8_t error = 0;
    uint16_t index = 0;

    while(size != 0)
    {
        if(write_buff[index] != read_buff[index])
        {
            error = 1;
            break;
        }
        index++;
        size--;
    }

    return error;
}


void u54_1(void) 
{

    volatile uint32_t errors = 0;
    uint32_t address = 0;
    uint16_t loop_count;
    uint8_t manufacturer_id = 0;
    uint8_t device_id = 0;


    uint64_t mcycle_start = 0U;
    uint64_t mcycle_end = 0U;
    uint64_t delta_mcycle = 0U;
    uint64_t hartid = read_csr(mhartid);


    clear_soft_interrupt();
    set_csr(mie, MIP_MSIP);

#if (IMAGE_LOADED_BY_BOOTLOADER == 0)

    /* Put this hart in WFI. */
    do
    {
        __asm("wfi");
    }while(0 == (read_csr(mip) & MIP_MSIP));

    /* The hart is now out of WFI, clear the SW interrupt. Here onwards the
     * application can enable and use any interrupts as required */

    clear_soft_interrupt();


#endif

    __enable_irq();

    /* Bring all the MMUARTs out of Reset */
    (void) mss_config_clk_rst(MSS_PERIPH_MMUART1, (uint8_t) 1, PERIPHERAL_ON);
    (void) mss_config_clk_rst(MSS_PERIPH_MMUART2, (uint8_t) 1, PERIPHERAL_ON);
    (void) mss_config_clk_rst(MSS_PERIPH_MMUART3, (uint8_t) 1, PERIPHERAL_ON);
    (void) mss_config_clk_rst(MSS_PERIPH_MMUART4, (uint8_t) 1, PERIPHERAL_ON);
    (void) mss_config_clk_rst(MSS_PERIPH_CFM, (uint8_t) 1, PERIPHERAL_ON);

    /* All clocks ON */

    MSS_UART_init(&g_mss_uart1_lo,
    MSS_UART_115200_BAUD,
    MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);

    /* Demonstrating polled MMUART transmission */
    MSS_UART_polled_tx(&g_mss_uart1_lo,g_message1,
            sizeof(g_message1));


    /*--------------------------------------------------------------------------
     * Initialize the write and read Buffers
    */
    for(loop_count = 0; loop_count < (BUFFER_A_SIZE); loop_count++)
    {
       if (loop_count < (BUFFER_A_SIZE/2))
       {
           g_flash_wr_buf[loop_count] = 0x45 + loop_count;
       }
       else
       {
           g_flash_wr_buf[loop_count] = 0x34;
       }
       g_flash_rd_buf[loop_count] = 0x00;
    }

    /*--------------------------------------------------------------------------
     * Flash Driver Initialization
    */

    FLASH_init();

    //FLASH_global_unprotect();

    //FLASH_erase_4k_block(0);

    /*--------------------------------------------------------------------------
     * Check SPI Flash part manufacturer and device ID.
    */
    while(1) {
    FLASH_read_device_id(&manufacturer_id, &device_id);
    }

    if ((manufacturer_id != FLASH_MANUFACTURER_ID) || (device_id != FLASH_DEVICE_ID))
    {
        ++errors;
    }

    /*--------------------------------------------------------------------------
     * Write Data to Flash.
    */
    //address = 200;

    //FLASH_program(address, g_flash_wr_buf, sizeof(g_flash_wr_buf));

    /*--------------------------------------------------------------------------
     * Read Data From Flash.
    */
    //address = 200;
    //FLASH_read(address, g_flash_rd_buf, sizeof(g_flash_wr_buf));

    //errors = verify_write(g_flash_wr_buf, g_flash_rd_buf, sizeof(g_flash_wr_buf));

    mcycle_start = readmcycle();
    while (1u) 
    {
    }
}

/* hart1 Software interrupt handler */

void Software_h1_IRQHandler(void) 
{
    uint64_t hart_id = read_csr(mhartid);
    count_sw_ints_h1++;
}

