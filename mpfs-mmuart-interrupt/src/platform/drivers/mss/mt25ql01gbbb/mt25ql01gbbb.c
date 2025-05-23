/***************************************************************************//**
 * Copyright 2017-2023 Microchip FPGA Embedded Systems Solutions.
 *
 * SPDX-License-Identifier: MIT
 *
 * Micron MT25QL01GBBB SPI Flash driver implementation.
 */

#include "mt25ql01gbbb.h"
#include "drivers/mss/CoreSPI/core_spi.h"
//#include "fpga_design_config/fpga_design_config.h"


#define DEVICE_ID_READ          0x9F
#define READ_CMD                0x03

#define WRITE_ENABLE_CMD        0x06
#define WRITE_DISABLE_CMD       0x04
#define PROGRAM_PAGE_CMD        0x02
#define WRITE_STATUS1_OPCODE    0x01
#define CHIP_ERASE_OPCODE       0xC4
#define ERASE_4K_BLOCK_OPCODE   0x20
#define ERASE_32K_BLOCK_OPCODE  0x52
#define ERASE_64K_BLOCK_OPCODE  0xD8
#define READ_STATUS             0x05
#define PROGRAM_RESUME_CMD      0xD0
#define READ_SECTOR_PROTECT     0x3C

#define READY_BIT_MASK          0x01
#define PROTECT_SECTOR_OPCODE     0x36
#define UNPROTECT_SECTOR_OPCODE 0x39

#define DONT_CARE               0x00u

#define NB_BYTES_PER_PAGE       256

#define BLOCK_ALIGN_MASK_4K      0xFFFFF000
#define BLOCK_ALIGN_MASK_32K     0xFFFF8000
#define BLOCK_ALIGN_MASK_64K     0xFFFF0000

spi_instance_t g_flash_core_spi; 


#define SPI_INSTANCE            &g_flash_core_spi
#define SPI_SLAVE               0
#define CORESPI_BASE_ADDR   0X40000500

/*******************************************************************************
 * Local functions
 */
static void wait_ready( void );
static uint8_t wait_ready_erase( void );

/*******************************************************************************
 *
 */
void FLASH_init( void )
{
    /*--------------------------------------------------------------------------
     * Configure SPI.
     */
    SPI_init( SPI_INSTANCE, CORESPI_BASE_ADDR, 32 );
    SPI_configure_master_mode( SPI_INSTANCE );

}

/*******************************************************************************
 *
 */
void FLASH_read_device_id
(
    uint8_t * manufacturer_id,
    uint8_t * device_id
)
{
    uint8_t read_device_id_cmd = DEVICE_ID_READ;
    uint8_t read_buffer[3];

    SPI_set_slave_select(SPI_INSTANCE, SPI_SLAVE);

    //for (int i=0; i<100000; i++){}

    SPI_transfer_block(SPI_INSTANCE, &read_device_id_cmd, 1, read_buffer, sizeof(read_buffer));

    SPI_clear_slave_select(SPI_INSTANCE, SPI_SLAVE);

    *manufacturer_id = read_buffer[0];
    *device_id = read_buffer[1];

}

/*******************************************************************************
 *
 */
void FLASH_read
(
    uint32_t address,
    uint8_t * rx_buffer,
    size_t size_in_bytes
)
{
    uint8_t cmd_buffer[6];
    
    cmd_buffer[0] = READ_CMD;
    cmd_buffer[1] = (uint8_t)((address >> 16) & 0xFF);
    cmd_buffer[2] = (uint8_t)((address >> 8) & 0xFF);;
    cmd_buffer[3] = (uint8_t)(address & 0xFF);
    cmd_buffer[4] = DONT_CARE;
    cmd_buffer[5] = DONT_CARE;
    
    SPI_set_slave_select(SPI_INSTANCE, SPI_SLAVE);
    wait_ready_erase();
    wait_ready();
    SPI_transfer_block(SPI_INSTANCE, cmd_buffer, 4, rx_buffer, size_in_bytes);
    wait_ready();
    SPI_clear_slave_select(SPI_INSTANCE, SPI_SLAVE);
    
}

/*******************************************************************************
 *
 */
