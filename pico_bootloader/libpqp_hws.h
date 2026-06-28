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

// LibPQP hardware specific functions
//
// these are not part of LibPQP
//

#ifndef LIBPQP_HWS_H
#define LIBPQP_HWS_H

#include <stdint.h>
#include <stdbool.h>
#include "pqp_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

int pqphws_core1_erase_flash( uint32_t address, uint32_t length );		// address and length must be FLASH_SECTOR_SIZE aligned; returns -1 if failed, 0 if succeeded
int pqphws_core1_flash_write( uint8_t * src_buf, uint32_t address, int size );	// address and size must be 256 byte aligned; src_buf MUST be in RAM; returns -1 if failed, 0 if succeeded

void pqphws_core0_flash_process( void );

void pqphws_core0_deinit_i2c_iterface_request(uint8_t type);
void pqphws_core0_deinit_i2c_iterface_process( void );

void pqphws_register_fault_handler(void);


#ifdef __cplusplus
}
#endif

#endif // LIBPQP_HWS_H
