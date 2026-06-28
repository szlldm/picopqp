/* Copyright (C) 2026 szlldm
 * 
 * This file is part of PicoPQP
 * 
 * The source code of this project is dual‑licensed under the GNU General Public License v3.0 (GPLv3) or the GNU Affero General Public License v3.0 (AGPLv3), at your option.
 * This project depends on the LibPQP library, which is licensed under AGPLv3 or a commercial license.
 * When this project is used together with the AGPLv3‑licensed version of the library, the resulting combined work is available only under the AGPLv3.
 * Users who obtain a commercial license for the library may instead use this project under the GPLv3 terms.
 * You should have received a copy of the AGPLv3 or GPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "libpqp/libpqp_foreign.h"
#include "libpqp/libpqp_private.h"

#include "libpqp_hws.h"

#include <string.h>		// memcpy
#include "pico.h"
#if defined(PLATFORM_RP2350)
//	#include "cmsis_gcc.h" // __DMB
	#include "RP2350.h"
#else
	#include "RP2040.h"
#endif
#include "pico/time.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/flash.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "memmap.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/vreg.h"
#include "hardware/exception.h"
//#include "pico/bootrom.h"
//#include "hardware/regs/resets.h"
//#include "hardware/structs/resets.h"

#if defined( LIBPQP_HAS_I2C1 ) || defined( LIBPQP_HAS_I2C2 )
#include "hardware/i2c.h"
#include <pico/i2c_slave.h>
#endif

#define ROUTING_TABLE_ADDR_SPACE		(64U)		// maximum address = ROUTING_TABLE_ADDR_SPACE - 2, broadcast = ROUTING_TABLE_ADDR_SPACE - 1
#if ROUTING_TABLE_ADDR_SPACE > 256
 #error invalid ROUTING_TABLE_ADDR_SPACE
#endif
#if ( ROUTING_TABLE_ADDR_SPACE * ROUTING_TABLE_ADDR_SPACE ) > ( USER_CONFIG_OFFSET - ROUTING_TABLE_OFFSET )
 #error the provided ROUTING_TABLE_ADDR_SPACE conflicts with the memory map
#endif

static uint32_t pqphws_sys_clock_freq = SYS_CLK_MHZ * 1000000ul;
static uint8_t _routing_table_cache[3] = { 0xFF, 0xFF, 0xFF };

static bool flash_range_program_with_check( uint32_t flash_offs, const uint8_t * data, size_t count )
{
	if ( flash_offs % FLASH_PAGE_SIZE ) return false;
	if ( count % FLASH_PAGE_SIZE ) return false;
	volatile uint8_t * readback = (volatile uint8_t *) ( XIP_BASE + flash_offs );

	bool erased = true;
	bool already_written = true;

	for ( size_t i = 0; i < count; i++ )
	{
		if ( readback[i] != 0xFF ) erased = false;
		if ( readback[i] != data[i] ) already_written = false;
		if ( ( !erased ) && ( !already_written ) ) return false;
	}

	if ( already_written ) return true;

	flash_range_program( flash_offs, data, count );

	for ( size_t i = 0; i < count; i++ )
	{
		if ( readback[i] != data[i] ) return false;
	}
	return true;
}

static bool flash_range_erase_with_check( uint32_t flash_offs, size_t count )
{
	if ( flash_offs % FLASH_SECTOR_SIZE ) return false;
	if ( count % FLASH_SECTOR_SIZE ) return false;
	volatile uint8_t * readback = (volatile uint8_t *) ( XIP_BASE + flash_offs );

	flash_range_erase( flash_offs, count );

	for ( size_t i = 0; i < count; i++ )
	{
		if ( readback[i] != 0xFF ) return false;
	}
	return true;
}


#define APP_HEADER_SIZE		( 256 )
#define FW_SIGNATURE		"LIBPQP03"

#if defined( LIBPQP_HAS_CAN1 ) || defined( LIBPQP_HAS_CAN2 )
#include "can2040/can2040.h"
#include "hardware/structs/pio.h"
#endif

void pqpf_reboot( uint32_t latch_bootloader )
{
	if ( ( latch_bootloader == PQP_BLDR_CODE_BOOTLOADER_LATCH ) || ( latch_bootloader == PQP_BLDR_CODE_FIRMWARE_UPDATE ) )
	{
		watchdog_hw->scratch[1] = latch_bootloader;
		watchdog_hw->scratch[2] = 0xB00710AD;
		watchdog_hw->scratch[3] = 0x1A7CC0DE;
	}
	else
	{
		watchdog_hw->scratch[1] = 0;
		watchdog_hw->scratch[2] = 0;
		watchdog_hw->scratch[3] = 0;
	}
	watchdog_hw->scratch[0] = 0x3EB007ED;
	watchdog_reboot(0, 0, 1);
	while(1)
	{
		;
	}
}

static uint32_t boot_word = 0;

uint32_t pqpf_bootloader_latch_reboot( void )
{
	static bool has_been_read = false;
	static uint32_t latched_code = 0;
	if ( has_been_read ) return latched_code;
	has_been_read = true;
	if ( ( watchdog_hw->scratch[1] == 0x1A7C4B00 ) && ( watchdog_hw->scratch[2] == 0x710ADE31 ) && ( watchdog_hw->scratch[3] == 0xAAAA5555 ) )
	{
		// backward compatibility with old bootloader latch
		latched_code = PQP_BLDR_CODE_BOOTLOADER_LATCH;
	}
	else
	{
		latched_code =  watchdog_hw->scratch[1];
		if ( watchdog_hw->scratch[2] != 0xB00710AD ) latched_code = 0;
		if ( watchdog_hw->scratch[3] != 0x1A7CC0DE ) latched_code = 0;
		if ( ( latched_code != PQP_BLDR_CODE_BOOTLOADER_LATCH ) && ( latched_code != PQP_BLDR_CODE_FIRMWARE_UPDATE ) )
		{
			latched_code = 0;
		}
	}
	watchdog_hw->scratch[1] = 0;
	watchdog_hw->scratch[2] = 0;
	watchdog_hw->scratch[3] = 0;
	boot_word = watchdog_hw->scratch[0];
	watchdog_hw->scratch[0] = 0x9B1C4A57;
	return latched_code;
}


bool pqpf_check_valid_firmware( void )
{
	// check BINARY_INFO_MARKER_START
#if defined(PLATFORM_RP2350)
	volatile uint8_t * marker = (volatile uint8_t *) (XIP_BASE + APP_OFFSET + APP_HEADER_SIZE + 0x124);
#else
	volatile uint8_t * marker = (volatile uint8_t *) (XIP_BASE + APP_OFFSET + APP_HEADER_SIZE + 0xD4);
#endif
	if ( marker[0] != 0xF2 ) return false;
	if ( marker[1] != 0xEB ) return false;
	if ( marker[2] != 0x88 ) return false;
	if ( marker[3] != 0x71 ) return false;

	volatile uint8_t * signature = (volatile uint8_t *)(XIP_BASE + APP_OFFSET + APP_HEADER_SIZE - 16);
	if ( !pqp_volatile_compare( signature, FW_SIGNATURE, 8) ) return false;

	uint32_t fw_size = pqpf_get_firmware_length( );
	if (fw_size < 4) return false;
	if (fw_size >= ( APP_SIZE - APP_HEADER_SIZE ) ) return false;

	return ( pqpf_get_firmware_stored_checksum( ) == pqpf_get_firmware_checksum( ) );
}

uint16_t pqpf_check_valid_clone_firmware( uint32_t address_start )			// returns 0 if no valid cone firmware, else returns the block count
{
	if ( address_start % 256 ) return 0;	// address must be page aligned
	if ( address_start <= XIP_BASE + APP_OFFSET ) return 0;	// must be after the app
	// check BINARY_INFO_MARKER_START
	#if defined(PLATFORM_RP2350)
	volatile uint8_t * marker = (volatile uint8_t *) (address_start + APP_HEADER_SIZE + 0x124);
	#else
	volatile uint8_t * marker = (volatile uint8_t *) (address_start + APP_HEADER_SIZE + 0xD4);
	#endif
	if ( marker[0] != 0xF2 ) return 0;
	if ( marker[1] != 0xEB ) return 0;
	if ( marker[2] != 0x88 ) return 0;
	if ( marker[3] != 0x71 ) return 0;

	volatile uint8_t * signature = (volatile uint8_t *)(address_start + APP_HEADER_SIZE - 16);
	if ( !pqp_volatile_compare( signature, FW_SIGNATURE, 8) ) return 0;

	uint32_t fw_size;
	pqp_volatile_copy(&fw_size, (volatile uint8_t *)(address_start + APP_HEADER_SIZE - 8), 4);

	if (fw_size < 4) return 0;
	if (fw_size >= ( APP_SIZE - APP_HEADER_SIZE ) ) return 0;

	uint32_t stored_checksum;
	pqp_volatile_copy( &stored_checksum, (volatile uint8_t *)(address_start + APP_HEADER_SIZE - 4), 4);	// LAST 4 BYTE OF THE APP HEADER STORES THE UPLOADED FIRMWARE'S CHECKSUM

	uint32_t crc;
	volatile uint8_t * code = (volatile uint8_t *) (address_start + APP_HEADER_SIZE);
	pqp_crc32_init( &crc );
	for ( int i=0; i<(APP_SIZE - APP_HEADER_SIZE); i++ )
	{
		if ( i >= fw_size ) break;
		pqp_crc32_update( &crc, code[i] );
	}
	pqp_crc32_update( &crc, fw_size );
	if ( stored_checksum != pqp_crc32_final( &crc ) ) return 0;

	return ( ( fw_size + APP_HEADER_SIZE + 255 ) / 256 );
}

#ifdef LIBPQP_IS_BOOTLOADER
static exception_handler_t original_fault_handler = NULL;
#endif

void pqpf_start_firmware( void )							// if success, it should not return
{
#ifdef LIBPQP_IS_BOOTLOADER
	watchdog_update( );

	watchdog_hw->scratch[0] = boot_word;

	if ( original_fault_handler != NULL )
	{
		exception_restore_handler( HARDFAULT_EXCEPTION , original_fault_handler );
		original_fault_handler = NULL;
	}

	// Turn off interrupts (NVIC ICER, NVIC ICPR)
	hw_set_bits((io_rw_32 *)0xe000e180, 0xFFFFFFFF);
	hw_set_bits((io_rw_32 *)0xe000e280, 0xFFFFFFFF);

#if defined(PLATFORM_RP2350)
	// Set VTOR register, set stack pointer, and jump to reset
	asm volatile (
		"mov r0, %[start]\n"
		"ldr r1, =%[vtable]\n"
		"str r0, [r1]\n"
		"ldmia r0, {r0, r1}\n"
		"msr msp, r0\n"
		"bx r1\n"
		:
		: [start] "r" (XIP_BASE + APP_OFFSET + APP_HEADER_SIZE), [vtable] "X" (PPB_BASE + M33_VTOR_OFFSET)
		:
	);
#else
	// Set VTOR register, set stack pointer, and jump to reset
	asm volatile (
		"mov r0, %[start]\n"
		"ldr r1, =%[vtable]\n"
		"str r0, [r1]\n"
		"ldmia r0, {r0, r1}\n"
		"msr msp, r0\n"
		"bx r1\n"
		:
		: [start] "r" (XIP_BASE + APP_OFFSET + APP_HEADER_SIZE), [vtable] "X" (PPB_BASE + M0PLUS_VTOR_OFFSET)
		:
	);
#endif
#endif
}

static int32_t _shadow_routing_table_base_address = -1;
static uint8_t _shadow_routing_table_in_ram[256];

uint32_t pqpf_get_routing_table_checksum( void )
{
	uint32_t crc;
	volatile uint8_t * table = (volatile uint8_t *) (XIP_BASE + ROUTING_TABLE_OFFSET);

	pqp_crc32_init( &crc );
	for ( int i=0; i<( ROUTING_TABLE_ADDR_SPACE * ROUTING_TABLE_ADDR_SPACE ); i++ )
	{
		pqp_crc32_update( &crc, table[i] );
	}
	return pqp_crc32_final( &crc );
}

int pqpf_clear_routing_table( void )						// sets the 256byte array to 0x00 or 0xFF; returns -1 on failure, else 0
{
	_routing_table_cache[0] = 0xFF;		//invalidate cache
	_routing_table_cache[1] = 0xFF;
	_routing_table_cache[2] = 0xFF;
#ifdef LIBPQP_IS_FIRMWARE
	bool multicore_lockout_is_initialized = multicore_lockout_victim_is_initialized( 1 );
	if ( multicore_lockout_is_initialized )
	{
		multicore_lockout_start_blocking( );
	}
#endif
	uint32_t ints = save_and_disable_interrupts( );
	flash_range_erase( ROUTING_TABLE_OFFSET, ( ( ( ROUTING_TABLE_ADDR_SPACE * ROUTING_TABLE_ADDR_SPACE ) + ( FLASH_SECTOR_SIZE - 1 ) ) / FLASH_SECTOR_SIZE ) * FLASH_SECTOR_SIZE );
	restore_interrupts( ints );
#ifdef LIBPQP_IS_FIRMWARE
	if ( multicore_lockout_is_initialized )
	{
		multicore_lockout_end_blocking( );
	}
#endif
	_shadow_routing_table_base_address = -1;
	volatile uint8_t * table = (volatile uint8_t *) (XIP_BASE + ROUTING_TABLE_OFFSET);
	for ( int i=0; i<( ROUTING_TABLE_ADDR_SPACE * ROUTING_TABLE_ADDR_SPACE ); i++ )
	{
		if ( table[i] != 0xFF ) return -1;		// check erased table
	}
	return 0;
}

int pqpf_write_routing_table( uint8_t src_addr, uint8_t dst_addr, uint8_t data )			// returns -1 on failure, else 0
{
	_routing_table_cache[0] = 0xFF;		//invalidate cache
	_routing_table_cache[1] = 0xFF;
	_routing_table_cache[2] = 0xFF;

	bool last = ( ( src_addr == 0xFF ) && ( dst_addr == 0xFF ) );
  #if ROUTING_TABLE_ADDR_SPACE < 256
	if ( src_addr == 0xFF )
	{
		src_addr = ( ROUTING_TABLE_ADDR_SPACE - 1 );
	}
	else
	{
		if ( src_addr > ( ROUTING_TABLE_ADDR_SPACE - 2 ) ) return -1;
	}
	if ( dst_addr == 0xFF )
	{
		dst_addr = ( ROUTING_TABLE_ADDR_SPACE - 1 );
	}
	else
	{
		if ( dst_addr > ( ROUTING_TABLE_ADDR_SPACE - 2 ) ) return -1;
	}
  #endif

	uint32_t entry_address = src_addr;
	entry_address *= ROUTING_TABLE_ADDR_SPACE;
	entry_address += dst_addr;
	uint32_t base_address = ( entry_address / 256U ) * 256U;
	uint32_t page_address = entry_address % 256U;

	if ( _shadow_routing_table_base_address < 0 )
	{
		if ( last )	// no need to write
		{
			return 0;
		}
		_shadow_routing_table_base_address = base_address;
		for ( int i = 0; i < 256; i++ )
		{
			_shadow_routing_table_in_ram[i] = 0xFF;
		}
	}
	if ( _shadow_routing_table_base_address == base_address )
	{
		_shadow_routing_table_in_ram[page_address & 0xFF] = data;
		if ( !last ) return 0;
	}
	// store shadow table in flash, when address is broadcast (0xFF)
#ifdef LIBPQP_IS_FIRMWARE
	bool multicore_lockout_is_initialized = multicore_lockout_victim_is_initialized( 1 );
	if ( multicore_lockout_is_initialized )
	{
		multicore_lockout_start_blocking( );
	}
#endif
	uint32_t ints = save_and_disable_interrupts( );
	flash_range_program( ROUTING_TABLE_OFFSET + _shadow_routing_table_base_address, _shadow_routing_table_in_ram, 256 );
	restore_interrupts( ints );
#ifdef LIBPQP_IS_FIRMWARE
	if ( multicore_lockout_is_initialized )
	{
		multicore_lockout_end_blocking( );
	}
#endif
	volatile uint8_t * table = (volatile uint8_t *) (XIP_BASE + ROUTING_TABLE_OFFSET);
	for ( int i=0; i<256; i++ )
	{
		if ( table[_shadow_routing_table_base_address + i] != _shadow_routing_table_in_ram[i] )		// check write
		{
			_shadow_routing_table_base_address = -1;
			return -1;
		}
	}

	if ( last )
	{
		_shadow_routing_table_base_address = -1;
	}
	else
	{
		if ( _shadow_routing_table_base_address != base_address )
		{
			_shadow_routing_table_base_address = base_address;
			for ( int i = 0; i < 256; i++ )
			{
				_shadow_routing_table_in_ram[i] = 0xFF;
				_shadow_routing_table_in_ram[page_address & 0xFF] = data;
			}
		}
		else
		{
			_shadow_routing_table_base_address = -1;
		}
	}

	return 0;
}

uint8_t pqpf_read_routing_table( uint8_t src_addr, uint8_t dst_addr )
{
  #if ROUTING_TABLE_ADDR_SPACE < 256
	if ( src_addr > ( ROUTING_TABLE_ADDR_SPACE - 2 ) ) return 0;	// cannot be broadcast
	if ( dst_addr > ( ROUTING_TABLE_ADDR_SPACE - 2 ) )
	{
		if ( dst_addr == 0xFF )
		{
			dst_addr = ( ROUTING_TABLE_ADDR_SPACE - 1 );
		}
		else
		{
			return 0;
		}
	}
  #endif
	if ( ( _routing_table_cache[0] == src_addr ) && ( _routing_table_cache[0] != 0xFF ) )
	{
		if ( ( _routing_table_cache[1] == dst_addr ) && ( _routing_table_cache[1] != 0xFF ) )
		{
			return _routing_table_cache[2];
		}
	}
	volatile uint8_t * table = (volatile uint8_t *) (XIP_BASE + ROUTING_TABLE_OFFSET);
	uint32_t entry_address = src_addr;
	entry_address *= ROUTING_TABLE_ADDR_SPACE;
	entry_address += dst_addr;
	uint8_t entry = table[entry_address];
	_routing_table_cache[0] = src_addr;
	_routing_table_cache[1] = dst_addr;
	_routing_table_cache[2] = entry;
	return entry;
}


int pqpf_firmware_erase( void )							// returns -1 on failure, else 0
{
#ifdef LIBPQP_IS_BOOTLOADER
	volatile uint8_t * signature = (volatile uint8_t *)(XIP_BASE + APP_OFFSET + APP_HEADER_SIZE - 16);
	if ( !pqp_volatile_compare( signature, FW_SIGNATURE, 8) ) return -1;

	uint32_t fw_size = pqpf_get_firmware_length( );
	if (fw_size < 4) return -1;
	if (fw_size >= ( APP_SIZE - APP_HEADER_SIZE ) ) return -1;

	bool success = true;

	for ( int i=FLASH_SECTOR_SIZE; i<( fw_size + APP_HEADER_SIZE ); i+=FLASH_SECTOR_SIZE )
	{
		watchdog_update( );
		bool erased_sector = true;
		volatile uint8_t * sector = (volatile uint8_t *) (XIP_BASE + APP_OFFSET + i);
		for ( int j=0; j<FLASH_SECTOR_SIZE; j++)
		{
			if ( sector[j] != 0xFF )
			{
				erased_sector = false;
				break;
			}
		}
		if ( erased_sector ) continue;
		uint32_t ints = save_and_disable_interrupts( );
		success &= flash_range_erase_with_check( APP_OFFSET + i, FLASH_SECTOR_SIZE );
		restore_interrupts( ints );
	}
	// erase first sector (including app header)
	{
		watchdog_update( );
		volatile uint8_t * sector = (volatile uint8_t *) (XIP_BASE + APP_OFFSET + 0);
		uint32_t ints = save_and_disable_interrupts( );
		success &= flash_range_erase_with_check( APP_OFFSET + 0, FLASH_SECTOR_SIZE );
		restore_interrupts( ints );
	}
	watchdog_update( );
	if ( !success ) return -1;
	return 0;
#else
	return -1;
#endif
}

int pqpf_erase_all( void )							// returns -1 on failure, else 0
{
#ifdef LIBPQP_IS_BOOTLOADER
	bool success = true;
	for ( int i=0; i<APP_SIZE; i+=FLASH_SECTOR_SIZE )
	{
		watchdog_update( );
		bool erased_sector = true;
		volatile uint8_t * sector = (volatile uint8_t *) (XIP_BASE + APP_OFFSET + i);
		for ( int j=0; j<FLASH_SECTOR_SIZE; j++)
		{
			if ( sector[j] != 0xFF )
			{
				erased_sector = false;
				break;
			}
		}
		if ( erased_sector ) continue;
		uint32_t ints = save_and_disable_interrupts( );
		success &= flash_range_erase_with_check( APP_OFFSET + i, FLASH_SECTOR_SIZE );
		restore_interrupts( ints );
	}
	watchdog_update( );
	if ( !success ) return -1;
	return 0;
#else
	return -1;
#endif
}

int pqpf_firmware_erase_section( uint32_t address, uint32_t length )
{
	// check alignment
	if ( address % FLASH_SECTOR_SIZE ) return -1;
	if ( length % FLASH_SECTOR_SIZE ) return -1;
	if ( length == 0 ) return -1;
	if ( address > ( address + length ) ) return -1;
#ifdef LIBPQP_IS_BOOTLOADER
	if ( address < ( XIP_BASE + APP_OFFSET ) ) return -1;
	if ( ( address + length ) > ( XIP_BASE + FLASH_SIZE ) ) return -1;
#else
	if ( address < ( XIP_BASE + APP_OFFSET + APP_HEADER_SIZE + pqpf_get_firmware_length( ) ) ) return -1;
#endif
	bool success = true;
	address -= XIP_BASE;
	for ( int i=0; i<length; i+=FLASH_SECTOR_SIZE )
	{
		watchdog_update( );
#ifdef LIBPQP_IS_FIRMWARE
		bool multicore_lockout_is_initialized = multicore_lockout_victim_is_initialized( 1 );
		if ( multicore_lockout_is_initialized )
		{
			multicore_lockout_start_blocking( );
		}
#endif
		uint32_t ints = save_and_disable_interrupts( );
		success &= flash_range_erase_with_check( address + i, FLASH_SECTOR_SIZE );
		restore_interrupts( ints );
#ifdef LIBPQP_IS_FIRMWARE
		if ( multicore_lockout_is_initialized )
		{
			multicore_lockout_end_blocking( );
		}
#endif
	}
	watchdog_update( );
	if ( !success ) return -1;
	return 0;
}

int pqpf_firmware_read( uint8_t * dst_buf, uint32_t address, int size )		// returns -1 on failure, else returns the number of bytes read
{
	if ( address < ( XIP_BASE + ROUTING_TABLE_OFFSET ) ) return -1;

	for ( int i=0; i<size; i++ )
	{
		if ( address >= ( XIP_BASE + FLASH_SIZE ) ) return -1;
		dst_buf[i] = *((volatile uint8_t *)address);
		address += 1;
	}

	return size;
}

int pqpf_firmware_write( uint8_t * src_buf, uint32_t address, int size )		// returns -1 on failure, else returns the number of bytes written
{
	if ( address & 0xFF ) return -1;	// address must be page aligned
	if ( size <= 0 ) return -1;
	if ( size & 0xFF ) return -1;	// size must be multiple of page size
	if ( address > ( address + size ) ) return -1;	// check rollback
#ifdef LIBPQP_IS_BOOTLOADER
	if ( address < ( XIP_BASE + ROUTING_TABLE_OFFSET ) ) return -1;
	if ( ( address + size ) > ( XIP_BASE + FLASH_SIZE ) ) return -1;
#else
	if ( address < ( XIP_BASE + APP_OFFSET + APP_HEADER_SIZE + pqpf_get_firmware_length( ) ) ) return -1;
#endif
#ifdef LIBPQP_IS_FIRMWARE
	bool multicore_lockout_is_initialized = multicore_lockout_victim_is_initialized( 1 );
	if ( multicore_lockout_is_initialized )
	{
		multicore_lockout_start_blocking( );
	}
#endif
	uint32_t ints = save_and_disable_interrupts( );
	bool success = flash_range_program_with_check( address - XIP_BASE, src_buf, size );
	restore_interrupts( ints );
#ifdef LIBPQP_IS_FIRMWARE
	if ( multicore_lockout_is_initialized )
	{
		multicore_lockout_end_blocking( );
	}
#endif
	if ( !success ) return -1;
	return size;
}

int pqpf_clone_local_firmware( uint16_t block_count, uint32_t write_address_start, uint32_t read_address_start )	// returns -1 on failure, else 0
{
#ifdef LIBPQP_IS_BOOTLOADER
	if ( block_count == 0 ) return -1;
	if ( write_address_start % 256 ) return -1;	// address must be page aligned
	if ( read_address_start % 256 ) return -1;	// address must be page aligned
	if ( write_address_start != ( XIP_BASE + APP_OFFSET ) ) return -1;
	uint32_t length = block_count * 256ul;
	if ( read_address_start < ( write_address_start + length ) ) return -1;
	if ( read_address_start >= ( XIP_BASE + FLASH_SIZE ) ) return -1;
	if ( ( read_address_start + length ) > ( XIP_BASE + FLASH_SIZE ) ) return -1;

	uint8_t page_buf[256];
	bool success = true;

	for ( uint32_t i = 1; i < block_count; i++ )
	{
		watchdog_update( );
		volatile uint8_t * read_page = (volatile uint8_t *) ( read_address_start + i*256 );
		pqp_volatile_copy( page_buf, read_page, 256 );
		uint32_t ints = save_and_disable_interrupts( );
		success &= flash_range_program_with_check( write_address_start + i*256 - XIP_BASE, page_buf, 256 );
		restore_interrupts( ints );
	}
	// write app header
	watchdog_update( );
	volatile uint8_t * read_page = (volatile uint8_t *) ( read_address_start );
	pqp_volatile_copy( page_buf, read_page, 256 );
	uint32_t ints = save_and_disable_interrupts( );
	success &= flash_range_program_with_check( write_address_start - XIP_BASE, page_buf, 256 );
	restore_interrupts( ints );

	if ( !success ) return -1;
	return 0;
#else
	return -1;
#endif
}


uint32_t pqpf_get_bootloader_checksum( void )
{
	uint32_t crc;
	volatile uint8_t * code = (volatile uint8_t *) (XIP_BASE + 0);

	pqp_crc32_init( &crc );
	for ( int i=0; i<PROTECTED_DATA_OFFSET; i++ )
	{
		pqp_crc32_update( &crc, code[i] );
	}
	return pqp_crc32_final( &crc );
}

uint32_t pqpf_get_firmware_checksum( void )
{
	uint32_t crc;
	volatile uint8_t * code = (volatile uint8_t *) (XIP_BASE + APP_OFFSET + APP_HEADER_SIZE);
	uint32_t fw_size = pqpf_get_firmware_length( );
	pqp_crc32_init( &crc );
	for ( int i=0; i<(APP_SIZE - APP_HEADER_SIZE); i++ )
	{
		if ( i >= fw_size ) break;
		pqp_crc32_update( &crc, code[i] );
	}
	pqp_crc32_update( &crc, pqpf_get_firmware_length( ) );
	return pqp_crc32_final( &crc );
}

uint32_t pqpf_get_firmware_stored_checksum( void )
{
	uint32_t stored_checksum;
	pqp_volatile_copy( &stored_checksum, (volatile uint8_t *)(XIP_BASE + APP_OFFSET + APP_HEADER_SIZE - 4), 4);	// LAST 4 BYTE OF THE APP HEADER STORES THE UPLOADED FIRMWARE'S CHECKSUM
	return stored_checksum;
}

uint32_t pqpf_get_firmware_length( void )
{
	uint32_t fw_size;
	pqp_volatile_copy(&fw_size, (volatile uint8_t *)(XIP_BASE + APP_OFFSET + APP_HEADER_SIZE - 8), 4);
	return fw_size;
}

uint32_t pqpf_get_section_checksum( uint32_t address, uint32_t length )			// returns 0xFFFFFFFF on failure, else the checksum
{
	uint32_t crc = 0xFFFFFFFF;
	if ( address % 256 ) return crc;
	if ( length % 256 ) return crc;
	if ( length == 0 ) return crc;
	if ( address < XIP_BASE ) return crc;
	if ( address >= ( XIP_BASE + FLASH_SIZE ) ) return crc;
	if ( ( address + length ) > ( XIP_BASE + FLASH_SIZE ) ) return crc;

	pqp_crc32_init( &crc );
	volatile uint8_t * code = (volatile uint8_t *) (XIP_BASE);
	address -= XIP_BASE;
	for ( uint32_t i = address; i < ( address + length ); i++ )
	{
		pqp_crc32_update( &crc, code[i] );
	}
	return pqp_crc32_final( &crc );
}

bool pqpf_get_section_erased( uint32_t address, uint32_t length )
{
	if ( address % 256 ) return false;
	if ( length % 256 ) return false;
	if ( length == 0 ) return false;
	if ( address < XIP_BASE ) return false;
	if ( address >= ( XIP_BASE + FLASH_SIZE ) ) return false;
	if ( ( address + length ) > ( XIP_BASE + FLASH_SIZE ) ) return false;

	volatile uint8_t * code = (volatile uint8_t *) (XIP_BASE);
	address -= XIP_BASE;
	for ( uint32_t i = address; i < ( address + length ); i++ )
	{
		if ( code[i] != 0xFF ) return false;
	}
	return true;
}

uint32_t pgpf_get_ms_timestamp( void )
{
	return to_ms_since_boot( get_absolute_time( ) );
}

uint32_t pgpf_get_uptime( void )
{
	return ( to_us_since_boot( get_absolute_time( ) ) / 1000000ULL );
}

static mutex_t pqpf_multicore_mutex;
void pqpf_multicore_init_lock( void )
{
	mutex_init( &pqpf_multicore_mutex );
}

void pqpf_multicore_lock( void )
{
	mutex_enter_blocking( &pqpf_multicore_mutex );
}

void pqpf_multicore_unlock( void )
{
	mutex_exit( &pqpf_multicore_mutex );
}

uint8_t pqpf_hw_info_length( void )
{
	return 2;
}

void pqpf_copy_hw_info( uint8_t * data )
{
#if defined(PLATFORM_RP2350)
	if ( boot_word == 0 )
	{
		uint32_t chip_reset = powman_hw->chip_reset;

		if ( chip_reset & POWMAN_CHIP_RESET_HAD_POR_BITS )
		{
			data[0] = 'S';
		}
		else if ( chip_reset & POWMAN_CHIP_RESET_HAD_RUN_LOW_BITS )
		{
			data[0] = 'P';
		}
		else if ( chip_reset & POWMAN_CHIP_RESET_HAD_GLITCH_DETECT_BITS )
		{
			data[0] = 'G';
		}
		else if ( (chip_reset & POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_RSM_BITS ) || (chip_reset & POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_SWCORE_BITS ) || (chip_reset & POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_POWMAN_BITS ) || (chip_reset & POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_POWMAN_ASYNC_BITS ))
		{
			data[0] = 'W';
		}
		else if ( chip_reset & POWMAN_CHIP_RESET_HAD_HZD_SYS_RESET_REQ_BITS )
		{
			data[0] = 'H';
		}
		else if ( chip_reset & POWMAN_CHIP_RESET_HAD_SWCORE_PD_BITS )
		{
			data[0] = 'T';
		}
		else if ( chip_reset & POWMAN_CHIP_RESET_HAD_RESCUE_BITS )
		{
			data[0] = 'L';
		}
		else if ( chip_reset & POWMAN_CHIP_RESET_HAD_DP_RESET_REQ_BITS )
		{
			data[0] = 'D';
		}
		else if ( chip_reset & POWMAN_CHIP_RESET_HAD_BOR_BITS )
		{
			data[0] = 'B';
		}
		else
		{
			data[0] = 'U';
		}
	} else
	if ( boot_word == 0x3EB007ED )
	{
		data[0] = 'R';
	} else
	if ( boot_word == 0x9B1C4A57 )
	{
		data[0] = 'C';
	} else
	if ( boot_word == 0xEEC7BCFF )
	{
		data[0] = '/';
	} else
	if ( boot_word == 0xEEC7AC00 )
	{
		data[0] = '0';
	} else
	if ( boot_word == 0xEEC7AC11 )
	{
		data[0] = '1';
	}
	else
	{
		data[0] = 'U';
	}

	data[1] = 0;
	//uint32_t reg = vreg_and_chip_reset_hw->vreg;
	//if ( reg & VREG_AND_CHIP_RESET_VREG_ROK_BITS ) data[1] |= 0x01;
	uint32_t reg = pll_sys_hw->cs;
	if ( reg & PLL_CS_LOCK_BITS ) data[1] |= 0x02;
	reg = pll_usb_hw->cs;
	if ( reg & PLL_CS_LOCK_BITS ) data[1] |= 0x04;
#else
	if ( boot_word == 0 )
	{
		uint32_t chip_reset = vreg_and_chip_reset_hw->chip_reset;

		if ( chip_reset & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_POR_BITS )
		{
			data[0] = 'S';
		} else
		if ( chip_reset & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_RUN_BITS )
		{
			data[0] = 'P';
		} else
		if ( chip_reset & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_PSM_RESTART_BITS )
		{
			data[0] = 'D';
		}
		else
		{
			data[0] = 'U';
		}
	} else
	if ( boot_word == 0x3EB007ED )
	{
		data[0] = 'R';
	} else
	if ( boot_word == 0x9B1C4A57 )
	{
		data[0] = 'C';
	} else
	if ( boot_word == 0xEEC7BCFF )
	{
		data[0] = '/';
	} else
	if ( boot_word == 0xEEC7AC00 )
	{
		data[0] = '0';
	} else
	if ( boot_word == 0xEEC7AC11 )
	{
		data[0] = '1';
	}
	else
	{
		data[0] = 'U';
	}

	data[1] = 0;
	uint32_t reg = vreg_and_chip_reset_hw->vreg;
	if ( reg & VREG_AND_CHIP_RESET_VREG_ROK_BITS ) data[1] |= 0x01;
	reg = pll_sys_hw->cs;
	if ( reg & PLL_CS_LOCK_BITS ) data[1] |= 0x02;
	reg = pll_usb_hw->cs;
	if ( reg & PLL_CS_LOCK_BITS ) data[1] |= 0x04;
}
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// STDIO INTERFACE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef LIBPQP_HAS_STDIOIF
void pqpf_stdio_if_init( void )							// should contain some initialization code, if needed
{
	;
}

int pqpf_stdio_if_getchar( void )						// returns received char, or -1 if no data available
{
	return getchar_timeout_us(0);
}

void pqpf_stdio_if_sendchar( uint8_t data )					// start a single char send
{
	putchar_raw(data);
}

void pqpf_stdio_if_flush( void )						// flush send buffer
{
	stdio_flush();
}

#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAN INTERFACE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined( LIBPQP_HAS_CAN1 ) || defined( LIBPQP_HAS_CAN2 )

#ifndef CAN2040_RX_IRQ_RING_BUFFER_SIZE
	#define CAN_RX_BUF_SIZE		( 16 )
#else
	#define CAN_RX_BUF_SIZE		( CAN2040_RX_IRQ_RING_BUFFER_SIZE )
#endif

#if ( CAN_RX_BUF_SIZE < 8 )
	#error CAN2040_RX_IRQ_RING_BUFFER_SIZE is not sufficient
#endif

#ifndef CAN2040_CAN_BITRATE
	#error CAN2040_CAN_BITRATE not defined
#endif

typedef struct
{
	uint32_t pio_num;
	unsigned int irq_num;
	struct can2040 bus;
	uint32_t sys_clock, bitrate, gpio_rx, gpio_tx;

	volatile struct can2040_msg rx_buffer[CAN_RX_BUF_SIZE];
	volatile uint32_t rx_buffer_head, rx_buffer_tail;

	volatile bool rx_lost, rx_buffer_lost;
} can_t;


#endif


#ifdef LIBPQP_HAS_CAN1
static can_t can1;

static void PIO0_IRQHandler( void )
{
	can2040_pio_irq_handler( &(can1.bus) );
}

static void can1_callback( struct can2040 *cd, uint32_t notify, struct can2040_msg *msg )
{
	// called from IRQ handler !
	if ( notify == CAN2040_NOTIFY_RX )
	{
		uint32_t current_head, next_head;

		current_head = can1.rx_buffer_head;
		if ( current_head >= CAN_RX_BUF_SIZE )
		{
			current_head -= CAN_RX_BUF_SIZE;
		}
		next_head = current_head + 1;
		if ( next_head >= CAN_RX_BUF_SIZE )
		{
			next_head -= CAN_RX_BUF_SIZE;
		}

		if ( next_head == can1.rx_buffer_tail )
		{
			can1.rx_buffer_lost = true;
		}
		else
		{
			pqp_volatile_copy( &(can1.rx_buffer[current_head]), msg, sizeof( struct can2040_msg ) );
			can1.rx_buffer_head = next_head;
		}
	} else
	if ( notify == CAN2040_NOTIFY_ERROR )
	{
		can1.rx_lost = true;
	}
}

#ifndef CAN2040_CAN1_GPIO_RX
	#error CAN2040_CAN1_GPIO_RX not defined
#endif
#ifndef CAN2040_CAN1_GPIO_TX
	#error CAN2040_CAN1_GPIO_TX not defined
#endif

void pqpf_can_if1_init( void )							// should contain some initialization code, if needed
{
	can1.pio_num = 0;
	can1.irq_num = PIO0_IRQ_0_IRQn;

	can1.rx_buffer_head = 0;
	can1.rx_buffer_tail = 0;
	can1.rx_lost = false;
	can1.rx_buffer_lost = false;

	can1.sys_clock = pqphws_sys_clock_freq;
	can1.bitrate = CAN2040_CAN_BITRATE;
	can1.gpio_rx = CAN2040_CAN1_GPIO_RX;
	can1.gpio_tx = CAN2040_CAN1_GPIO_TX;

	// Setup canbus
	can2040_setup( &(can1.bus), can1.pio_num );
	can2040_callback_config( &(can1.bus), can1_callback );

	// Enable irqs
	irq_set_exclusive_handler( can1.irq_num, PIO0_IRQHandler );
	NVIC_SetPriority( can1.irq_num, 1 );
	NVIC_EnableIRQ( can1.irq_num );

	// Start canbus
	can2040_start( &(can1.bus), can1.sys_clock, can1.bitrate, can1.gpio_rx, can1.gpio_tx );
}

void pqpf_can_if1_deinit( void )
{
	can2040_stop( &(can1.bus) );
	NVIC_DisableIRQ( can1.irq_num );
	irq_remove_handler( can1.irq_num, PIO0_IRQHandler );
}

void pqpf_can_if1_clear_tx( void )							// clear transmitter queue
{
	can2040_stop( &(can1.bus) );
	can2040_setup( &(can1.bus), can1.pio_num );
	can2040_callback_config( &(can1.bus), can1_callback );
	can2040_start( &(can1.bus), can1.sys_clock, can1.bitrate, can1.gpio_rx, can1.gpio_tx );
}

int pqpf_can_if1_enqueue_fragment( uint32_t eid, uint8_t * data, int data_len)	// returns -1 on failure; copy fragment into the transmitter queue; eid lower 29 bits
{
	if ( data_len < 0 ) return -1;
	if ( data_len > 8 ) return -1;
	struct can2040_msg tmp;
	tmp.id = ( CAN2040_ID_EFF | ( eid & 0x1FFFFFFF ) );
	tmp.dlc = data_len;
	memcpy( tmp.data, data, data_len );
	return can2040_transmit( &(can1.bus), &tmp );
}

int pqpf_can_if1_received_fragment( uint32_t * eid, uint8_t * data )		// returns data length of the received fragment
{
	if ( can1.rx_buffer_head == can1.rx_buffer_tail ) return -1;

	int data_len;
	uint32_t current_head, current_tail, next_tail;
	volatile struct can2040_msg * tmp;

	current_head = can1.rx_buffer_head;
	if ( current_head >= CAN_RX_BUF_SIZE )
	{
		current_head -= CAN_RX_BUF_SIZE;
	}
	current_tail = can1.rx_buffer_tail;
	if ( current_tail >= CAN_RX_BUF_SIZE )
	{
		current_tail -= CAN_RX_BUF_SIZE;
	}

	if ( current_head == current_tail )
	{
		can1.rx_buffer_tail = current_tail;	// tail might be overflowed?
		return -1;
	}

	next_tail = current_tail + 1;
	if ( next_tail >= CAN_RX_BUF_SIZE )
	{
		next_tail -= CAN_RX_BUF_SIZE;
	}

	tmp = &(can1.rx_buffer[current_tail]);
	pqp_volatile_copy( eid, &(tmp->id) , sizeof( uint32_t ) );
	data_len = tmp->dlc;
	if ( data_len > 8 )
	{
		data_len = 8;
	}
	if ( data_len > 0)
	{
		pqp_volatile_copy( data, tmp->data, data_len );
	}

	can1.rx_buffer_tail = next_tail;

	return data_len;
}

#endif

#ifdef LIBPQP_HAS_CAN2
static can_t can2;

static void PIO1_IRQHandler( void )
{
	can2040_pio_irq_handler( &(can2.bus) );
}

static void can2_callback( struct can2040 *cd, uint32_t notify, struct can2040_msg *msg )
{
	// called from IRQ handler !
	if ( notify == CAN2040_NOTIFY_RX )
	{
		uint32_t current_head, next_head;

		current_head = can2.rx_buffer_head;
		if ( current_head >= CAN_RX_BUF_SIZE )
		{
			current_head -= CAN_RX_BUF_SIZE;
		}
		next_head = current_head + 1;
		if ( next_head >= CAN_RX_BUF_SIZE )
		{
			next_head -= CAN_RX_BUF_SIZE;
		}

		if ( next_head == can2.rx_buffer_tail )
		{
			can2.rx_buffer_lost = true;
		}
		else
		{
			pqp_volatile_copy( &(can2.rx_buffer[current_head]), msg, sizeof( struct can2040_msg ) );
			can2.rx_buffer_head = next_head;
		}
	} else
	if ( notify == CAN2040_NOTIFY_ERROR )
	{
		can2.rx_lost = true;
	}
}

#ifndef CAN2040_CAN2_GPIO_RX
	#error CAN2040_CAN2_GPIO_RX not defined
#endif
#ifndef CAN2040_CAN2_GPIO_TX
	#error CAN2040_CAN2_GPIO_TX not defined
#endif

void pqpf_can_if2_init( void )
{
	can2.pio_num = 1;
	can2.irq_num = PIO1_IRQ_0_IRQn;

	can2.rx_buffer_head = 0;
	can2.rx_buffer_tail = 0;

	can2.sys_clock = pqphws_sys_clock_freq;
	can2.bitrate = CAN2040_CAN_BITRATE;
	can2.gpio_rx = CAN2040_CAN2_GPIO_RX;
	can2.gpio_tx = CAN2040_CAN2_GPIO_TX;

	// Setup canbus
	can2040_setup( &(can2.bus), can2.pio_num );
	can2040_callback_config( &(can2.bus), can2_callback );

	// Enable irqs
	irq_set_exclusive_handler( can2.irq_num, PIO1_IRQHandler );
	NVIC_SetPriority( can2.irq_num, 1 );
	NVIC_EnableIRQ( can2.irq_num );

	// Start canbus
	can2040_start( &(can2.bus), can2.sys_clock, can2.bitrate, can2.gpio_rx, can2.gpio_tx );
}

void pqpf_can_if2_deinit( void )
{
	can2040_stop( &(can2.bus) );
	NVIC_DisableIRQ( can2.irq_num );
	irq_remove_handler( can2.irq_num, PIO1_IRQHandler );
}

void pqpf_can_if2_clear_tx( void )
{
	can2040_stop( &(can2.bus) );
	can2040_setup( &(can2.bus), can2.pio_num );
	can2040_callback_config( &(can2.bus), can2_callback );
	can2040_start( &(can2.bus), can2.sys_clock, can2.bitrate, can2.gpio_rx, can2.gpio_tx );
}

int pqpf_can_if2_enqueue_fragment( uint32_t eid, uint8_t * data, int data_len)
{
	if ( data_len < 0 ) return -1;
	if ( data_len > 8 ) return -1;
	struct can2040_msg tmp;
	tmp.id = ( CAN2040_ID_EFF | ( eid & 0x1FFFFFFF ) );
	tmp.dlc = data_len;
	memcpy( tmp.data, data, data_len );
	return can2040_transmit( &(can2.bus), &tmp );
}

int pqpf_can_if2_received_fragment( uint32_t * eid, uint8_t * data )
{
	if ( can2.rx_buffer_head == can2.rx_buffer_tail ) return -1;

	int data_len;
	uint32_t current_head, current_tail, next_tail;
	volatile struct can2040_msg * tmp;

	current_head = can2.rx_buffer_head;
	if ( current_head >= CAN_RX_BUF_SIZE )
	{
		current_head -= CAN_RX_BUF_SIZE;
	}
	current_tail = can2.rx_buffer_tail;
	if ( current_tail >= CAN_RX_BUF_SIZE )
	{
		current_tail -= CAN_RX_BUF_SIZE;
	}

	if ( current_head == current_tail )
	{
		can2.rx_buffer_tail = current_tail;	// tail might be overflowed?
		return -1;
	}

	next_tail = current_tail + 1;
	if ( next_tail >= CAN_RX_BUF_SIZE )
	{
		next_tail -= CAN_RX_BUF_SIZE;
	}

	tmp = &(can2.rx_buffer[current_tail]);
	pqp_volatile_copy( eid, &(tmp->id) , sizeof( uint32_t ) );
	data_len = tmp->dlc;
	if ( data_len > 8 )
	{
		data_len = 8;
	}
	if ( data_len > 0)
	{
		pqp_volatile_copy( data, tmp->data, data_len );
	}

	can2.rx_buffer_tail = next_tail;

	return data_len;
}

#endif




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// HDUART INTERFACE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define HDUART_FRAME_END			( 0xC0 )
#define HDUART_TX_PUSH_BYTES			(0)
#define HDUART_TX_MIN_LENGTH			(8)

#ifndef HDUART_RX_IRQ_RING_BUFFER_SIZE
	#define UART_RX_BUF_SIZE		( 256 )
#else
	#define UART_RX_BUF_SIZE		( HDUART_RX_IRQ_RING_BUFFER_SIZE )
#endif

#ifdef LIBPQP_HAS_HDUPLEX_UART1
static volatile uint8_t uart0_rx_data[UART_RX_BUF_SIZE];
static volatile uint32_t uart0_rx_head, uart0_rx_tail;
static uint8_t * uart0_tx_data;
static volatile int uart0_tx_len, uart0_tx_bytes, uart0_tx_ptr, uart0_rx_ptr;


static inline void uart0_enable_tx_irq( void )
{
	uart0_hw->imsc = ( ( 1 << UART_UARTIMSC_TXIM_LSB) | ( 1 << UART_UARTIMSC_RXIM_LSB) | ( 1 << UART_UARTIMSC_RTIM_LSB ) );
}

static inline void uart0_disable_tx_irq( void )
{
	uart0_hw->imsc = ( ( 1 << UART_UARTIMSC_RXIM_LSB) | ( 1 << UART_UARTIMSC_RTIM_LSB ) );
}

static void UART0_IRQHandler( void )
{
	if ( uart0_hw->mis & ( UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS ) )	// RX or receive timeout interrupt
	{
		while ( uart_is_readable( uart0 ) )
		{
			uint32_t data32 = uart0_hw->dr;
			if ( data32 & 0xFFFFFF00 )	// some error happened during receiving
			{
				data32 = HDUART_FRAME_END;
			}
			uint8_t data = ( uint8_t )data32;
			if ( uart0_rx_ptr >= uart0_tx_len )	// goes to the ring buffer
			{
				uint32_t head = uart0_rx_head;
				uint32_t head_next = head + 1;
				if ( head_next >= UART_RX_BUF_SIZE )
				{
					head_next = 0;
				}
				if ( head_next == uart0_rx_tail )	// ring buffer is full
				{
					if ( head == 0 )
					{
						head = UART_RX_BUF_SIZE;
					}
					head -= 1;
					uart0_rx_data[head] = HDUART_FRAME_END;	// overwrite last valid entry with FRAME_END ( ~= invalidate)
				}
				else
				{
					uart0_rx_data[head] = data;
					uart0_rx_head = head_next;
				}

			}
			else					// check transmission
			{
				if ( uart0_tx_data == NULL )
				{
					uart0_rx_ptr = -1;
					uart0_tx_len = -1;
				}
				else
				{
					if ( uart0_tx_data[uart0_rx_ptr] != data )
					{
						uart0_rx_ptr = -1;
						uart0_tx_len = -1;
					}
					else
					{
						uart0_rx_ptr += 1;
					}
				}
			}
		}
	}

	if ( uart0_hw->mis & UART_UARTMIS_TXMIS_BITS )		// TX interrupt
	{
		if ( uart0_tx_data == NULL )
		{
			uart0_disable_tx_irq( );
			return;
		}
		if ( uart0_tx_ptr >= uart0_tx_bytes )
		{
			uart0_disable_tx_irq( );
			return;
		}
		while ( uart_is_writable( uart0 ) )
		{
			if ( uart0_tx_ptr < uart0_tx_len )
			{
				uart0_hw->dr = uart0_tx_data[uart0_tx_ptr];
			}
			else
			{
				uart0_hw->dr = HDUART_FRAME_END;
			}
			uart0_tx_ptr += 1;
			if ( uart0_tx_ptr >= uart0_tx_bytes )
			{
				uart0_disable_tx_irq( );
				return;
			}
		}
	}
}

void pqpf_hduart_if1_init( void )							// should contain some initialization code, if needed
{
	uart0_tx_data = NULL;
	uart0_tx_len = uart0_tx_bytes = uart0_tx_ptr = uart0_rx_ptr = -1;
	uart0_rx_head = 0;
	uart0_rx_tail = 0;
	uart_init( uart0, HDUART_BAUD_RATE );
	uart_set_format( uart0, 8, 1, UART_PARITY_NONE );
	gpio_disable_pulls( UART0_HDUART1_GPIO_RX );
	gpio_disable_pulls( UART0_HDUART1_GPIO_TX );
	gpio_set_function( UART0_HDUART1_GPIO_RX, GPIO_FUNC_UART );
	gpio_set_function( UART0_HDUART1_GPIO_TX, GPIO_FUNC_UART );
	uart_set_hw_flow( uart0, false, false );

	irq_set_exclusive_handler( UART0_IRQ_IRQn, UART0_IRQHandler );
	uart0_disable_tx_irq( );	// this enables RX irqs
	// Set FIFO levels:
	// RX FIFO: interrupt when >= 1/4 == 8 bytes
	// TX FIFO: interrupt when <= 1/8 == 4 bytes
	uart0_hw->ifls = ( 1 << UART_UARTIFLS_RXIFLSEL_LSB | 0 << UART_UARTIFLS_TXIFLSEL_LSB );


	NVIC_SetPriority( UART0_IRQ_IRQn, 2 );

	NVIC_EnableIRQ( UART0_IRQ_IRQn );
}

void pqpf_hduart_if1_deinit( void )							// stop and deinit the interface
{
	NVIC_DisableIRQ( UART0_IRQ_IRQn );
	irq_remove_handler( UART0_IRQ_IRQn, UART0_IRQHandler );
	gpio_deinit( UART0_HDUART1_GPIO_RX );
	gpio_deinit( UART0_HDUART1_GPIO_TX );
	uart_deinit( uart0 );
}

bool pqpf_hduart_if1_is_busy( void )						// return true, if there are already trafic on the HDUART, or last RX happened in less than 1ms
{
	if ( !gpio_get( UART0_HDUART1_GPIO_RX ) ) return true;			// line is 0V, must be communicating
	if ( uart0_rx_head != uart0_rx_tail ) return true;			// rx ring buffer not empty
	if ( !( uart0_hw->fr & UART_UARTFR_RXFE_BITS ) ) return true;		// uart rx fifo not empty
	if ( uart0_hw->fr & UART_UARTFR_BUSY_BITS ) return true;		// transmitting
	return false;
}

void pqpf_hduart_if1_send_data( uint8_t * data, int len )				// starts a HDUART transmit procedure (probably interrupt driven),
{
	if ( pqpf_hduart_if1_is_busy( ) )
	{
		uart0_tx_data = NULL;
		uart0_tx_bytes = -1;
		uart0_tx_len = -1;
		uart0_tx_ptr = -1;
		uart0_rx_ptr = -1;
		return;
	}
	uart0_tx_len = -1;
	uart0_rx_ptr = -1;
	uart0_tx_data = data;
	uart0_tx_bytes = ( ( len < HDUART_TX_MIN_LENGTH ) ? HDUART_TX_MIN_LENGTH : len );
	uart0_tx_bytes += HDUART_TX_PUSH_BYTES;
	uart0_tx_ptr = 0;
	uart0_rx_ptr = 0;
	uart0_tx_len = len;
	while ( uart_is_writable( uart0 ) )
	{
		if ( ( uart0_tx_ptr + 1 ) >= uart0_tx_bytes ) break;
		if ( uart0_tx_ptr < uart0_tx_len )
		{
			uart0_hw->dr = uart0_tx_data[uart0_tx_ptr];
		}
		else
		{
			uart0_hw->dr = HDUART_FRAME_END;
		}
		uart0_tx_ptr += 1;
	}
	uart0_enable_tx_irq( );
}

int pqpf_hduart_if1_send_status( void )						// returns -1 on failure (HDUART busy or HDUART echo failed), 0 on busy, 1 on success
{
	if ( uart0_hw->fr & UART_UARTFR_BUSY_BITS ) return 0;		// still transmitting
	if ( uart0_tx_ptr < 0 ) return -1;
	if ( uart0_tx_ptr == uart0_tx_bytes )
	{
		if ( uart0_rx_ptr < 0 )
		{
			return -1;
		}

		if ( uart0_rx_ptr < uart0_tx_len )
		{
			if ( !( uart0_hw->fr & UART_UARTFR_RXFE_BITS ) ) return 0;		// uart rx fifo not empty
		}

		if ( uart0_rx_ptr == uart0_tx_len )
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}
	return 0;
}

int pqpf_hduart_if1_receive_data( void )						// get received bytes (not from own transmission), or returns -1 if no data available
{
	if ( uart0_rx_head == uart0_rx_tail ) return -1;
	uint32_t tail = uart0_rx_tail;
	uint8_t data = uart0_rx_data[tail];
	tail += 1;
	if ( tail >= UART_RX_BUF_SIZE )
	{
		tail = 0;
	}
	uart0_rx_tail = tail;
	return data;
}
#endif

#ifdef LIBPQP_HAS_HDUPLEX_UART2
static volatile uint8_t uart1_rx_data[UART_RX_BUF_SIZE];
static volatile uint32_t uart1_rx_head, uart1_rx_tail;
static uint8_t * uart1_tx_data;
static volatile int uart1_tx_len, uart1_tx_bytes, uart1_tx_ptr, uart1_rx_ptr;


static inline void uart1_enable_tx_irq( void )
{
	uart1_hw->imsc = ( ( 1 << UART_UARTIMSC_TXIM_LSB) | ( 1 << UART_UARTIMSC_RXIM_LSB) | ( 1 << UART_UARTIMSC_RTIM_LSB ) );
}

static inline void uart1_disable_tx_irq( void )
{
	uart1_hw->imsc = ( ( 1 << UART_UARTIMSC_RXIM_LSB) | ( 1 << UART_UARTIMSC_RTIM_LSB ) );
}

static void UART1_IRQHandler( void )
{
	if ( uart1_hw->mis & ( UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS ) )	// RX or receive timeout interrupt
	{
		while ( uart_is_readable( uart1 ) )
		{
			uint32_t data32 = uart1_hw->dr;
			if ( data32 & 0xFFFFFF00 )	// some error happened during receiving
			{
				data32 = HDUART_FRAME_END;
			}
			uint8_t data = ( uint8_t )data32;
			if ( uart1_rx_ptr >= uart1_tx_len )	// goes to the ring buffer
			{
				uint32_t head = uart1_rx_head;
				uint32_t head_next = head + 1;
				if ( head_next >= UART_RX_BUF_SIZE )
				{
					head_next = 0;
				}
				if ( head_next == uart1_rx_tail )	// ring buffer is full
				{
					if ( head == 0 )
					{
						head = UART_RX_BUF_SIZE;
					}
					head -= 1;
					uart1_rx_data[head] = HDUART_FRAME_END;	// overwrite last valid entry with FRAME_END ( ~= invalidate)
				}
				else
				{
					uart1_rx_data[head] = data;
					uart1_rx_head = head_next;
				}

			}
			else					// check transmission
			{
				if ( uart1_tx_data == NULL )
				{
					uart1_rx_ptr = -1;
					uart1_tx_len = -1;
				}
				else
				{
					if ( uart1_tx_data[uart1_rx_ptr] != data )
					{
						uart1_rx_ptr = -1;
						uart1_tx_len = -1;
					}
					else
					{
						uart1_rx_ptr += 1;
					}
				}
			}
		}
	}

	if ( uart1_hw->mis & UART_UARTMIS_TXMIS_BITS )		// TX interrupt
	{
		if ( uart1_tx_data == NULL )
		{
			uart1_disable_tx_irq( );
			return;
		}
		if ( uart1_tx_ptr >= uart1_tx_bytes )
		{
			uart1_disable_tx_irq( );
			return;
		}
		while ( uart_is_writable( uart1 ) )
		{
			if ( uart1_tx_ptr < uart1_tx_len )
			{
				uart1_hw->dr = uart1_tx_data[uart1_tx_ptr];
			}
			else
			{
				uart1_hw->dr = HDUART_FRAME_END;
			}
			uart1_tx_ptr += 1;
			if ( uart1_tx_ptr >= uart1_tx_bytes )
			{
				uart1_disable_tx_irq( );
				return;
			}
		}
	}
}

void pqpf_hduart_if2_init( void )							// should contain some initialization code, if needed
{
	uart1_tx_data = NULL;
	uart1_tx_len = uart1_tx_bytes = uart1_tx_ptr = uart1_rx_ptr = -1;
	uart1_rx_head = 0;
	uart1_rx_tail = 0;
	uart_init( uart1, HDUART_BAUD_RATE );
	uart_set_format( uart1, 8, 1, UART_PARITY_NONE );
	gpio_disable_pulls( UART1_HDUART2_GPIO_RX );
	gpio_disable_pulls( UART1_HDUART2_GPIO_TX );
	gpio_set_function( UART1_HDUART2_GPIO_RX, GPIO_FUNC_UART );
	gpio_set_function( UART1_HDUART2_GPIO_TX, GPIO_FUNC_UART );
	uart_set_hw_flow( uart1, false, false );

	irq_set_exclusive_handler( UART1_IRQ_IRQn, UART1_IRQHandler );
	uart1_disable_tx_irq( );	// this enables RX irqs
	// Set FIFO levels:
	// RX FIFO: interrupt when >= 1/4 == 8 bytes
	// TX FIFO: interrupt when <= 1/8 == 4 bytes
	uart1_hw->ifls = ( 1 << UART_UARTIFLS_RXIFLSEL_LSB | 0 << UART_UARTIFLS_TXIFLSEL_LSB );


	NVIC_SetPriority( UART1_IRQ_IRQn, 2 );

	NVIC_EnableIRQ( UART1_IRQ_IRQn );
}

void pqpf_hduart_if2_deinit( void )							// stop and deinit the interface
{
	NVIC_DisableIRQ( UART1_IRQ_IRQn );
	irq_remove_handler( UART1_IRQ_IRQn, UART1_IRQHandler );
	gpio_deinit( UART1_HDUART2_GPIO_RX );
	gpio_deinit( UART1_HDUART2_GPIO_TX );
	uart_deinit( uart1 );
}

bool pqpf_hduart_if2_is_busy( void )						// return true, if there are already trafic on the HDUART, or last RX happened in less than 1ms
{
	if ( !gpio_get( UART1_HDUART2_GPIO_RX ) ) return true;			// line is 0V, must be communicating
	if ( uart1_rx_head != uart1_rx_tail ) return true;			// rx ring buffer not empty
	if ( !( uart1_hw->fr & UART_UARTFR_RXFE_BITS ) ) return true;		// uart rx fifo not empty
	if ( uart1_hw->fr & UART_UARTFR_BUSY_BITS ) return true;		// transmitting
	return false;
}

void pqpf_hduart_if2_send_data( uint8_t * data, int len )				// starts a HDUART transmit procedure (probably interrupt driven),
{
	if ( pqpf_hduart_if2_is_busy( ) )
	{
		uart1_tx_data = NULL;
		uart1_tx_bytes = -1;
		uart1_tx_len = -1;
		uart1_tx_ptr = -1;
		uart1_rx_ptr = -1;
		return;
	}
	uart1_tx_len = -1;
	uart1_rx_ptr = -1;
	uart1_tx_data = data;
	uart1_tx_bytes = ( ( len < HDUART_TX_MIN_LENGTH ) ? HDUART_TX_MIN_LENGTH : len );
	uart1_tx_bytes += HDUART_TX_PUSH_BYTES;
	uart1_tx_ptr = 0;
	uart1_rx_ptr = 0;
	uart1_tx_len = len;
	while ( uart_is_writable( uart1 ) )
	{
		if ( ( uart1_tx_ptr + 1 ) >= uart1_tx_bytes ) break;
		if ( uart1_tx_ptr < uart1_tx_len )
		{
			uart1_hw->dr = uart1_tx_data[uart1_tx_ptr];
		}
		else
		{
			uart1_hw->dr = HDUART_FRAME_END;
		}
		uart1_tx_ptr += 1;
	}
	uart1_enable_tx_irq( );
}

int pqpf_hduart_if2_send_status( void )						// returns -1 on failure (HDUART busy or HDUART echo failed), 0 on busy, 1 on success
{
	if ( uart1_hw->fr & UART_UARTFR_BUSY_BITS ) return 0;		// still transmitting
	if ( uart1_tx_ptr < 0 ) return -1;
	if ( uart1_tx_ptr == uart1_tx_bytes )
	{
		if ( uart1_rx_ptr < 0 )
		{
			return -1;
		}

		if ( uart1_rx_ptr < uart1_tx_len )
		{
			if ( !( uart1_hw->fr & UART_UARTFR_RXFE_BITS ) ) return 0;		// uart rx fifo not empty
		}

		if ( uart1_rx_ptr == uart1_tx_len )
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}
	return 0;
}

int pqpf_hduart_if2_receive_data( void )						// get received bytes (not from own transmission), or returns -1 if no data available
{
	if ( uart1_rx_head == uart1_rx_tail ) return -1;
	uint32_t tail = uart1_rx_tail;
	uint8_t data = uart1_rx_data[tail];
	tail += 1;
	if ( tail >= UART_RX_BUF_SIZE )
	{
		tail = 0;
	}
	uart1_rx_tail = tail;
	return data;
}
#endif






/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// I2C INTERFACE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined( LIBPQP_HAS_I2C1) || defined( LIBPQP_HAS_I2C2 )

#define I2C_FRAME_END			( 0xC0 )
#define I2C_TX_PUSH_BYTES			(4)
#define I2C_TX_MIN_LENGTH			(8)

#ifndef I2C_RX_IRQ_RING_BUFFER_SIZE
	#define I2C_RX_BUF_SIZE		( 256 )
#else
	#define I2C_RX_BUF_SIZE		( I2C_RX_IRQ_RING_BUFFER_SIZE )
#endif

/*
bool i2c_reserved_addr_10bit(uint16_t addr)
{
    return (addr & 0x0378) == 0 || (addr & 0x0378) == 0x78;
}


uint i2c_master_init_10bit(i2c_inst_t *i2c, uint baudrate)
{
	uint rtnValue = i2c_init(i2c, baudrate);
	// Configure as a fast-mode master with RepStart support, 10-bit addresses
	i2c->hw->con =
			I2C_IC_CON_SPEED_VALUE_FAST << I2C_IC_CON_SPEED_LSB |
			I2C_IC_CON_MASTER_MODE_BITS |
			I2C_IC_CON_IC_SLAVE_DISABLE_BITS |
			I2C_IC_CON_IC_RESTART_EN_BITS |
			I2C_IC_CON_IC_10BITADDR_MASTER_BITS |
			I2C_IC_CON_TX_EMPTY_CTRL_BITS;
	return rtnValue;
}

void i2c_set_slave_mode_10bit(i2c_inst_t *i2c, bool slave, uint16_t addr)
{
    invalid_params_if(I2C, addr >= 0xF00); // 7-bit addresses
    invalid_params_if(I2C, i2c_reserved_addr_10bit(addr));
    i2c->hw->enable = 0;
    uint32_t ctrl_set_if_master = I2C_IC_CON_IC_10BITADDR_MASTER_BITS | I2C_IC_CON_MASTER_MODE_BITS | I2C_IC_CON_IC_SLAVE_DISABLE_BITS;
    uint32_t ctrl_set_if_slave = I2C_IC_CON_IC_10BITADDR_SLAVE_BITS | I2C_IC_CON_RX_FIFO_FULL_HLD_CTRL_BITS;
    if (slave)
    {
        hw_write_masked(&i2c->hw->con,
            ctrl_set_if_slave,
            ctrl_set_if_master | ctrl_set_if_slave
        );
        i2c->hw->sar = addr & 0x03FF;
    }
    else
    {
        hw_write_masked(&i2c->hw->con,
            ctrl_set_if_master,
            ctrl_set_if_master | ctrl_set_if_slave
        );
    }
    i2c->hw->enable = 1;
}*/

