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

#ifndef PQP_HUB_H
#define PQP_HUB_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include "libpqp_defines.h"
#include "pqp_addresses.h"

#define TCP_DEFAULT_IP			"127.0.0.1"					// can be override during runtime via the PQP_HUB_IP environmental variable
#define TCP_PORT_OFFSET			10000						// can be override during runtime via the PQP_HUB_PORT_BASE environmental variable. The HUB must use the same port base!


#define SEC				(1000)	// ms
#define DEFAULT_TIMEOUT			(5*SEC)
#define DEFAULT_WRITE_TIMEOUT		(10*SEC)
#define DEFAULT_CHECKSUM_TIMEOUT	(10*SEC)
#define DEFAULT_ERASE_TIMEOUT		(30*SEC)

int receive_data( uint8_t * data, bool return_on_errors, int timeout_ms );		// data must be at least 512 bytes; negative timeout is wait forever; return received bytes, or negative on error / timeout
void send_data( uint8_t * data, int data_len );
void set_port( uint8_t dst_port );
void set_priority( uint8_t priority );
void set_flag( uint8_t flag );
void set_ttl( uint8_t ttl );

// helpers
int convert_arg_to_addr( const char * arg );
uint8_t get_dst_addr( void );
bool user_yesno( const char * msg );
bool get_stop_flag( void );
char getch( void );

// functions for management ports
int ping( uint32_t * uptime, int timeout_ms );						// returns -1 on error/timeout, 0 if bootloader, 1 if firmware
int transfer_test( int size, bool first, int timeout_ms );				// returns -1 on error/timeout, 0 on success
int reboot( void );									// returns -1 on error/timeout, 0 on success
int bootloader_latch( int timeout_ms );							// returns -1 on error/timeout, 0 on success
int bootloader_fwupd( void );								// returns -1 on error
int get_chksum( uint32_t * bootloader_chksum, uint32_t * fw_chksum, uint32_t * stored_chksum, uint32_t * fw_len, int timeout_ms );		// returns -1 on error/timeout, 0 on success
int get_section_chksum( uint32_t * chksum, uint32_t start_addr, uint32_t len, int timeout_ms );							// returns -1 on error/timeout, 0 on success
int get_section_erased( bool * map, uint32_t start_addr, uint32_t len, int timeout_ms );							// returns -1 on error/timeout, else number of bits; ;	map should be a 2048 size array
int run_fw( void );									// returns -1 on error/timeout, 0 on success
int erase_firmware( int timeout_ms );							// returns -1 on error/timeout, 0 on success
int erase_all( int timeout_ms );							// returns -1 on error/timeout, 0 on success
int erase_section( uint32_t start_addr, uint32_t len, int timeout_ms );			// returns -1 on error/timeout, 0 on success
int clear_progmap( int timeout_ms );							// returns -1 on error/timeout, 0 on success
int get_progmap( bool * map, int timeout_ms );						// returns -1 on error/timeout, 0 on success;	map should be a 512 size array
int fw_write( uint32_t addr, uint8_t * block, bool silent, int timeout_ms );		// returns -1 on error/timeout, 0 on success;
int fw_clone_start( uint8_t target_device, bool skip_ff, uint16_t block_count, uint32_t dst_write_addr, uint32_t src_read_addr, int timeout_ms );	// returns -1 on error/timeout, 0 on success;
int fw_clone_abort( int timeout_ms );							// returns -1 on error/timeout, 0 on success;
int fw_clone_status( int timeout_ms );							// returns -1 on error/timeout, >= 0 remaining block count (0 == done / not in progress)
int fw_clone_set_outgoing_interface( uint8_t target_device, uint8_t interface, int timeout_ms );	// returns -1 on error/timeout, 0 on success;
int clear_routing_table( int timeout_ms );						// returns -1 on error/timeout, 0 on success;
int get_routing_table_checksum( uint32_t * chksum, int timeout_ms );			// returns -1 on error/timeout, 0 on success;
int set_routing_table_entries( uint8_t * entries, int num_of_entries, int timeout_ms );	// returns -1 on error/timeout, 0 on success;	entries should be a N*3 byte array


// this must be implemented in the application:
int app_main( int argc, char** argv );

#endif // PQP_HUB_H
