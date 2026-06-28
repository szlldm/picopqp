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

#define _GNU_SOURCE

#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <stdarg.h>
#include <signal.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>


#include "pqp_addresses.h"
#include "gnd_app_base.h"

static int dst_addr = -1;
static const char * ip_addr = TCP_DEFAULT_IP;
static int sockfd = -1;
static uint8_t send_buf[1024];
static uint8_t read_buf[1024];
static int readbuf_pos = 0;
static bool verbose_mode = false;
static _Atomic bool stop_flag = false;

// argument descriptors
extern const char * arguments_list;
extern const int arguments_required_num;
/////////////////////////////////////


static void error_print_usage( const char * pn )
{
	char pl[1024];
	pl[0] = 0;
	if ( arguments_required_num > 0 )
	{
		strcpy( pl, " " );
		for ( int i = 0;  i < arguments_required_num; i++ )
		{
			char arg[64];
			sprintf( arg, "param_%d", i + 1 );
			strcat( pl, arg );
		}
	}
	fprintf( stderr, "argument error\n" );
	fprintf( stderr, "Usage: %s [address / device] <--verbose> %s\n", pn, ( arguments_list != NULL ) ? arguments_list : "" );
	fprintf( stderr, "examples:\n" );
	fprintf( stderr, "\t%s 123%s\n", pn, pl );
	fprintf( stderr, "\t%s OBC0%s\n", pn, pl );
	fprintf( stderr, "\t%s com1%s\n", pn, pl );
}

static void terminate( const char * status )
{
	fprintf( stderr, "Connection broken (%s), exit...\n", status );
	exit( -1 );
}

int receive_data( uint8_t * data, bool return_on_errors, int timeout_ms )		// data must be at least 512 bytes; negative timeout is wait forever; return received bytes, or negative on error / timeout
{
	int timeout_cntr = 0;
	while ( 1 )
	{
		if ( readbuf_pos > 768 ) terminate( "RX overflow" );		// too long
		int size = recv( sockfd, read_buf + readbuf_pos, 1024 - readbuf_pos, MSG_DONTWAIT | MSG_NOSIGNAL);
		if ( size == 0 )	// proper disconnetion
		{
			terminate( "proper disconnetion" );
		}
		if ( size < 0 )
		{
			if ( ( errno == EAGAIN ) || ( errno == EWOULDBLOCK ) )	// no data yet
			{
				if ( readbuf_pos > 0 )
				{
					for ( int i = 0; i < ( readbuf_pos ); i++ )
					{
						if ( read_buf[i] == '\n' )
						{
							size = 0;
							break;
						}
					}
				}
				if ( size < 0 )
				{
					if (timeout_ms >= 0)
					{
						timeout_cntr += 1;
					}
					if ( timeout_cntr > timeout_ms ) return -1;
					usleep( 1000 );
					continue;
				}
			}
			else
			{
				terminate( "RX error" );	// connection error
			}
		}
		if ( size >= 0 )
		{
			int flag = 1; 
			setsockopt(sockfd, IPPROTO_TCP, TCP_QUICKACK, (char *) &flag, sizeof(int));
			if ( verbose_mode )
			{
				fprintf( stderr, "* recv: [" );
				for ( int i = 0; i < size; i++ )
				{
					if ( read_buf[readbuf_pos + i] == 0)
					{
						fprintf( stderr, "\\0" );
					} else
					if ( read_buf[readbuf_pos + i] == '\r')
					{
						fprintf( stderr, "\\r" );
					} else
					if ( read_buf[readbuf_pos + i] == '\n')
					{
						fprintf( stderr, "\\n" );
					}
					else
					{
						fprintf( stderr, "%c", read_buf[readbuf_pos + i] );
					}
				}
				fprintf( stderr, "]\n" );
			}
			int newline = -1;
			for ( int i = 0; i < ( readbuf_pos + size ); i++ )
			{
				if ( read_buf[i] == '\n' )
				{
					read_buf[i] = 0;
					newline = i;
					break;
				}
			}
			readbuf_pos += size;
			
			if ( newline >= 0 )
			{
				int ret = -1;
				int line_length = newline;
				if ( line_length > 0 )
				{
					if ( read_buf[line_length - 1] == '\r' )		// remove potential CR of CRLF
					{
						read_buf[line_length - 1] = 0;
						line_length -= 1;
					}
				}
				if ( ( line_length >= 5 ) && ( read_buf[4] == '=' ) )
				{
					if ( memcmp( read_buf, "recv", 4 ) == 0 )
					{
						int data_len = ( line_length - 5 );
						if ( ( data_len % 2 ) == 0 )
						{
							bool hex_ok = true;
							uint8_t * hex_string = read_buf + 5;
							for ( int i = 0; i < data_len; i++ )	// uppercase
							{
								if ( hex_string[i] >= 'a' )
								{
									hex_string[i] -= ( 'a' - 'A' );
								}
								if ( hex_string[i] < '0' )
								{
									hex_ok = false;
									break;
								}
								if ( hex_string[i] > 'F' )
								{
									hex_ok = false;
									break;
								}
								if ( ( hex_string[i] > '9' ) && ( hex_string[i] < 'A' ) )
								{
									hex_ok = false;
									break;
								}
							}
							
							if ( hex_ok )
							{
								data_len /= 2;
								for ( int i = 0; i < data_len; i++ )
								{
									uint8_t nibble;
									
									nibble = hex_string[i*2 + 0];
									if ( nibble <= '9' )
									{
										nibble -= '0';
									}
									else
									{
										nibble -= ('A' - 10 );
									}
									data[i] = ( nibble << 4 );
									
									nibble = hex_string[i*2 + 1];
									if ( nibble <= '9' )
									{
										nibble -= '0';
									}
									else
									{
										nibble -= ('A' - 10 );
									}
									data[i] += nibble;
								}
								
								ret = data_len;
							}
						}
					}
				}
				
				newline += 1;
				if ( newline < readbuf_pos )
				{
					readbuf_pos -= newline;
					memmove( read_buf, read_buf + newline, readbuf_pos );
				}
				else
				{
					readbuf_pos = 0;
				}
				if ( ret >= 0 ) return ret;
				if ( return_on_errors ) return -1;
			}
		}
	}
}

