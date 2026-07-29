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
const char * arguments_list = "[LED status; 0 or 1]";
const int arguments_required_num = 1;
/////////////////////////////////////

int app_main( int argc, char** argv )
{
	char led_value = argv[0][0];
	set_port(55);
	printf("%c\n",led_value);
	send_data( &led_value, 1 );
	char buf[512];
	int rxlen = receive_data(buf, false, DEFAULT_TIMEOUT);
	if (rxlen > 0)
	{
		buf[rxlen] = 0;
		printf("%s",buf);
		fflush(stdout);
	}
	return 0;
}

