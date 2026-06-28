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
const char * arguments_list = "[payload length] <repeat>";
const int arguments_required_num = 1;
/////////////////////////////////////


int app_main( int argc, char** argv )
{
	int size = strtol( argv[0], NULL, 10 );
	int repeat = 1;
	if ( argc > 1 )
	{
		repeat = strtol( argv[1], NULL, 10 );
	}
	if ( repeat < 1 ) repeat = 1;
	int r = -1;
	for ( int i = 0; i < repeat; i++ )
	{
		r = transfer_test( size, ( i == 0 ), DEFAULT_TIMEOUT );						// returns -1 on error/timeout, 0 on success
		fprintf( stderr, "Transfer test %s\n", ( r < 0 ) ? "failed" : "OK" );
		if ( r < 0 ) break;
	}
	
	return r;
}