#endif

#ifdef LIBPQP_HAS_I2C1

static volatile uint8_t i2c0_rx_data[I2C_RX_BUF_SIZE];
static volatile uint32_t i2c0_rx_head, i2c0_rx_tail;
static uint8_t * i2c0_tx_data;
static volatile int i2c0_tx_len, i2c0_tx_ptr;
static int i2c0_write_stm;
static uint8_t i2c0_modeState;		//0 slave 1 master
//static uint32_t i2c0_last_event_us;


uint8_t pqpf_i2c_if1_getModeState()
{
	return i2c0_modeState;
}

int pqpf_i2c_if1_get_write_state()
{
	return i2c0_write_stm;
}

void i2c0_switch_to_master_mode( void )
{
	i2c_slave_deinit(i2c0);
	i2c0_modeState = 1;
	/*irq_set_enabled(I2C0_IRQ, 0);


	i2c_slave_deinit(i2c0);
	i2c_deinit(i2c0);
	i2c0_tx_data = NULL;
	i2c0_tx_len = i2c0_tx_ptr = -1;
	i2c0_rx_head = 0;
	i2c0_rx_tail = 0;
	gpio_init(I2C0_I2CI1_GPIO_SDA);
	gpio_init(I2C0_I2CI1_GPIO_SCL);
	i2c0_write_stm = 0;

	//i2c0_last_event_us = time_us_32();

	i2c_init(i2c0, I2C_BITRATE * 1000);
	gpio_set_function( I2C0_I2CI1_GPIO_SDA, GPIO_FUNC_I2C );
	gpio_set_function( I2C0_I2CI1_GPIO_SCL, GPIO_FUNC_I2C );
	gpio_pull_up(I2C0_I2CI1_GPIO_SDA);
	gpio_pull_up(I2C0_I2CI1_GPIO_SCL);
	irq_set_enabled(I2C0_IRQ, 1);*/
}