static void send_line( const char * line, int len )
{
	if ( verbose_mode )
	{
		fprintf( stderr, "* send: [" );
		for ( int i = 0; i < len; i++ )
		{
			if ( line[i] == 0)
			{
				fprintf( stderr, "\\0" );
			} else
			if ( line[i] == '\r')
			{
				fprintf( stderr, "\\r" );
			} else
			if ( line[i] == '\n')
			{
				fprintf( stderr, "\\n" );
			}
			else
			{
				fprintf( stderr, "%c", line[i] );
			}
		}
		fprintf( stderr, "]\n" );
	}
	for (int i=0; i<8; i++)
	{
		ssize_t sent = send( sockfd, line, len, MSG_NOSIGNAL );
		if ( sent != len )
		{
			if (sent < 0)
			{
				;// retry terminate( "TX error" );
			}
			else
			{
				terminate( "TX error partial" );
			}
		}
		usleep(1000);
		if ( sent == len ) return;
	}
	terminate( "TX error multiple retry" );
}

void send_data( uint8_t * data, int data_len )
{
	assert( data_len >= 0 );
	if ( data_len > 0 )
	{
		assert( data != NULL );
	}
	assert( data_len <= PQP_MAX_PAYLOAD );
	int len = sprintf( send_buf, "send=" );
	assert( len > 0 );
	
	for ( int i = 0; i < data_len; i++ )
	{
		int len_add = sprintf( send_buf + len, "%02X", data[i] );
		assert( len_add > 0 );
		len += len_add;
	}

	int len_add = sprintf( send_buf + len, "\n" );
	assert( len_add > 0 );
	len += len_add;
	
	assert( len > 0 );
	send_line( send_buf, len );
}

void set_port( uint8_t dst_port )
{
	int len = sprintf( send_buf, "port=%d\n", dst_port );
	assert( len > 0 );
	send_line( send_buf, len );
}

void set_priority( uint8_t priority )
{
	assert( priority <= 3 );
	int len = sprintf( send_buf, "prio=%d\n", priority );
	assert( len > 0 );
	send_line( send_buf, len );
}

void set_flag( uint8_t flag )
{
	assert( flag <= 3 );
	int len = sprintf( send_buf, "flag=%d\n", flag );
	assert( len > 0 );
	send_line( send_buf, len );
}

void set_ttl( uint8_t ttl )
{
	assert( ttl <= 15 );
	int len = sprintf( send_buf, "ttlv=%d\n", ttl );
	assert( len > 0 );
	send_line( send_buf, len );
}