void FLASH_global_unprotect( void )
{
    uint8_t cmd_buffer[2];
    /* Send Write Enable command */
    cmd_buffer[0] = WRITE_ENABLE_CMD;

    SPI_set_slave_select(SPI_INSTANCE, SPI_SLAVE);
    wait_ready();
    SPI_transfer_block(SPI_INSTANCE, cmd_buffer, 1, 0, 0);
    
    /* Send Chip Erase command */
    cmd_buffer[0] = WRITE_STATUS1_OPCODE;
    cmd_buffer[1] = 0;
    
    wait_ready();
    SPI_transfer_block(SPI_INSTANCE, cmd_buffer, 2, 0, 0);
    wait_ready();
    SPI_clear_slave_select(SPI_INSTANCE, SPI_SLAVE);
}

/*******************************************************************************
 *
 */
void FLASH_chip_erase( void )
{
    uint8_t cmd_buffer;
    /* Send Write Enable command */
    cmd_buffer = WRITE_ENABLE_CMD;

    SPI_set_slave_select(SPI_INSTANCE, SPI_SLAVE);
    wait_ready();
    SPI_transfer_block(SPI_INSTANCE, &cmd_buffer, 1, 0, 0);
    
    /* Send Chip Erase command */
    cmd_buffer = CHIP_ERASE_OPCODE;
    
    wait_ready();
    SPI_transfer_block(SPI_INSTANCE, &cmd_buffer, 1, 0, 0);
    wait_ready();
    SPI_clear_slave_select(SPI_INSTANCE, SPI_SLAVE);
}

/*******************************************************************************
 *
 */
void FLASH_erase_4k_block
(
    uint32_t address
)
{
    uint8_t cmd_buffer[4];

    address &= BLOCK_ALIGN_MASK_4K;

    /* Send Write Enable command */
    cmd_buffer[0] = WRITE_ENABLE_CMD;

    SPI_set_slave_select(SPI_INSTANCE, SPI_SLAVE);
    wait_ready();
    SPI_transfer_block(SPI_INSTANCE, cmd_buffer, 1, 0, 0);
    
    /* Send Chip Erase command */
    cmd_buffer[0] = ERASE_4K_BLOCK_OPCODE;
    cmd_buffer[1] = (address >> 16) & 0xFF;
    cmd_buffer[2] = (address >> 8 ) & 0xFF;
    cmd_buffer[3] = address & 0xFF;
    
    wait_ready();
    wait_ready_erase();
    SPI_transfer_block(SPI_INSTANCE, cmd_buffer, sizeof(cmd_buffer), 0, 0);
    wait_ready();
    wait_ready_erase();
    SPI_clear_slave_select(SPI_INSTANCE, SPI_SLAVE);
}

/*******************************************************************************
 *
 */
void FLASH_erase_64k_block
(
    uint32_t address
)
{
    uint8_t cmd_buffer[4];

    address &= BLOCK_ALIGN_MASK_64K;

    /* Send Write Enable command */
    cmd_buffer[0] = WRITE_ENABLE_CMD;

    SPI_set_slave_select(SPI_INSTANCE, SPI_SLAVE);
    wait_ready();
    SPI_transfer_block(SPI_INSTANCE, cmd_buffer, 1, 0, 0);

    /* Send Chip Erase command */
    cmd_buffer[0] = ERASE_64K_BLOCK_OPCODE;
    cmd_buffer[1] = (address >> 16) & 0xFF;
    cmd_buffer[2] = (address >> 8 ) & 0xFF;
    cmd_buffer[3] = address & 0xFF;

    wait_ready();
    SPI_transfer_block(SPI_INSTANCE, cmd_buffer, sizeof(cmd_buffer), 0, 0);
    wait_ready();
    SPI_clear_slave_select(SPI_INSTANCE, SPI_SLAVE);
}

/*******************************************************************************
 *
 */
void write_cmd_data
(
    spi_instance_t * this_spi,
    const uint8_t * cmd_buffer,
    uint16_t cmd_byte_size,
    uint8_t * data_buffer,
    uint16_t data_byte_size
)
{
    uint8_t tx_buffer[516];
    uint16_t transfer_size;
    uint16_t idx = 0;
    
    transfer_size = cmd_byte_size + data_byte_size;
    
    for(idx = 0; idx < cmd_byte_size; ++idx)
    {
        tx_buffer[idx] = cmd_buffer[idx];
    }

    for(idx = 0; idx < data_byte_size; ++idx)
    {
        tx_buffer[cmd_byte_size + idx] = data_buffer[idx];
    }

    SPI_transfer_block(SPI_INSTANCE, tx_buffer, transfer_size, 0, 0);    
    
}
        
