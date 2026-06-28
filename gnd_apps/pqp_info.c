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
const char * arguments_list = "";
const int arguments_required_num = 0;
/////////////////////////////////////

#define INDENT		"    "

void print_CAN_info( uint8_t * data, int len )
{
	if ( len != 30 )
	{
		fprintf( stderr, INDENT "invalid data\n" );
		return;
	}
	uint32_t cntr;
	memcpy( &cntr, data + 0, 4 );
	fprintf( stderr, INDENT "TX counter: %u packets sent\n", cntr );
	memcpy( &cntr, data + 4, 4 );
	fprintf( stderr, INDENT "TX unstuck counter: %u attempts\n", cntr );
	memcpy( &cntr, data + 8, 4 );
	fprintf( stderr, INDENT "RX counter: %u packets with valid CRC\n", cntr );
	memcpy( &cntr, data + 12, 4 );
	fprintf( stderr, INDENT "RX loss counter: %u packets\n", cntr );
	memcpy( &cntr, data + 16, 4 );
	fprintf( stderr, INDENT "RX lost fragment counter: %u packets lost\n", cntr );
	uint16_t u16, u16_2;
	memcpy( &u16, data + 20, 2 );
	fprintf( stderr, INDENT "ROGUE sender counter: %u\n", u16 );
	fprintf( stderr, INDENT "Packets waiting to be transmitted: %u\n", data[22] );
	memcpy( &u16, data + 23, 2 );
	memcpy( &u16_2, data + 25, 2 );
	fprintf( stderr, INDENT "RX fragment buffer usage: %u/%u  (%.1f%%)\n", ( u16_2 - u16 ), u16_2, 100.0 * (float)( u16_2 - u16 ) / (float)u16_2 );
	fprintf( stderr, INDENT "RX packet buffer usage: %u complete, %u unfinished / %u total\n", data[27], data[28], data[29] );
}

void print_HDUART_info( uint8_t * data, int len )
{
	if ( len != 21 )
	{
		fprintf( stderr, INDENT "invalid data\n" );
		return;
	}
	uint32_t cntr;
	memcpy( &cntr, data + 0, 4 );
	fprintf( stderr, INDENT "TX counter: %u packets sent\n", cntr );
	memcpy( &cntr, data + 4, 4 );
	fprintf( stderr, INDENT "TX collision counter: %u packets\n", cntr );
	memcpy( &cntr, data + 8, 4 );
	fprintf( stderr, INDENT "RX counter: %u packets with valid CRC\n", cntr );
	memcpy( &cntr, data + 12, 4 );
	fprintf( stderr, INDENT "RX counter: %u packets with INVALID CRC\n", cntr );
	memcpy( &cntr, data + 16, 4 );
	fprintf( stderr, INDENT "RX error counter: %u frames\n", cntr );
	switch ( data[20] )
	{
		case 0:
			fprintf( stderr, INDENT "Current TX state: NOP\n" );
			break;
		case 1:
			fprintf( stderr, INDENT "Current TX state: WAIT_FOR_LINE\n" );
			break;
		case 2:
			fprintf( stderr, INDENT "Current TX state: UNDER_SEND\n" );
			break;
		default:
			fprintf( stderr, INDENT "Current TX state: INVALID VALUE\n" );
			break;
	}
}

void print_I2C_info( uint8_t * data, int len )
{
	if ( len != 26 )
	{
		fprintf( stderr, INDENT "invalid data\n" );
		return;
	}
	uint32_t cntr;
	memcpy( &cntr, data + 0, 4 );
	fprintf( stderr, INDENT "TX counter: %u packets sent\n", cntr );
	memcpy( &cntr, data + 4, 4 );
	fprintf( stderr, INDENT "TX collision counter: %u packets\n", cntr );
	memcpy( &cntr, data + 8, 4 );
	fprintf( stderr, INDENT "RX counter: %u packets with valid CRC\n", cntr );
	memcpy( &cntr, data + 12, 4 );
	fprintf( stderr, INDENT "RX counter: %u packets with INVALID CRC\n", cntr );
	memcpy( &cntr, data + 16, 4 );
	fprintf( stderr, INDENT "RX error counter: %u frames\n", cntr );
	switch ( data[20] )
	{
		case 0:
			fprintf( stderr, INDENT "Current TX state: NOP\n" );
			break;
		case 1:
			fprintf( stderr, INDENT "Current TX state: WAIT_FOR_LINE\n" );
			break;
		case 2:
			fprintf( stderr, INDENT "Current TX state: UNDER_SEND\n" );
			break;
		default:
			fprintf( stderr, INDENT "Current TX state: INVALID VALUE\n" );
			break;
	}
	switch(data[21])
	{
		case 0:
			fprintf( stderr, INDENT "Current mode state: SLAVE\n" );
			break;
		case 1:
			fprintf( stderr, INDENT "Current mode state: MASTER\n" );
			break;
		default:
			fprintf( stderr, INDENT "Current mode state: INVALID VALUE\n" );
			break;
	}
	int i2c_write_stm_state = 0;
	memcpy( &i2c_write_stm_state, data + 22, 4 );
	fprintf( stderr, INDENT "Write state machine state: %d \n", i2c_write_stm_state );
}

