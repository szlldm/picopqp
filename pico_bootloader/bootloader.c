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

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/uart.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "libpqp/libpqp.h"
#include "libpqp/libpqp_foreign.h"
#include "libpqp_hws.h"
#include "memmap.h"

#define FIRMWARE_AUTOSTART_MS		(3000)
#define FIRMWARE_START_LATCH_TIMEOUT_MS	(3600000)	// 1 hour
#define CLONE_OFFSET			(0x00100000)

// Main (runs on core 0)
int main()
{
	pqphws_register_fault_handler( );
	
	// Pico USB serial init:
	#if defined(LIBPQP_HAS_STDIOIF)
	stdio_init_all();
	stdio_set_translate_crlf(&stdio_usb, false);
	#endif
	
	uint8_t base_address = MY_BASE_ADDRESS;
	#if defined(ADDRESS_MODIFIER_ID_PIN)
	gpio_init(ADDRESS_MODIFIER_ID_PIN);
	gpio_set_dir(ADDRESS_MODIFIER_ID_PIN, GPIO_IN);
	if ( gpio_get(ADDRESS_MODIFIER_ID_PIN) )
	{
		base_address += 1;
	}
	#endif
	pqp_init(base_address);
	
	watchdog_enable( 0x7fffff, true );	// set to the maximum 8.3 seconds; pause on debug
	
	// FW update from clone area
	if ( pqp_bootloader_latch_code( ) == PQP_BLDR_CODE_FIRMWARE_UPDATE )
	{
		uint16_t block_count = pqpf_check_valid_clone_firmware( XIP_BASE + CLONE_OFFSET );
		if ( block_count > 0 )
		{
			uint32_t erase_size = ( ( ( block_count * 256ul + 4095 ) / 4096 ) * 4096);
			pqpf_firmware_erase_section( XIP_BASE + APP_OFFSET, erase_size );
			pqpf_clone_local_firmware( block_count, XIP_BASE + APP_OFFSET, XIP_BASE + CLONE_OFFSET );
			pqpf_reboot( 0 );
		}
	}
	
	
	uint32_t start_ms = pgpf_get_ms_timestamp( );
	uint32_t bldr_timeout_ms = pgpf_get_ms_timestamp( );
	
	// My initializations
	// ...

	while (1)
	{
		pqp_process();
		
		if ( pqp_received_packet_available( ) )
		{
			pqp_release_received_packet( pqp_take_received_packet( ) );
		}
		
		if ( ( pgpf_get_ms_timestamp( ) - start_ms ) > FIRMWARE_AUTOSTART_MS )
		{
			if ( pqp_bootloader_latch_code( ) != PQP_BLDR_CODE_BOOTLOADER_LATCH )
			{
				if ( pqpf_check_valid_firmware( ) )
				{
					pqpf_start_firmware( );
				}
			}
			start_ms = pgpf_get_ms_timestamp( );
		}
		
		if ( ( pgpf_get_ms_timestamp( ) - bldr_timeout_ms ) > FIRMWARE_START_LATCH_TIMEOUT_MS )
		{
			if ( pqpf_check_valid_firmware( ) )
			{
				pqpf_start_firmware( );
			}
			bldr_timeout_ms = pgpf_get_ms_timestamp( );
		}

		watchdog_update( );
	}
}
