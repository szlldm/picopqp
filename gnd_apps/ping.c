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
const char * arguments_list = "<-c : get checksums>";
const int arguments_required_num = 0;
/////////////////////////////////////

int app_main( int argc, char** argv )
{
	bool get_checksums = false;
	for ( int i = 0; i < argc; i++ )
	{
		if ( strcmp( argv[i], "-c" ) == 0 )
		{
			get_checksums = true;
		}
	}
	uint32_t uptime;
	int r = ping( &uptime, DEFAULT_TIMEOUT );						// returns -1 on error/timeout, 0 if bootloader, 1 if firmware
	
	if ( r < 0 )
	{
		fprintf( stderr, "PING failed\n" );
		return -1;
	}
	
	fprintf( stderr, "PING OK\n%s since %u seconds\n", ( r > 0 ) ? "Firmware" : "Bootloader", uptime );
	
	if ( get_checksums )
	{
		uint32_t bootloader_chksum, fw_chksum, stored_chksum, fw_len;
		int r = get_chksum( &bootloader_chksum, &fw_chksum, &stored_chksum, &fw_len, 10*SEC );		// returns -1 on error/timeout, 0 on success
		if ( r < 0 )
		{
			fprintf( stderr, "Checksum querry failed\n" );
			return -1;
		}
		fprintf( stderr, "Bootloader checksum: 0x%08X\n", bootloader_chksum );
		int32_t fw_len_signed = (int32_t)fw_len;
		if ( fw_len_signed < 0 )
		{
			fprintf( stderr, "Firmware length:     EMPTY (%d)\n", fw_len_signed );
			fprintf( stderr, "Firmware checksum:   0x%08X vs 0x%08X\n", fw_chksum, stored_chksum );
		}
		else
		{
			fprintf( stderr, "Firmware length:     %u\n", fw_len );
			fprintf( stderr, "Firmware checksum:   0x%08X %s 0x%08X\t(calculated %s stored%s)\n", fw_chksum, (fw_chksum == stored_chksum) ? "==" : "!=", stored_chksum, (fw_chksum == stored_chksum) ? "==" : "vs", (fw_chksum == stored_chksum) ? "" : " MISMATCH!" );
		}
		
	}
	
	return 0;
}