void print_RAW_info( uint8_t * data, int len )
{
	if ( len != 12 )
	{
		fprintf( stderr, INDENT "invalid data\n" );
		return;
	}
	uint32_t cntr;
	memcpy( &cntr, data + 0, 4 );
	fprintf( stderr, INDENT "TX counter: %u packets\n", cntr );
	memcpy( &cntr, data + 4, 4 );
	fprintf( stderr, INDENT "RX counter: %u packets\n", cntr );
	memcpy( &cntr, data + 8, 4 );
	fprintf( stderr, INDENT "RX counter: %u packets with valid CRC\n", cntr );
}

void print_USB_info( uint8_t * data, int len )
{
	if ( len != 8 )
	{
		fprintf( stderr, INDENT "invalid data\n" );
		return;
	}
	uint32_t cntr;
	memcpy( &cntr, data + 0, 4 );
	fprintf( stderr, INDENT "TX counter: %u packets\n", cntr );
	memcpy( &cntr, data + 4, 4 );
	fprintf( stderr, INDENT "RX counter: %u packets\n", cntr );
}

void print_SYS_clks( uint8_t * data, int len )
{
	if ( len != 32 )
	{
		fprintf( stderr, INDENT "invalid data\n" );
		return;
	}
	uint32_t sysclk;
}


#define ERROR()		do { fprintf( stderr, "INFO failed %d\n", __LINE__ );  return -1; } while (0)