int convert_arg_to_addr( const char * arg )
{
	int addr = -1;
	if ( ( arg[0] >= '0' ) && ( arg[0] <= '9' ) )	// numeric address
	{
		addr = strtoul( arg, NULL, 10 );
	}
	else	// device string address
	{
		if ( strcasecmp( "bridge", arg ) == 0 ) 	addr = PQP_USB_BRIDGE;
	}
	return addr;
}

int main( int argc, char** argv )
{
	if ( argc < ( 2 + arguments_required_num ) )
	{
		error_print_usage( argv[0] );
		return -1;
	}
	
	dst_addr = convert_arg_to_addr( argv[1] );
	
	if ( ( dst_addr <= 0 ) || ( dst_addr > 255 ) )
	{
		error_print_usage( argv[0] );
		return -1;
	}
	if ( argc >= 3 )
	{
		if ( strcmp( "--verbose", argv[2] ) == 0 )
		{
			verbose_mode = true;
		}
	}
	
	
	if ( getenv( "PQP_HUB_IP" ) != NULL )
	{
		ip_addr = getenv( "PQP_HUB_IP" );
	}
	
	short port = TCP_PORT_OFFSET + dst_addr;
	if ( getenv( "PQP_HUB_PORT_BASE" ) != NULL )
	{
		int port_override = strtol(getenv( "PQP_HUB_PORT_BASE" ), NULL, 10);
		if (port_override > 0)
		{
			port = port_override + dst_addr;
		}
	}
	struct sockaddr_in servaddr;
 
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if ( sockfd < 0 )
	{
		fprintf( stderr, " socket creation failed\n" );
		exit( -1 );
	}
	bzero( &servaddr, sizeof(servaddr) );
 
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr( ip_addr );
	servaddr.sin_port = htons( port );

	int flag = 1; 
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	setsockopt(sockfd, IPPROTO_TCP, TCP_QUICKACK, (char *) &flag, sizeof(int));
	fprintf( stderr, "Connecting to the PQP HUB ... " ); 
	fflush( stderr );
	if ( connect( sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr) ) != 0 )
	{
		fprintf( stderr, "\t failed\n" ); 
		exit( -1 );
	}
	fprintf( stderr, "\t OK\n" ); 
	
	struct timespec tp;
	clock_gettime( CLOCK_MONOTONIC, &tp );
	srand( tp.tv_nsec );
	
	
	int ret = app_main( argc - 2 - ( verbose_mode ? 1 : 0 ),  argv + 2 + ( verbose_mode ? 1 : 0 ) );
	stop_flag = true;
	
	close( sockfd );
	if ( verbose_mode )
	{
		fprintf( stderr, "Done, exit ...\n" ); 
	}
	return ret;
}





int ping( uint32_t * uptime, int timeout_ms )						// returns -1 on error/timeout, 0 if bootloader, 1 if firmware
{
	uint8_t rx_dat[PQP_MAX_PAYLOAD];
	
	set_port( MGMT_PORT_PING );
	send_data( NULL, 0 );
	int r = receive_data( rx_dat, false, timeout_ms );
	if ( r != 5 ) return -1;
	if ( ( rx_dat[0] != 0xB0 ) && ( rx_dat[0] != 0xF3 ) ) return -1;
	if ( uptime != NULL )
	{
		memcpy( uptime, rx_dat + 1, 4 );
	}
	if ( rx_dat[0] == 0xB0 ) return 0;
	return 1;
}

int transfer_test( int size, bool first, int timeout_ms )						// returns -1 on error/timeout, 0 on success
{
	uint8_t tx_dat[PQP_MAX_PAYLOAD];
	uint8_t rx_dat[PQP_MAX_PAYLOAD];
	
	if ( size > PQP_MAX_PAYLOAD ) return -1;
	if ( first )
	{
		set_port( MGMT_PORT_TRANSFER_TEST );
	}
	
	for ( int i = 0; i < size; i++ )
	{
		tx_dat[i] = ( rand( ) % 256 );
	}
	send_data( tx_dat, size );
	int r = receive_data( rx_dat, false, timeout_ms );
	if ( r != size ) return -1;
	for ( int i = 0; i < size; i++ )
	{
		if ( tx_dat[i] != rx_dat[i] ) return -1;
	}
	return 0;
}

int reboot( void )									// returns -1 on error/timeout, 0 on success
{
	uint8_t tx_dat[PQP_MAX_PAYLOAD];
	
	set_port( MGMT_PORT_REBOOT );
	
	memcpy( tx_dat, PQP_REBOOT_MAGIC, 4 );
	send_data( tx_dat, 4 );
	
	return 0;
}