// i2c slave ISR
static void i2c0_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event)
{
	//i2c0_last_event_us = time_us_32();
	switch (event)
	{
		case I2C_SLAVE_RECEIVE: // master has written some data
			while(i2c_get_hw(i2c)->rxflr)
			{
				uint8_t data = (uint8_t) i2c0_inst.hw->data_cmd;
				uint32_t head = i2c0_rx_head;
				uint32_t head_next = head + 1;
				if ( head_next >= I2C_RX_BUF_SIZE )
				{
					head_next = 0;
				}
				if ( head_next == i2c0_rx_tail )	// ring buffer is full
				{
					if ( head == 0 )
					{
						head = I2C_RX_BUF_SIZE;
					}
					head -= 1;
					i2c0_rx_data[head] = I2C_FRAME_END;	// overwrite last valid entry with FRAME_END ( ~= invalidate)
				}
				else
				{
					i2c0_rx_data[head] = data;
					i2c0_rx_head = head_next;
				}
			}
			break;
		case I2C_SLAVE_REQUEST: // master is requesting data
			//do nothing, because master never request data from slave
			i2c_write_byte_raw(i2c, 0x11);
			break;
		case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
			break;
		default:
			break;
	}
}

void i2c0_switch_to_slave_mode( void )
{
	//working 3 lines
	//i2c0_modeState = 0;
	//i2c_slave_init(i2c0, 42, &i2c0_slave_handler);
	//irq_set_enabled(I2C0_IRQ, 0);
	//irq_set_enabled(I2C0_IRQ, 1);


	//test lines
	irq_set_enabled(I2C0_IRQ, 0);

	//i2c_slave_deinit(i2c0);
	i2c_deinit(i2c0);
	//i2c0_tx_data = NULL;
	//i2c0_tx_len = i2c0_tx_ptr = -1;
	//i2c0_rx_head = 0;
	//i2c0_rx_tail = 0;
	//gpio_init(I2C0_I2CI1_GPIO_SDA);
	//gpio_init(I2C0_I2CI1_GPIO_SCL);
	i2c0_write_stm = 0;
	i2c0_modeState = 0;

	//i2c0_last_event_us = time_us_32();

	i2c_init(i2c0, I2C_BITRATE);
	//gpio_set_function( I2C0_I2CI1_GPIO_SDA, GPIO_FUNC_I2C );
	//gpio_set_function( I2C0_I2CI1_GPIO_SCL, GPIO_FUNC_I2C );
	//gpio_pull_up(I2C0_I2CI1_GPIO_SDA);
	//gpio_pull_up(I2C0_I2CI1_GPIO_SCL);
	i2c_slave_init(i2c0, 42, &i2c0_slave_handler);
	//i2c0_last_event_us = time_us_32();


	irq_set_enabled(I2C0_IRQ, 1);

}