int app_main( int argc, char** argv )
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];
	
	set_port( MGMT_PORT_EXTENSIONS );
	
	memcpy( trx_dat, "INFO", 4 );
	send_data( trx_dat, 4 );
	memset( trx_dat, 0, PQP_MAX_PAYLOAD );
	
	int r = receive_data( trx_dat, false, DEFAULT_TIMEOUT );
	if ( r < 5 ) ERROR();
	if ( memcmp( trx_dat, "INFO", 4 ) ) ERROR();
	int group = 0;
	int pos = 4;
	int len;
	while ( pos < r )
	{
		len = trx_dat[pos];
		if ( ( pos + 1 + len ) > r ) ERROR();
		pos += 1;
		switch( group )
		{
			case 0:	// hw info
				if ( len != 2 ) ERROR();
				uint8_t reset = trx_dat[pos + 0];
				switch ( reset )
				{
					case 'S': fprintf( stderr, "Reset cause: Power supply (POR)\n" ); break;
					case 'B': fprintf( stderr, "Reset cause: Power supply (BOR)\n" ); break;
					case 'P': fprintf( stderr, "Reset cause: Run pin\n" ); break;
					case 'G': fprintf( stderr, "Reset cause: Power supply glitch\n" ); break;
					case 'W': fprintf( stderr, "Reset cause: Watchdog\n" ); break;
					case 'H': fprintf( stderr, "Reset cause: Hazard\n" ); break;
					case 'T': fprintf( stderr, "Reset cause: Switched core powerdown\n" ); break;
					case 'L': fprintf( stderr, "Reset cause: Rescue reset\n" ); break;
					case 'D': fprintf( stderr, "Reset cause: Debug port\n" ); break;
					case 'R': fprintf( stderr, "Reset cause: Rebooted\n" ); break;
					case 'C': fprintf( stderr, "Reset cause: Watchdog or crash\n" ); break;
					case '/': fprintf( stderr, "Reset cause: Bootloader Core0 hardfault\n" ); break;
					case '0': fprintf( stderr, "Reset cause: App Core0 hardfault\n" ); break;
					case '1': fprintf( stderr, "Reset cause: App Core1 hardfault\n" ); break;
					default:  fprintf( stderr, "Reset cause: UNKNOWN (WTF moment...)\n" ); break;
				}
				uint8_t flags = trx_dat[pos + 1];
				fprintf( stderr, "Core voltage: %s\n", ( ( flags & 0x01 ) ? "OK" : "UNSTABLE" ) );
				fprintf( stderr, "SYS-PLL: %s\n", ( ( flags & 0x02 ) ? "LOCKED" : "UNLOCKED" ) );
				fprintf( stderr, "USB-PLL: %s\n", ( ( flags & 0x04 ) ? "LOCKED" : "UNLOCKED" ) );
				break;
			case 1:	// build date
				if ( len != 11 ) ERROR();
				char date[12];
				memcpy( date, trx_dat + pos, 11 );
				date[11] = 0;
				fprintf( stderr, "Build date: %s\n", date );
				break;
			case 2:	// build time
				if ( len != 8 ) ERROR();
				char time[9];
				memcpy( time, trx_dat + pos, 8 );
				time[8] = 0;
				fprintf( stderr, "Build time: %s\n", time );
				break;
			case 3:	// fw vagy bldr git version
				if ( len != 4 ) ERROR();
				uint32_t main_git_version = 0;
				memcpy( &main_git_version, trx_dat + pos, 4 );
				fprintf( stderr, "Program git version: 0x%08X\n", main_git_version);
				break;
			case 4:	// pqp git version
				if ( len != 4 ) ERROR();
				uint32_t pqp_git_version = 0;
				memcpy( &pqp_git_version, trx_dat + pos, 4 );
				fprintf( stderr, "PQP git version: 0x%08X\n", pqp_git_version );
				break;
			case 5:	// buffer usage
				if ( len != 4 ) ERROR();
				fprintf( stderr, "TX buffer usage: %u/%u  (%.1f%%)\n", trx_dat[pos+0], trx_dat[pos+1], 100.0 * (float)trx_dat[pos+0] / (float)trx_dat[pos+1] );
				fprintf( stderr, "RX buffer usage: %u/%u  (%.1f%%)\n", trx_dat[pos+2], trx_dat[pos+3], 100.0 * (float)trx_dat[pos+2] / (float)trx_dat[pos+3] );
				break;
			default: // interface info
				if ( len < 3 ) ERROR();
				if ( memcmp( trx_dat + pos, "CAN", 3 ) == 0 )
				{
					fprintf( stderr, "CAN interface: \n" );
					print_CAN_info( trx_dat + pos + 3, len - 3 );
				} else
				if ( memcmp( trx_dat + pos, "HDU", 3 ) == 0 )
				{
					fprintf( stderr, "HDUART interface: \n" );
					print_HDUART_info( trx_dat + pos + 3, len - 3 );
				} else
				if ( memcmp( trx_dat + pos, "I2C", 3 ) == 0 )
				{
					fprintf( stderr, "I2C interface: \n" );
					print_I2C_info( trx_dat + pos + 3, len - 3 );
				} else
				if ( memcmp( trx_dat + pos, "RAW", 3 ) == 0 )
				{
					fprintf( stderr, "RAW interface: \n" );
					print_RAW_info( trx_dat + pos + 3, len - 3 );
				}
				else
				if ( memcmp( trx_dat + pos, "USB", 3 ) == 0 )
				{
					fprintf( stderr, "USB interface: \n" );
					print_USB_info( trx_dat + pos + 3, len - 3 );
				}
				else
				if ( memcmp( trx_dat + pos, "SYS", 3 ) == 0 )
				{
					fprintf( stderr, "SYS CLOCKS: \n" );
					print_SYS_clks( trx_dat + pos + 3, len - 3 );
				}
				else
				{
					char ifname[4];
					memcpy( ifname, trx_dat + pos, 3 );
					ifname[3] = 0;
					fprintf( stderr, "Unknown interface: %s\n", ifname );
				}
				break;
		}
		pos += len;
		group += 1;
	}
	
	
	
	return 0;
}