int bootloader_latch( int timeout_ms )							// returns -1 on error/timeout, 0 on success
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];

	set_port( MGMT_PORT_BOOTLOADER );
	
	memcpy( trx_dat, PQP_BOOTLOADER_LATCH_MAGIC, 4 );
	send_data( trx_dat, 4 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 0 ) return -1;
	return 0;
}

int bootloader_fwupd( void )								// returns -1 on error
{
	uint8_t tx_dat[PQP_MAX_PAYLOAD];

	set_port( MGMT_PORT_BOOTLOADER );
	
	memcpy( tx_dat, PQP_BOOTLOADER_FWUPD_MAGIC, 4 );
	send_data( tx_dat, 4 );
	
	return 0;
}

int get_chksum( uint32_t * bootloader_chksum, uint32_t * fw_chksum, uint32_t * stored_chksum, uint32_t * fw_len, int timeout_ms )		// returns -1 on error/timeout, 0 on success
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];

	set_port( MGMT_PORT_CHECKSUM );
	trx_dat[0] = CUE_CHECKSUM_FW;
	send_data( trx_dat, 1 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 17 ) return -1;
	if ( trx_dat[0] != CUE_CHECKSUM_FW ) return -1;
	if ( bootloader_chksum != NULL )
	{
		memcpy( bootloader_chksum, trx_dat + 1, 4 );
	}
	if ( fw_chksum != NULL )
	{
		memcpy( fw_chksum, trx_dat + 5, 4 );
	}
	if ( stored_chksum != NULL )
	{
		memcpy( stored_chksum, trx_dat + 9, 4 );
	}
	if ( fw_len != NULL )
	{
		memcpy( fw_len, trx_dat + 13, 4 );
	}
	return 0;
}

int get_section_chksum( uint32_t * chksum, uint32_t start_addr, uint32_t len, int timeout_ms )							// returns -1 on error/timeout, 0 on success
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];

	set_port( MGMT_PORT_CHECKSUM );
	trx_dat[0] = CUE_CHECKSUM_SECTION;
	memcpy( trx_dat + 1, &start_addr, 4 );
	memcpy( trx_dat + 5, &len, 4 );
	send_data( trx_dat, 9 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 13 ) return -1;
	if ( trx_dat[0] != CUE_CHECKSUM_SECTION ) return -1;
	if ( chksum != NULL )
	{
		memcpy( chksum, trx_dat + 1, 4 );
	}
	if ( memcmp( &start_addr, trx_dat + 5, 4 ) ) return -1;
	if ( memcmp( &len, trx_dat + 9, 4 ) ) return -1;
	
	return 0;
}

int get_section_erased( bool * map, uint32_t start_addr, uint32_t len, int timeout_ms )								// returns -1 on error/timeout, number of bytes of map
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];

	set_port( MGMT_PORT_CHECKSUM );
	trx_dat[0] = CUE_ERASED_SECTION;
	memcpy( trx_dat + 1, &start_addr, 4 );
	memcpy( trx_dat + 5, &len, 4 );
	send_data( trx_dat, 9 );
	int num_of_bits = ( ( len + 255 ) / 256 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r < 9 ) return -1;
	if ( trx_dat[0] != CUE_ERASED_SECTION ) return -1;
	if ( r != ( 9 + ( ( num_of_bits + 7 ) / 8 ) ) ) return -1;
	if ( memcmp( &start_addr, trx_dat + 1, 4 ) ) return -1;
	if ( memcmp( &len, trx_dat + 5, 4 ) ) return -1;
	if ( map != NULL )
	{
		for ( int i = 0; i < num_of_bits; i++ )
		{
			map[i] = ( trx_dat[9 + i / 8] & ( 1 << ( i % 8 ) ) );
		}
	}
	
	return num_of_bits;
}
int run_fw( void )									// returns -1 on error/timeout, 0 on success
{
	set_port( MGMT_PORT_RUN );
	
	send_data( NULL, 0 );
	return 0;
}

int erase_firmware( int timeout_ms )								// returns -1 on error/timeout, 0 on success
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];
	
	set_port( MGMT_PORT_ERASE );
	
	trx_dat[0] = CUE_ERASE_FIRMWARE;
	memcpy( trx_dat + 1, PQP_ERASE_MAGIC, 4 );
	send_data( trx_dat, 5 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 2 ) return -1;
	if ( trx_dat[0] != CUE_ERASE_FIRMWARE ) return -1;
	if ( trx_dat[1] == 0x0C ) return 0;
	return -1;
}

