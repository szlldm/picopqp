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

void * rx_thread_function(void * arg)
{
	char buf[512];
	while ( !get_stop_flag( ) )
	{
		int rxlen = receive_data(buf,true,20);
		if (rxlen > 0)
		{
			buf[rxlen] = 0;
			printf("%s",buf);
			fflush(stdout);
		}
	}
	pthread_exit(NULL);
}


int app_main( int argc, char** argv )
{
	set_port( MGMT_PORT_TTY );
	char buf[1024];
	
	pthread_t rx_thread;
	pthread_create(&rx_thread, NULL, rx_thread_function, NULL);
	
	while ( fgets( buf, sizeof buf, stdin ) != NULL )
	{
		strtok( buf, "\r\n" );
		int len = strlen(buf);
		if ( len > PQP_MAX_PAYLOAD ) return -1;
		send_data( buf, len );
	}
	return 0;
}