static int i2c0_write_status(void)
{
	uint32_t abort_reason;
	//bool abort;
	int loop_guard = 32;

	while (i2c0_write_stm >= 0 && loop_guard > 0)
	{
		loop_guard -= 1;
		switch (i2c0_write_stm)
		{
			case 0:

				if ( i2c0_tx_ptr < (i2c0_tx_len -1) )
				{
					i2c0_inst.hw->data_cmd = i2c0_tx_data[i2c0_tx_ptr];
				}
				else
				{
					i2c0_inst.hw->data_cmd = ( (1 << I2C_IC_DATA_CMD_STOP_LSB)) | (  i2c0_tx_data[i2c0_tx_ptr] );
				}
				i2c0_tx_ptr += 1;
				i2c0_write_stm += 1;
				break;
			case 1:
				if (!(i2c0_inst.hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_EMPTY_BITS)) return 0;
				i2c0_write_stm += 1;
				break;
			case 2:
				abort_reason = i2c0_inst.hw->tx_abrt_source;
				if (abort_reason)
				{
					i2c0_inst.hw->clr_tx_abrt;
					i2c0_write_stm = -1;
					return -1;
				}
				if (i2c0_tx_ptr < i2c0_tx_len)
				{
					i2c0_write_stm = 0;
				}
				else
				{
					i2c0_write_stm += 1;
				}
				break;
			case 3:
				if (!(i2c0_inst.hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_STOP_DET_BITS)) return 0;
				i2c0_inst.hw->clr_stop_det;
				i2c0_write_stm = 4;
				i2c0_tx_data = NULL;
				return 1;
			case 4:
				return 1;
			default:
				return -1;
		}
	}

	return -1;
}