int erase_all( int timeout_ms )								// returns -1 on error/timeout, 0 on success
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];
	
	set_port( MGMT_PORT_ERASE );
	
	trx_dat[0] = CUE_ERASE_FW_ALL;
	memcpy( trx_dat + 1, PQP_ERASE_ALL_MAGIC, 4 );
	send_data( trx_dat, 5 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 2 ) return -1;
	if ( trx_dat[0] != CUE_ERASE_FW_ALL ) return -1;
	if ( trx_dat[1] == 0x0C ) return 0;
	return -1;
}

int erase_section( uint32_t start_addr, uint32_t len, int timeout_ms )			// returns -1 on error/timeout, 0 on success
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];

	if ( start_addr % 4096 ) return -1;	// on RP2040 address and length must be FLASH_SECTOR_SIZE aligned
	if ( len % 4096 ) return -1;
	
	set_port( MGMT_PORT_ERASE );
	
	trx_dat[0] = CUE_ERASE_SECTION;
	memcpy( trx_dat + 1, PQP_ERASE_SECTION_MAGIC, 4 );
	memcpy( trx_dat + 5, &start_addr, 4 );
	memcpy( trx_dat + 9, &len, 4 );
	send_data( trx_dat, 13 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 2 ) return -1;
	if ( trx_dat[0] != CUE_ERASE_SECTION ) return -1;
	if ( trx_dat[1] == 0x0C ) return 0;
	return -1;
}

int clear_progmap( int timeout_ms )							// returns -1 on error/timeout, 0 on success
{
	uint8_t rx_dat[PQP_MAX_PAYLOAD];
	
	set_port( MGMT_PORT_CLEAR_MAP );
	
	send_data( NULL, 0 );
	int r = receive_data( rx_dat, false, timeout_ms );
	if ( r != 0 ) return -1;
	return 0;
}

int get_progmap( bool * map, int timeout_ms )					// returns -1 on error/timeout, 0 on success;	map should be a 512 size array
{
	uint8_t rx_dat[PQP_MAX_PAYLOAD];
	
	set_port( MGMT_PORT_GET_MAP );
	
	send_data( NULL, 0 );
	int r = receive_data( rx_dat, false, timeout_ms );
	if ( r != 64 ) return -1;
	if ( map != NULL )
	{
		for ( int i = 0; i < 512; i++ )
		{
			map[i] = ( rx_dat[i / 8] & ( 1 << ( i % 8 ) ) );
		}
	}
	return 0;
}

int fw_write( uint32_t addr, uint8_t * block, bool silent, int timeout_ms )		// returns -1 on error/timeout, 0 on success;
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];
	
	if ( addr % 256 ) return -1;	// must be block aligned
	
	set_port( MGMT_PORT_FW_WRITE );
	
	memcpy( trx_dat + 0, ( silent ? PQP_WRITE_MAGIC_NOREPLY : PQP_WRITE_MAGIC ), 4 );
	memcpy( trx_dat + 4, &addr, 4 );
	memcpy( trx_dat + 8, block, 256 );
	send_data( trx_dat, 8 + 256 );

	if ( !silent )
	{
		int r = receive_data( trx_dat, false, timeout_ms );
		if ( r != 1 ) return -1;
		if ( trx_dat[0] != 0x0C ) return -1;
	}
	return 0;
}

int fw_clone_start( uint8_t target_device, bool skip_ff, uint16_t block_count, uint32_t dst_write_addr, uint32_t src_read_addr, int timeout_ms )		// returns -1 on error/timeout, 0 on success;
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];
	set_port( MGMT_PORT_FW_CLONE );
	
	assert( block_count > 0 );
	
	trx_dat[0] = target_device;
	trx_dat[1] = ( skip_ff ? PQP_FW_CLONE_FLAG_SKIP_FF_BLOCK : 0 );
	memcpy( trx_dat + 2, &block_count, 2 ); 
	memcpy( trx_dat + 4, ( ( target_device == get_dst_addr() ) ? PQP_WRITE_MAGIC_NOREPLY : PQP_WRITE_MAGIC ), 4 );
	memcpy( trx_dat + 8, &dst_write_addr, 4 );
	memcpy( trx_dat + 12, &src_read_addr, 4 );
	send_data( trx_dat, 16 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 1 ) return -1;
	if ( trx_dat[0] != 0x0C ) return -1;
	return 0;
}

