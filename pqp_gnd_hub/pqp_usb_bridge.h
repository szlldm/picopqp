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

#ifndef PQP_USB_BRIDGE_H
#define PQP_USB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>


#define USB_DEVICE	"/dev/ttyACM0"
#define RX_BUFFER_SIZE	(256)

void init_pqp_usb_bridge( int argc, char** argv );
void pqp_usb_bridge_process( void );			// must be periodically called

// THESE MUST BE IMPLEMENTED SOMEWHERE ELSE !!
int pqp_usb_bridge_raw_get_packet( uint8_t * data );			// returns the number of bytes of the raw packet, or -1 on error / no packet
int pqp_usb_bridge_raw_add_packet( uint8_t * data, int data_len );	// returns 0 if raw packet taken, or -1 if cannot take at the moment (try later)

#endif // PQP_USB_BRIDGE_H