/*******************************************************************************
 *
 */
void FLASH_program
(
    uint32_t address,
    uint8_t * write_buffer,
    size_t size_in_bytes
)
{
    uint8_t cmd_buffer[4];
    
    uint32_t in_buffer_idx;
    uint32_t nb_bytes_to_write;
    uint32_t target_addr;

    SPI_set_slave_select(SPI_INSTANCE, SPI_SLAVE);
    
    /* Send Write Enable command */
    cmd_buffer[0] = WRITE_ENABLE_CMD;
    wait_ready();
    SPI_transfer_block(SPI_INSTANCE, cmd_buffer, 1, 0, 0);     
    
    /* Unprotect sector */
    cmd_buffer[0] = UNPROTECT_SECTOR_OPCODE;
    cmd_buffer[1] = (address >> 16) & 0xFF;
    cmd_buffer[2] = (address >> 8 ) & 0xFF;
    cmd_buffer[3] = address & 0xFF;
    wait_ready();
    SPI_transfer_block(SPI_INSTANCE, cmd_buffer, sizeof(cmd_buffer), 0, 0 );
    wait_ready_erase();

    /* Send Write Enable command */
    cmd_buffer[0] = WRITE_ENABLE_CMD;
    wait_ready();
    SPI_transfer_block(SPI_INSTANCE, cmd_buffer, 1, 0, 0 );
    
    /**/
    in_buffer_idx = 0;
    nb_bytes_to_write = size_in_bytes;
    target_addr = address;
    
    while ( in_buffer_idx < size_in_bytes )
    {
        wait_ready_erase();
        uint32_t size_left;
        nb_bytes_to_write = 0x100 - (target_addr & 0xFF);   /* adjust max possible size to page boundary. */
        size_left = size_in_bytes - in_buffer_idx;
        if ( size_left < nb_bytes_to_write )
        {
            nb_bytes_to_write = size_left;
        }
        
        wait_ready();
        
        /* Send Write Enable command */
        cmd_buffer[0] = WRITE_ENABLE_CMD;
        SPI_transfer_block(SPI_INSTANCE, cmd_buffer, 1, 0, 0 );
            
        /* Program page */
        wait_ready();
    
        cmd_buffer[0] = PROGRAM_PAGE_CMD;
        cmd_buffer[1] = (target_addr >> 16) & 0xFF;
        cmd_buffer[2] = (target_addr >> 8 ) & 0xFF;
        cmd_buffer[3] = target_addr & 0xFF;
        
        write_cmd_data
          (
            SPI_INSTANCE,
            cmd_buffer,
            sizeof(cmd_buffer),
            &write_buffer[in_buffer_idx],
            nb_bytes_to_write
          );
        
        target_addr += nb_bytes_to_write;
        in_buffer_idx += nb_bytes_to_write;
    }
    
    /* Send Write Disable command. */
    cmd_buffer[0] = WRITE_DISABLE_CMD;

    wait_ready();

    SPI_transfer_block(SPI_INSTANCE,  cmd_buffer, 1, 0, 0 );
    SPI_clear_slave_select(SPI_INSTANCE, SPI_SLAVE);
}

/*******************************************************************************
 *
 */
uint8_t FLASH_get_status( void )
{
    uint8_t status;
    uint8_t command = READ_STATUS;

    wait_ready();

    SPI_transfer_block(SPI_INSTANCE, &command, 1, &status, sizeof(status));

    return status;
}

/*******************************************************************************
 *
 */
static void wait_ready( void )
{
    uint8_t ready_bit;
    uint8_t command = READ_STATUS;
    
    do {
        SPI_transfer_block(SPI_INSTANCE, &command, 1, &ready_bit, sizeof(ready_bit));
        ready_bit = ready_bit & READY_BIT_MASK;
    } while( ready_bit == 1 );
}

static uint8_t wait_ready_erase( void )
{
    uint32_t count = 0;
    uint8_t ready_bit;
    uint8_t command = 0x70 ; // FLAG_READ_STATUS;
#if 1
    do {
        SPI_transfer_block(SPI_INSTANCE, &command, 1, &ready_bit, 1);
        count++;
    } while((ready_bit & 0x80) == 0);
#endif
    return (ready_bit);
}
