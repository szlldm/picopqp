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

#include "gnd_app_base.h"

// argument descriptors
const char * arguments_list = NULL;
const int arguments_required_num = 0;
/////////////////////////////////////

#define XIP_BASE 		(0x10000000UL)
#define APP_OFFSET		98304	// 96k
#define APP_HEADER_SIZE		( 256 )

int app_main( int argc, char** argv )
{
	int r;
	for ( int i = 0; i < 16; i++ )
	{
		uint32_t uptime;
		r = ping( &uptime, DEFAULT_TIMEOUT );
		if ( r < 0 )
		{
			fprintf( stderr, "PING failed\n" );
			return -1;
		}
		if ( r != 0 )
		{
			fprintf( stderr, "Target running firmware, can't erase\n" );
			return -1;
		}
		if ( uptime <= 5 )
		{
			fprintf( stderr, "PING OK, wait for uptime > 5 ...\n" );
			sleep( 1 );
			continue;
		}
		fprintf( stderr, "PING OK, in bootloader mode\n" );
		break;
	}
	
	uint32_t bootloader_chksum, fw_chksum, stored_chksum, fw_len;
	r = get_chksum( &bootloader_chksum, &fw_chksum, &stored_chksum, &fw_len, DEFAULT_CHECKSUM_TIMEOUT );		// returns -1 on error/timeout, 0 on success
	if ( r < 0 )
	{
		fprintf( stderr, "Failed to get checksum\n" );
		return -1;
	}
	if ( ( fw_chksum == stored_chksum ) && ( fw_len != 0xFFFFFFFF ) )
	{
		fprintf( stderr, "Found valid firmware, erasing...\n" );
		if ( erase_firmware( DEFAULT_ERASE_TIMEOUT ) < 0 )
		{
			fprintf( stderr, "Erase failed\n" );
			return -1;
		}
	}
	else
	{
		fprintf( stderr, "Not found valid firmware\n" );
		bool do_erase = false;
		while ( !do_erase )
		{
			fprintf( stderr, "Needs full flash erase:\t(Y/n)\n" );
			char buf[4];
			buf[0] = 0;
			if ( buf == fgets( buf, 4, stdin ) )
			{
				do_erase = ( buf[0] == 'Y' );
				if ( ( buf[0] == 'n' ) || ( buf[0] == 'N' ) )
				{
					fprintf( stderr, "Interrupted by the user\n" );
					return 0;
				}
			}
		}
		fprintf( stderr, "Erasing flash... Might take a long time!\n" );
		if ( erase_all( DEFAULT_ERASE_TIMEOUT ) < 0 )
		{
			fprintf( stderr, "Erase failed\n" );
			return -1;
		}
	}
	
	fprintf( stderr, "Erase operation DONE!\n" );
	return 0;
}
