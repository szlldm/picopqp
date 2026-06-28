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

#include "main.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "libpqp/libpqp.h"
#include "libpqp_hws.h"

#define TTY_PORT	(31)

void main_core1( void )
{
	// do not write code before this function call!
	multicore_lockout_victim_init();
	
	uint8_t rdata[PQP_MAX_PAYLOAD];
	uint8_t tdata[PQP_MAX_PAYLOAD];
	uint8_t dst_addr, src_addr,dst_port, src_port;
	pqp_prio_t priority;
	
	/*
	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
	*/
	
	while (1)
	{
		/*
		gpio_put(PICO_DEFAULT_LED_PIN, 1);
		sleep_ms(250);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(250);
		gpio_put(PICO_DEFAULT_LED_PIN, 1);
		sleep_ms(250);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(250);
		gpio_put(PICO_DEFAULT_LED_PIN, 1);
		sleep_ms(750);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(500);
		*/
		
		int rxlen = pqp_recv( rdata, &dst_addr , &src_addr, &dst_port, &src_port, &priority );
		if (rxlen >= 0)
		{
			if (dst_addr != LIBPQP_BROADCAST_ADDRESS)
			{
				int txlen = -1;
				if (dst_port == TTY_PORT)
				{
					if (rxlen >= 1)
					{
						switch(rdata[0])
						{
							case 'h':
								txlen = sprintf(tdata,"h help\n");
								break;
							default:
								break;
						}
					}
				} else
				if (dst_port == 100)
				{
					txlen = sprintf(tdata,"Hello world!\n");
				}
				
				if (txlen >= 0)
				{
					pqp_send( tdata, txlen, src_addr, src_port, dst_port, priority );	// ports are swapped
				}
			}
		}
		
		// !!!!!
		// for flash erase & write USE ONLY the following functions:
		// int pqphws_core1_erase_flash( uint32_t address, uint32_t length );			// address and length must be FLASH_SECTOR_SIZE aligned; returns -1 if failed, 0 if succeeded
		// int pqphws_core1_flash_write( uint8_t * src_buf, uint32_t address, int size );	// address and size must be 256 byte aligned; src_buf MUST be in RAM; returns -1 if failed, 0 if succeeded

	}
}