static void i2c0_master_handler()
{
	if ( i2c0_tx_data == NULL )
	{
		NVIC_DisableIRQ( I2C0_IRQ );
		irq_remove_handler( I2C0_IRQ, i2c0_master_handler );
		i2c0_switch_to_slave_mode();
		return;
	}
	int tmprtn = i2c0_write_status( );
	if(tmprtn == 1)
	{
		//transmission end
		NVIC_DisableIRQ( I2C0_IRQ );
		irq_remove_handler( I2C0_IRQ, i2c0_master_handler );
		i2c0_switch_to_slave_mode();
	}
	else if(tmprtn == -1)
	{
		//transmission end
		i2c0_tx_ptr = -1;
		NVIC_DisableIRQ( I2C0_IRQ );
		irq_remove_handler( I2C0_IRQ, i2c0_master_handler );
		i2c0_switch_to_slave_mode();
	}
}

void pqpf_i2c_if1_init( void )							// should contain some initialization code, if needed
{
	i2c0_tx_data = NULL;
	i2c0_tx_len = i2c0_tx_ptr = -1;
	i2c0_rx_head = 0;
	i2c0_rx_tail = 0;
	gpio_init(I2C0_I2CI1_GPIO_SDA);
	gpio_init(I2C0_I2CI1_GPIO_SCL);
	i2c0_write_stm = 0;
	i2c0_modeState = 0;

	i2c_init(i2c0, I2C_BITRATE);
	gpio_set_function( I2C0_I2CI1_GPIO_SDA, GPIO_FUNC_I2C );
	gpio_set_function( I2C0_I2CI1_GPIO_SCL, GPIO_FUNC_I2C );
	gpio_pull_up(I2C0_I2CI1_GPIO_SDA);
	gpio_pull_up(I2C0_I2CI1_GPIO_SCL);

	i2c0_switch_to_slave_mode();
}

