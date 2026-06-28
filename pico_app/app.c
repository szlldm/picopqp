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
#include "libpqp/libpqp.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "main.h"
#include "libpqp_hws.h"

// Main (runs on core 0)
int main()
{
	watchdog_enable( 0x7fffff, true );	// set to the maximum 8.3 seconds; pause on debug
	watchdog_update( );
	pqphws_register_fault_handler( );
	
	// Pico USB serial init:
	#if defined(LIBPQP_HAS_STDIOIF)
	stdio_init_all();
	stdio_set_translate_crlf(&stdio_usb, false);
	#endif
	
	// My initializations
	// ...
	
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
	
	
	multicore_launch_core1( main_core1 );
	
	while (1)
	{
		pqp_process();
		
		pqphws_core0_flash_process( );
		
		watchdog_update( );
	}
}
