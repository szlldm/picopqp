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

#define FIRMWARE_AUTOSTART_MS	(3000)

// Main (runs on core 0)
int main()
{
	// Pico USB serial init:
	#if defined(LIBPQP_HAS_STDIOIF)
	stdio_init_all();
	stdio_set_translate_crlf(&stdio_usb, false);
	#endif
	
	
	// Pico LED blinking
	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
	gpio_put(PICO_DEFAULT_LED_PIN, 1);
	sleep_ms(100);
	gpio_put(PICO_DEFAULT_LED_PIN, 0);
	sleep_ms(100);
	gpio_put(PICO_DEFAULT_LED_PIN, 1);
	sleep_ms(100);
	gpio_put(PICO_DEFAULT_LED_PIN, 0);
	
	uint8_t base_address = MY_BASE_ADDRESS;
	pqp_init(base_address);
	
	watchdog_enable( 0x7fffff, true );	// set to the maximum 8.3 seconds; pause on debug
	
	uint32_t start_ms = pgpf_get_ms_timestamp( );
	uint32_t blink_ms = pgpf_get_ms_timestamp( );
	uint8_t blink = 0;
	
	// My initializations
	// ...
	gpio_init(22);
	gpio_set_dir(22, GPIO_OUT);
	gpio_put(22, 1);

	gpio_init(10);
	gpio_set_dir(10, GPIO_OUT);
	gpio_put(10, 0);

	while (1)
	{
		pqp_process();
		
		if ( pqp_received_packet_available( ) )
		{
			pqp_release_received_packet( pqp_take_received_packet( ) );
			gpio_put( PICO_DEFAULT_LED_PIN, !gpio_get( PICO_DEFAULT_LED_PIN ) );
		}
		
		// ...
		watchdog_update( );
	}
}