void pqpf_i2c_if1_hw_init( void )							// should contain some initialization code, if needed
{
	pqpf_i2c_if1_init();
}

void pqpf_i2c_if1_deinit( void )
{
	NVIC_DisableIRQ( I2C0_IRQ );
	irq_remove_handler( I2C0_IRQ, i2c0_master_handler );

	gpio_deinit( I2C0_I2CI1_GPIO_SDA );
	gpio_deinit( I2C0_I2CI1_GPIO_SCL );
	i2c_slave_deinit(i2c0);
	i2c_deinit(i2c0);
}

bool pqpf_i2c_if1_is_busy( void )						// return true, if there are already trafic on the I2C, or last RX happened in less than 1ms
{
	if ( i2c0_rx_head != i2c0_rx_tail ) return true;			// rx ring buffer not empty
	if(i2c0_inst.hw->status & I2C_IC_STATUS_SLV_ACTIVITY_BITS ) return true;				//slave state machine not in idle state
	if(i2c0_inst.hw->status & I2C_IC_STATUS_MST_ACTIVITY_BITS ) return true;				//master state machine not in idle state
	if(!(i2c0_inst.hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_EMPTY_BITS)) return true;		// still transmitting
	return false;
}

void pqpf_i2c_if1_send_data(uint8_t addr, uint8_t * data, int len)
{
	if ( pqpf_i2c_if1_is_busy( ) )
	{
		i2c0_tx_data = NULL;
		i2c0_tx_len = -1;
		i2c0_tx_ptr = -1;
		return;
	}

	i2c0_switch_to_master_mode();

	//i2c_init(&i2c1_inst, I2C_BITRATE * 1000);
	i2c0_inst.hw->enable = 0;
	i2c0_inst.hw->tar = addr;
	i2c0_inst.hw->enable = 1;

	i2c0_write_stm = 0;

	i2c0_tx_len = -1;
	i2c0_tx_data = data;
	i2c0_tx_ptr = 0;
	i2c0_tx_len = len;

	//set interrupt callback to i2c0_master_handler
	// Set up the interrupt handlers.
	irq_set_exclusive_handler(I2C0_IRQ, i2c0_master_handler);
	NVIC_SetPriority( I2C0_IRQ, 2 );
	NVIC_EnableIRQ( I2C0_IRQ );
}