int fw_clone_abort( int timeout_ms )							// returns -1 on error/timeout, 0 on success;
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];
	set_port( MGMT_PORT_FW_CLONE_CTRL );
	
	trx_dat[0] = 0xAB;
	send_data( trx_dat, 1 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 12 ) return -1;
	if ( trx_dat[0] != 0 ) return -1;
	return 0;
}

int fw_clone_status( int timeout_ms )							// returns -1 on error/timeout, >= 0 remaining block count (0 == done / not in progress)
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];
	set_port( MGMT_PORT_FW_CLONE_CTRL );
	
	trx_dat[0] = 0x57;
	send_data( trx_dat, 1 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 12 ) return -1;
	if ( trx_dat[0] == 0 ) return 0;
	uint16_t block_count;
	memcpy( &block_count, trx_dat + 2, 2 );
	
	return block_count;
}

int fw_clone_set_outgoing_interface( uint8_t target_device, uint8_t interface, int timeout_ms )	// returns -1 on error/timeout, 0 on success;
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];
	set_port( MGMT_PORT_FW_CLONE_CTRL );
	
	trx_dat[0] = 0x1F;
	trx_dat[1] = target_device;
	trx_dat[2] = interface;
	send_data( trx_dat, 3 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 1 ) return -1;
	if ( trx_dat[0] == 0x0C ) return 0;
	return -1;
}

int clear_routing_table( int timeout_ms )						// returns -1 on error/timeout, 0 on success;
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];
	
	set_port( MGMT_PORT_CLEAR_ROUTING_TABLE );
	
	memcpy( trx_dat, PQP_CLEAR_ROUTING_TABLE_MAGIC, 4 );
	send_data( trx_dat, 4 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 1 ) return -1;
	if ( trx_dat[0] != 0x0C ) return -1;
	return 0;
}

int get_routing_table_checksum( uint32_t * chksum, int timeout_ms )			// returns -1 on error/timeout, 0 on success;
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];
	
	set_port( MGMT_PORT_CHECKSUM );
	trx_dat[0] = CUE_CHECKSUM_ROUTING_TABLE;
	send_data( trx_dat, 1 );
	
	trx_dat[0] = 0;
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 5 ) return -1;
	if ( trx_dat[0] != CUE_CHECKSUM_ROUTING_TABLE ) return -1;
	if ( chksum )
	{
		memcpy( chksum, trx_dat + 1, 4 );
	}
	return 0;
}

int set_routing_table_entries( uint8_t * entries, int num_of_entries, int timeout_ms )		// returns -1 on error/timeout, 0 on success;	entries should be a N*3 byte array
{
	uint8_t trx_dat[PQP_MAX_PAYLOAD];
	
	set_port( MGMT_PORT_ROUTING_TABLE_ENTRY );
	
	memcpy( trx_dat, entries, num_of_entries * 3 );
	send_data( trx_dat, num_of_entries * 3 );
	
	int r = receive_data( trx_dat, false, timeout_ms );
	if ( r != 1 ) return -1;
	if ( trx_dat[0] != 0x0C ) return -1;
	
	return 0;
}




uint8_t get_dst_addr( void )
{
	return dst_addr;
}

bool user_yesno( const char * msg )
{
	while ( 1 )
	{
		fprintf( stderr, "%s\t(Y/n)\n", msg );
		char buf[4];
		buf[0] = 0;
		if ( buf == fgets( buf, 4, stdin ) )
		{
			if ( buf[0] == 'Y' ) return true;
			if ( ( buf[0] == 'n' ) || ( buf[0] == 'N' ) )
			{
				return false;
			}
		}
		else
		{
			fprintf( stderr, "User input error\n" );
			close( sockfd );
			exit(-1);
		}
	}
}

bool get_stop_flag( void )
{
	return stop_flag;
}

static struct termios old_termios, current_termios;

static void init_termios( void )
{
	tcgetattr(0, &old_termios);
	current_termios = old_termios;
	current_termios.c_lflag &= ~ICANON;
	current_termios.c_lflag &= ~ECHO;
	tcsetattr(0, TCSANOW, &current_termios);
	tcflush(0, TCIFLUSH);
}

static void reset_termios( void )
{
	tcsetattr(0, TCSANOW, &old_termios);
}

char getch( void )
{
	char c;
	init_termios( );
	c = getchar( );
	reset_termios( );
	return c;
}