int pqpf_i2c_if1_send_status( void )						// returns -1 on failure (i2c busy), 0 on busy, 1 on success
{
	if(i2c0_inst.hw->status & I2C_IC_STATUS_MST_ACTIVITY_BITS ) return 0;				//master state machine not in idle state, still transmitting
	if(!(i2c0_inst.hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_EMPTY_BITS)) return 0;		// still transmitting
	if ( i2c0_tx_ptr < 0 ) return -1;
	if ( i2c0_tx_ptr >= i2c0_tx_len )
	{
		return 1;
	}
	return 0;
}

int pqpf_i2c_if1_receive_data( void)			// get received bytes, return -1 if bufer end or returns -2 if no data available
{
	/*uint32_t status = i2c0_hw->raw_intr_stat;
	if((status & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS))// || ((time_us_32() - i2c0_last_event_us) > 10000000))
	{
		i2c0_last_event_us = time_us_32();
		// Master megszakította a tranzakciót (pl. NACK vagy STOP nélkül)
		// Itt lehet resetelni a TX FIFO-t és a belső állapotokat
		i2c0_switch_to_slave_mode();
	}
	if (!(i2c0_hw->status & I2C_IC_STATUS_ACTIVITY_BITS))
	{
		// Busz nyugalmi állapotban van — normál helyzet
	}*/

	if ( i2c0_rx_head == i2c0_rx_tail ) return -1;
	uint32_t tail = i2c0_rx_tail;
	uint8_t data = i2c0_rx_data[tail];
	tail += 1;
	if ( tail >= I2C_RX_BUF_SIZE )
	{
		tail = 0;
	}
	i2c0_rx_tail = tail;
	return data;
}


#endif

#ifdef LIBPQP_HAS_I2C2
static volatile uint8_t i2c1_rx_data[I2C_RX_BUF_SIZE];
static volatile uint32_t i2c1_rx_head, i2c1_rx_tail;
static uint8_t * i2c1_tx_data;
static volatile int i2c1_tx_len, i2c1_tx_ptr;
static int i2c1_write_stm;
static uint8_t i2c1_modeState;		//0 slave 1 master
//static uint32_t i2c1_last_event_us;



uint8_t pqpf_i2c_if2_getModeState()
{
	return i2c1_modeState;
}

int pqpf_i2c_if2_get_write_state()
{
	return i2c1_write_stm;
}



void i2c1_switch_to_master_mode( void )
{
	i2c1_modeState = 1;
	i2c_slave_deinit(i2c1);
	/*irq_set_enabled(I2C1_IRQ, 0);

	i2c1_modeState = 1;

	i2c_slave_deinit(i2c1);
	i2c_deinit(i2c1);
	i2c1_tx_data = NULL;
	i2c1_tx_len = i2c1_tx_ptr = -1;
	i2c1_rx_head = 0;
	i2c1_rx_tail = 0;
	gpio_init(I2C1_I2CI2_GPIO_SDA);
	gpio_init(I2C1_I2CI2_GPIO_SCL);
	i2c1_write_stm = 0;

	//i2c1_last_event_us = time_us_32();

	i2c_init(i2c1, I2C_BITRATE * 1000);
	gpio_set_function( I2C1_I2CI2_GPIO_SDA, GPIO_FUNC_I2C );
	gpio_set_function( I2C1_I2CI2_GPIO_SCL, GPIO_FUNC_I2C );
	gpio_pull_up(I2C1_I2CI2_GPIO_SDA);
	gpio_pull_up(I2C1_I2CI2_GPIO_SCL);
	irq_set_enabled(I2C1_IRQ, 1);*/
}

// i2c slave ISR
static void i2c1_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event)
{
	//i2c1_last_event_us = time_us_32();
	switch (event)
	{
		case I2C_SLAVE_RECEIVE: // master has written some data
			while(i2c_get_hw(i2c)->rxflr)
			{
				uint8_t data = (uint8_t) i2c1_inst.hw->data_cmd;
				uint32_t head = i2c1_rx_head;
				uint32_t head_next = head + 1;
				if ( head_next >= I2C_RX_BUF_SIZE )
				{
					head_next = 0;
				}
				if ( head_next == i2c1_rx_tail )	// ring buffer is full
				{
					if ( head == 0 )
					{
						head = I2C_RX_BUF_SIZE;
					}
					head -= 1;
					i2c1_rx_data[head] = I2C_FRAME_END;	// overwrite last valid entry with FRAME_END ( ~= invalidate)
				}
				else
				{
					i2c1_rx_data[head] = data;
					i2c1_rx_head = head_next;
				}
			}
			break;
		case I2C_SLAVE_REQUEST: // master is requesting data
			//do nothing, because master never request data from slave
			i2c_write_byte_raw(i2c, 0x11);
			break;
		case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
			break;
		default:
			break;
	}
}

void i2c1_switch_to_slave_mode( void )
{
	//working 3 lines
	//i2c1_modeState = 0;
	//i2c_slave_init(i2c1, 42, &i2c1_slave_handler);
	//irq_set_enabled(I2C1_IRQ, 0);
	//irq_set_enabled(I2C1_IRQ, 1);


	//test lines
	irq_set_enabled(I2C1_IRQ, 0);

	//i2c_slave_deinit(i2c1);
	i2c_deinit(i2c1);
	//i2c1_tx_data = NULL;
	//i2c1_tx_len = i2c1_tx_ptr = -1;
	//i2c1_rx_head = 0;
	//i2c1_rx_tail = 0;
	//gpio_init(I2C1_I2CI2_GPIO_SDA);
	//gpio_init(I2C1_I2CI2_GPIO_SCL);
	i2c1_write_stm = 0;
	i2c1_modeState = 0;

	i2c_init(i2c1, I2C_BITRATE);
	//gpio_set_function( I2C1_I2CI2_GPIO_SDA, GPIO_FUNC_I2C );
	//gpio_set_function( I2C1_I2CI2_GPIO_SCL, GPIO_FUNC_I2C );
	//gpio_pull_up(I2C1_I2CI2_GPIO_SDA);
	//gpio_pull_up(I2C1_I2CI2_GPIO_SCL);
	i2c_slave_init(i2c1, 42, &i2c1_slave_handler);

	//i2c1_last_event_us = time_us_32();
	irq_set_enabled(I2C1_IRQ, 1);
}

static int i2c1_write_status(void)
{
	uint32_t abort_reason;
	int loop_guard = 32;

	while (i2c1_write_stm >= 0 && loop_guard > 0)
	{
		loop_guard -= 1;
		switch (i2c1_write_stm)
		{
			case 0:

				if ( i2c1_tx_ptr < (i2c1_tx_len -1) )
				{
					i2c1_inst.hw->data_cmd = i2c1_tx_data[i2c1_tx_ptr];
				}
				else
				{
					i2c1_inst.hw->data_cmd = ( (1 << I2C_IC_DATA_CMD_STOP_LSB)) | (  i2c1_tx_data[i2c1_tx_ptr] );
				}
				i2c1_tx_ptr += 1;
				i2c1_write_stm += 1;
				break;
			case 1:
				if (!(i2c1_inst.hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_EMPTY_BITS)) return 0;
				i2c1_write_stm += 1;
				break;
			case 2:
				abort_reason = i2c1_inst.hw->tx_abrt_source;
				if (abort_reason)
				{
					i2c1_inst.hw->clr_tx_abrt;
					i2c1_write_stm = -1;
					return -1;
				}
				if (i2c1_tx_ptr < i2c1_tx_len)
				{
					i2c1_write_stm = 0;
				}
				else
				{
					i2c1_write_stm += 1;
				}
				break;
			case 3:
				if (!(i2c1_inst.hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_STOP_DET_BITS)) return 0;
				i2c1_inst.hw->clr_stop_det;
				i2c1_write_stm = 4;
				i2c1_tx_data = NULL;
				return 1;
			case 4:
				return 1;
			default:
				return -1;
		}
	}

	return -1;
}

static void i2c1_master_handler()
{
	//i2c1_last_event_us = time_us_32();
	if ( i2c1_tx_data == NULL )
	{
		NVIC_DisableIRQ( I2C1_IRQ );
		irq_remove_handler( I2C1_IRQ, i2c1_master_handler );
		i2c1_switch_to_slave_mode();
		return;
	}
	int tmprtn = i2c1_write_status( );
	if(tmprtn == 1)
	{
		//transmission end
		NVIC_DisableIRQ( I2C1_IRQ );
		irq_remove_handler( I2C1_IRQ, i2c1_master_handler );
		i2c1_switch_to_slave_mode();
	}
	else if(tmprtn == -1)
	{
		//transmission end
		i2c1_tx_ptr = -1;
		NVIC_DisableIRQ( I2C1_IRQ );
		irq_remove_handler( I2C1_IRQ, i2c1_master_handler );
		i2c1_switch_to_slave_mode();
	}
}

void pqpf_i2c_if2_init( void )							// should contain some initialization code, if needed
{
	i2c1_tx_data = NULL;
	i2c1_tx_len = i2c1_tx_ptr = -1;
	i2c1_rx_head = 0;
	i2c1_rx_tail = 0;
	gpio_init(I2C1_I2CI2_GPIO_SDA);
	gpio_init(I2C1_I2CI2_GPIO_SCL);
	i2c1_write_stm = 0;

	i2c_init(i2c1, I2C_BITRATE);
	gpio_set_function( I2C1_I2CI2_GPIO_SDA, GPIO_FUNC_I2C );
	gpio_set_function( I2C1_I2CI2_GPIO_SCL, GPIO_FUNC_I2C );
	gpio_pull_up(I2C1_I2CI2_GPIO_SDA);
	gpio_pull_up(I2C1_I2CI2_GPIO_SCL);

	i2c1_switch_to_slave_mode();
}

void pqpf_i2c_if2_hw_init( void )							// should contain some initialization code, if needed
{
	pqpf_i2c_if2_init();
}

void pqpf_i2c_if2_deinit( void )
{
	NVIC_DisableIRQ( I2C1_IRQ );
	irq_remove_handler( I2C1_IRQ, i2c1_master_handler );

	gpio_deinit( I2C1_I2CI2_GPIO_SDA );
	gpio_deinit( I2C1_I2CI2_GPIO_SCL );
	i2c_slave_deinit(i2c1);
	i2c_deinit(i2c1);
}

bool pqpf_i2c_if2_is_busy( void )						// return true, if there are already trafic on the I2C, or last RX happened in less than 1ms
{
	if ( i2c1_rx_head != i2c1_rx_tail ) return true;			// rx ring buffer not empty
	if(i2c1_inst.hw->status & I2C_IC_STATUS_SLV_ACTIVITY_BITS ) return true;				//slave state machine not in idle state
	if(i2c1_inst.hw->status & I2C_IC_STATUS_MST_ACTIVITY_BITS ) return true;				//master state machine not in idle state
	if(!(i2c1_inst.hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_EMPTY_BITS)) return true;		// still transmitting
	return false;
}

void pqpf_i2c_if2_send_data(uint8_t addr, uint8_t * data, int len)
{
	//gpio_put(PICO_DEFAULT_LED_PIN, 1);
	if ( pqpf_i2c_if2_is_busy( ) )
	{
		i2c1_tx_data = NULL;
		i2c1_tx_len = -1;
		i2c1_tx_ptr = -1;
		return;
	}

	i2c1_switch_to_master_mode();

	i2c_init(&i2c1_inst, I2C_BITRATE);
	i2c1_inst.hw->enable = 0;
	i2c1_inst.hw->tar = addr;
	i2c1_inst.hw->enable = 1;

	i2c1_write_stm = 0;

	i2c1_tx_len = -1;
	i2c1_tx_data = data;
	i2c1_tx_ptr = 0;
	i2c1_tx_len = len;

	//set interrupt callback to i2c1_master_handler
	// Enable the interrupts we want.
	//i2c1->hw->intr_mask = (I2C_IC_INTR_MASK_M_TX_EMPTY_BITS | I2C_IC_INTR_MASK_M_TX_ABRT_BITS | I2C_IC_INTR_MASK_M_STOP_DET_BITS);
	// Set up the interrupt handlers.
	irq_set_exclusive_handler(I2C1_IRQ, i2c1_master_handler);
	//i2c_if1.timer = 2;
	// Enable I2C interrupts.
	NVIC_SetPriority( I2C1_IRQ, 2 );
	NVIC_EnableIRQ( I2C1_IRQ );
}

int pqpf_i2c_if2_send_status( void )						// returns -1 on failure (i2c busy), 0 on busy, 1 on success
{
	if(i2c1_inst.hw->status & I2C_IC_STATUS_MST_ACTIVITY_BITS ) return 0;				//master state machine not in idle state, still transmitting
	if(!(i2c1_inst.hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_EMPTY_BITS)) return 0;		// still transmitting
	if ( i2c1_tx_ptr < 0 ) return -1;
	if ( i2c1_tx_ptr >= i2c1_tx_len )
	{
		return 1;
	}
	return 0;
}

int pqpf_i2c_if2_receive_data( void)			// get received bytes, return -1 if bufer end or returns -2 if no data available
{
	/*uint32_t status = i2c1_hw->raw_intr_stat;
	if((status & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS))// || ((time_us_32() - i2c1_last_event_us) > 10000000))
	{
		// Master megszakította a tranzakciót (pl. NACK vagy STOP nélkül)
		// Itt lehet resetelni a TX FIFO-t és a belső állapotokat
		i2c1_switch_to_slave_mode();
	}
	if (!(i2c1_hw->status & I2C_IC_STATUS_ACTIVITY_BITS))
	{
		// Busz nyugalmi állapotban van — normál helyzet
	}*/

	if ( i2c1_rx_head == i2c1_rx_tail ) return -1;
	uint32_t tail = i2c1_rx_tail;
	uint8_t data = i2c1_rx_data[tail];
	tail += 1;
	if ( tail >= I2C_RX_BUF_SIZE )
	{
		tail = 0;
	}
	i2c1_rx_tail = tail;
	return data;
}


#endif

// HARDWARE SPECIFIC EXTENSIONS



static volatile uint32_t core1_i2c_deinit_request = 0;
void pqphws_core0_deinit_i2c_iterface_request(uint8_t type)
{
	if(type == 0)
	{
		core1_i2c_deinit_request = 0xDDDD;
	}
	else if(type == 1)
	{
		core1_i2c_deinit_request = 0x1111;
	}
}

void pqphws_core0_deinit_i2c_iterface_process( void )
{
	if(core1_i2c_deinit_request == 0xDDDD)
	{
		#ifdef LIBPQP_HAS_I2C1
				pqpf_i2c_if1_deinit();
		#endif
		#ifdef LIBPQP_HAS_I2C2
				pqpf_i2c_if2_deinit();
		#endif
	}
	else if(core1_i2c_deinit_request == 0x1111)
	{
		#ifdef LIBPQP_HAS_I2C1
				pqpf_i2c_if1_init();
		#endif
		#ifdef LIBPQP_HAS_I2C2
				pqpf_i2c_if2_init();
		#endif
	}
}


static volatile uint32_t core1_flash_request = 0;
static volatile uint32_t core1_flash_address, core1_flash_length;
static uint8_t * volatile core1_flash_src_buf = NULL;	// pointer itself is volatile

int pqphws_core1_erase_flash( uint32_t address, uint32_t length )
{
#ifdef LIBPQP_IS_BOOTLOADER
	return -1;
#else
	// check alignment
	if ( address % FLASH_SECTOR_SIZE ) return -1;
	if ( length % FLASH_SECTOR_SIZE ) return -1;
	if ( length == 0 ) return -1;
	if ( address > ( address + length ) ) return -1;	// check rollback
	if ( address < ( XIP_BASE + PROTECTED_DATA_OFFSET ) ) return -1;										// BOOTLOADER AREA
	if ( ( address >= ( XIP_BASE + PROTECTED_DATA_OFFSET ) ) && ( address < ( XIP_BASE + ROUTING_TABLE_OFFSET ) ) )					// PROTECTED_DATA
	{
		if ( ( address + length ) > ( XIP_BASE + ROUTING_TABLE_OFFSET ) ) return -1;
	}
	if ( ( address >= ( XIP_BASE + ROUTING_TABLE_OFFSET ) ) && ( address < ( XIP_BASE + USER_CONFIG_OFFSET ) ) )					// ROUTING_TABLE
	{
		return -1;
	}
	if ( ( address >= ( XIP_BASE + USER_CONFIG_OFFSET ) ) && ( address < ( XIP_BASE + APP_OFFSET ) ) )						// USER_CONFIG
	{
		if ( ( address + length ) > ( XIP_BASE + APP_OFFSET ) ) return -1;
	}
	if ( ( address >= ( XIP_BASE + APP_OFFSET ) ) && ( address < ( XIP_BASE + APP_OFFSET + APP_HEADER_SIZE + pqpf_get_firmware_length( ) ) ) )	// FW AREA
	{
		return -1;
	}
	if ( ( address + length ) > ( XIP_BASE + FLASH_SIZE ) ) return -1;	// check flash range


	core1_flash_address = address;
	core1_flash_length = length;
	core1_flash_request = 0xEEEE;
	do {
		sleep_ms( 1 );
	} while ( core1_flash_request != 0 );
	return 0;
#endif
}

int pqphws_core1_flash_write( uint8_t * src_buf, uint32_t address, int size )
{
#ifdef LIBPQP_IS_BOOTLOADER
	return -1;
#else
	if ( address & 0xFF ) return -1;	// address must be page aligned
	if ( size <= 0 ) return -1;
	if ( size & 0xFF ) return -1;		// size must be multiple of page size
	if ( address > ( address + size ) ) return -1;	// check rollback
	if ( (uintptr_t)src_buf < SRAM_BASE ) return -1;	// src_buf must be in RAM
	if ( address < ( XIP_BASE + PROTECTED_DATA_OFFSET ) ) return -1;										// BOOTLOADER AREA
	if ( ( address >= ( XIP_BASE + PROTECTED_DATA_OFFSET ) ) && ( address < ( XIP_BASE + ROUTING_TABLE_OFFSET ) ) )					// PROTECTED_DATA
	{
		if ( ( address + size ) > ( XIP_BASE + ROUTING_TABLE_OFFSET ) ) return -1;
	}
	if ( ( address >= ( XIP_BASE + ROUTING_TABLE_OFFSET ) ) && ( address < ( XIP_BASE + USER_CONFIG_OFFSET ) ) )					// ROUTING_TABLE
	{
		return -1;
	}
	if ( ( address >= ( XIP_BASE + USER_CONFIG_OFFSET ) ) && ( address < ( XIP_BASE + APP_OFFSET ) ) )						// USER_CONFIG
	{
		if ( ( address + size ) > ( XIP_BASE + APP_OFFSET ) ) return -1;
	}
	if ( ( address >= ( XIP_BASE + APP_OFFSET ) ) && ( address < ( XIP_BASE + APP_OFFSET + APP_HEADER_SIZE + pqpf_get_firmware_length( ) ) ) )	// FW AREA
	{
		return -1;
	}
	if ( ( address + size ) > ( XIP_BASE + FLASH_SIZE ) ) return -1;	// check flash range

	core1_flash_address = address;
	core1_flash_length = size;
	core1_flash_src_buf = src_buf;
	core1_flash_request = 0x3333;
	do {
		sleep_ms( 1 );
	} while ( core1_flash_request != 0 );
	return 0;
#endif
}

void pqphws_core0_flash_process( void )
{
	if ( core1_flash_request == 0xEEEE )
	{
		multicore_lockout_start_blocking( );

		uint32_t address = core1_flash_address;
		uint32_t length = core1_flash_length;
		do {
			if ( address % FLASH_SECTOR_SIZE ) break;
			if ( length % FLASH_SECTOR_SIZE ) break;
			if ( length == 0 ) break;
			if ( address > ( address + length ) ) break;
			if ( address < ( XIP_BASE + PROTECTED_DATA_OFFSET ) ) break;
			if ( ( address + length ) > ( XIP_BASE + FLASH_SIZE ) ) break;

			address -= XIP_BASE;
			for ( int i=0; i<length; i+=FLASH_SECTOR_SIZE )
			{
				watchdog_update( );
				uint32_t ints = save_and_disable_interrupts( );
				flash_range_erase( address + i, FLASH_SECTOR_SIZE );
				restore_interrupts( ints );
			}
			watchdog_update( );
		} while ( 0 );

		core1_flash_request = 0;
		multicore_lockout_end_blocking( );
	} else
	if ( core1_flash_request == 0x3333 )
	{
		multicore_lockout_start_blocking( );
		uint32_t address = core1_flash_address;
		uint32_t length = core1_flash_length;
		uint8_t * src_buf = core1_flash_src_buf;
		do {
			if ( address & 0xFF ) break;
			if ( length & 0xFF ) break;
			if ( length == 0 ) break;
			if ( address > ( address + length ) ) break;
			if ( address < ( XIP_BASE + PROTECTED_DATA_OFFSET ) ) break;
			if ( ( address + length ) > ( XIP_BASE + FLASH_SIZE ) ) break;
			if ( (uintptr_t)src_buf < SRAM_BASE ) break;

			address -= XIP_BASE;
			watchdog_update( );
			uint32_t ints = save_and_disable_interrupts( );
			flash_range_program( address, src_buf, length );
			restore_interrupts( ints );
		} while ( 0 );

		core1_flash_request = 0;
		multicore_lockout_end_blocking( );
	}
}

static void __not_in_flash_func( custom_hardfault_handler )( void )
{
#ifdef LIBPQP_IS_BOOTLOADER
	watchdog_hw->scratch[0] = 0xEEC7BCFF;
#else
	if ( 0 == get_core_num() )
	{
		watchdog_hw->scratch[0] = 0xEEC7AC00;
	}
	else
	{
		watchdog_hw->scratch[0] = 0xEEC7AC11;
	}
#endif
	watchdog_reboot(0, 0, 0);
	while(1)
	{
		;
	}
}

void pqphws_register_fault_handler( void )
{
#ifdef LIBPQP_IS_BOOTLOADER
	original_fault_handler =
#endif
	exception_set_exclusive_handler( HARDFAULT_EXCEPTION , custom_hardfault_handler );
}




