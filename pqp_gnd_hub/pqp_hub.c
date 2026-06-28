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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <stdarg.h>
#include <signal.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>

#include "pqp_hub.h"
#include "pqp_usb_bridge.h"

#define PQP_MAX_PAYLOAD			( 264 )
#define RAW_PACKET_HEADER_SIZE		( 10 )
#define RAW_PACKET_MAX_SIZE		( RAW_PACKET_HEADER_SIZE + PQP_MAX_PAYLOAD )

#if MAX_DEVICE_ADDR > 124
#error MAX_DEVICE_ADDR > 124 currently not supported
#endif

static int hub_raw_get_packet( uint8_t * data );			// returns the number of bytes of the raw packet, or -1 on error / no packet
static int hub_raw_add_packet( uint8_t * data, int data_len );	// returns 0 if raw packet taken, or -1 if cannot take at the moment (try later)


int pqp_usb_bridge_raw_get_packet( uint8_t * data )
{
	return hub_raw_get_packet( data );
}
int pqp_usb_bridge_raw_add_packet( uint8_t * data, int data_len )
{
	return hub_raw_add_packet( data, data_len );
}


_Atomic bool stop_flag = false;
const char * SIGINT_handler_msg = "\nSIGINT registered...\n";
void SIGINT_handler( int sig )
{
	if ( stop_flag )
	{
		signal( sig, SIG_DFL );
		raise( sig );
	}
	else
	{
		ssize_t dummy = write( STDERR_FILENO, SIGINT_handler_msg, strlen( SIGINT_handler_msg ) );
	}
	stop_flag = true;
}

static bool verbose_mode = false;

static const uint32_t _crc_table[256] = {
	0x00000000, 0xF26B8303, 0xE13B70F7, 0x1350F3F4, 0xC79A971F, 0x35F1141C, 0x26A1E7E8, 0xD4CA64EB,
	0x8AD958CF, 0x78B2DBCC, 0x6BE22838, 0x9989AB3B, 0x4D43CFD0, 0xBF284CD3, 0xAC78BF27, 0x5E133C24,
	0x105EC76F, 0xE235446C, 0xF165B798, 0x030E349B, 0xD7C45070, 0x25AFD373, 0x36FF2087, 0xC494A384,
	0x9A879FA0, 0x68EC1CA3, 0x7BBCEF57, 0x89D76C54, 0x5D1D08BF, 0xAF768BBC, 0xBC267848, 0x4E4DFB4B,
	0x20BD8EDE, 0xD2D60DDD, 0xC186FE29, 0x33ED7D2A, 0xE72719C1, 0x154C9AC2, 0x061C6936, 0xF477EA35,
	0xAA64D611, 0x580F5512, 0x4B5FA6E6, 0xB93425E5, 0x6DFE410E, 0x9F95C20D, 0x8CC531F9, 0x7EAEB2FA,
	0x30E349B1, 0xC288CAB2, 0xD1D83946, 0x23B3BA45, 0xF779DEAE, 0x05125DAD, 0x1642AE59, 0xE4292D5A,
	0xBA3A117E, 0x4851927D, 0x5B016189, 0xA96AE28A, 0x7DA08661, 0x8FCB0562, 0x9C9BF696, 0x6EF07595,
	0x417B1DBC, 0xB3109EBF, 0xA0406D4B, 0x522BEE48, 0x86E18AA3, 0x748A09A0, 0x67DAFA54, 0x95B17957,
	0xCBA24573, 0x39C9C670, 0x2A993584, 0xD8F2B687, 0x0C38D26C, 0xFE53516F, 0xED03A29B, 0x1F682198,
	0x5125DAD3, 0xA34E59D0, 0xB01EAA24, 0x42752927, 0x96BF4DCC, 0x64D4CECF, 0x77843D3B, 0x85EFBE38,
	0xDBFC821C, 0x2997011F, 0x3AC7F2EB, 0xC8AC71E8, 0x1C661503, 0xEE0D9600, 0xFD5D65F4, 0x0F36E6F7,
	0x61C69362, 0x93AD1061, 0x80FDE395, 0x72966096, 0xA65C047D, 0x5437877E, 0x4767748A, 0xB50CF789,
	0xEB1FCBAD, 0x197448AE, 0x0A24BB5A, 0xF84F3859, 0x2C855CB2, 0xDEEEDFB1, 0xCDBE2C45, 0x3FD5AF46,
	0x7198540D, 0x83F3D70E, 0x90A324FA, 0x62C8A7F9, 0xB602C312, 0x44694011, 0x5739B3E5, 0xA55230E6,
	0xFB410CC2, 0x092A8FC1, 0x1A7A7C35, 0xE811FF36, 0x3CDB9BDD, 0xCEB018DE, 0xDDE0EB2A, 0x2F8B6829,
	0x82F63B78, 0x709DB87B, 0x63CD4B8F, 0x91A6C88C, 0x456CAC67, 0xB7072F64, 0xA457DC90, 0x563C5F93,
	0x082F63B7, 0xFA44E0B4, 0xE9141340, 0x1B7F9043, 0xCFB5F4A8, 0x3DDE77AB, 0x2E8E845F, 0xDCE5075C,
	0x92A8FC17, 0x60C37F14, 0x73938CE0, 0x81F80FE3, 0x55326B08, 0xA759E80B, 0xB4091BFF, 0x466298FC,
	0x1871A4D8, 0xEA1A27DB, 0xF94AD42F, 0x0B21572C, 0xDFEB33C7, 0x2D80B0C4, 0x3ED04330, 0xCCBBC033,
	0xA24BB5A6, 0x502036A5, 0x4370C551, 0xB11B4652, 0x65D122B9, 0x97BAA1BA, 0x84EA524E, 0x7681D14D,
	0x2892ED69, 0xDAF96E6A, 0xC9A99D9E, 0x3BC21E9D, 0xEF087A76, 0x1D63F975, 0x0E330A81, 0xFC588982,
	0xB21572C9, 0x407EF1CA, 0x532E023E, 0xA145813D, 0x758FE5D6, 0x87E466D5, 0x94B49521, 0x66DF1622,
	0x38CC2A06, 0xCAA7A905, 0xD9F75AF1, 0x2B9CD9F2, 0xFF56BD19, 0x0D3D3E1A, 0x1E6DCDEE, 0xEC064EED,
	0xC38D26C4, 0x31E6A5C7, 0x22B65633, 0xD0DDD530, 0x0417B1DB, 0xF67C32D8, 0xE52CC12C, 0x1747422F,
	0x49547E0B, 0xBB3FFD08, 0xA86F0EFC, 0x5A048DFF, 0x8ECEE914, 0x7CA56A17, 0x6FF599E3, 0x9D9E1AE0,
	0xD3D3E1AB, 0x21B862A8, 0x32E8915C, 0xC083125F, 0x144976B4, 0xE622F5B7, 0xF5720643, 0x07198540,
	0x590AB964, 0xAB613A67, 0xB831C993, 0x4A5A4A90, 0x9E902E7B, 0x6CFBAD78, 0x7FAB5E8C, 0x8DC0DD8F,
	0xE330A81A, 0x115B2B19, 0x020BD8ED, 0xF0605BEE, 0x24AA3F05, 0xD6C1BC06, 0xC5914FF2, 0x37FACCF1,
	0x69E9F0D5, 0x9B8273D6, 0x88D28022, 0x7AB90321, 0xAE7367CA, 0x5C18E4C9, 0x4F48173D, 0xBD23943E,
	0xF36E6F75, 0x0105EC76, 0x12551F82, 0xE03E9C81, 0x34F4F86A, 0xC69F7B69, 0xD5CF889D, 0x27A40B9E,
	0x79B737BA, 0x8BDCB4B9, 0x988C474D, 0x6AE7C44E, 0xBE2DA0A5, 0x4C4623A6, 0x5F16D052, 0xAD7D5351
};

static void pqp_crc32_init( uint32_t * crc )
{
	if ( crc )
	{
		*crc = 0xFFFFFFFF;
	}
}

static void pqp_crc32_update( uint32_t * crc, const uint8_t data )
{
	if ( crc )
	{
		*crc = _crc_table[( (*crc) ^ data ) & 0xFFUL] ^ ( (*crc) >> 8 );
	}
}

static uint32_t pqp_crc32_final( uint32_t * crc )
{
	if ( crc )
	{
		return ( (*crc) ^ 0xFFFFFFFFUL );
	}
	return 0;
}


typedef struct
{
	uint32_t head, tail;
	uint32_t data_len[TXRX_QUEUE_LEN];
	uint8_t priority[TXRX_QUEUE_LEN];
	uint8_t fifo[TXRX_QUEUE_LEN][RAW_PACKET_MAX_SIZE];
} txrx_fifo_t;

static pthread_mutex_t		server_mutex;
#define SERVER_ACCESS_LOCK	pthread_mutex_lock(&server_mutex)
#define SERVER_ACCESS_UNLOCK	pthread_mutex_unlock(&server_mutex)
static uint64_t			server_uniq = 0;
static uint64_t			pqp_reservations[256] = { 0 };
static txrx_fifo_t * 		rx_fifos[256] = { NULL };
static txrx_fifo_t		tx_fifos[256];


static int hub_raw_get_packet( uint8_t * data )			// returns the number of bytes of the raw packet, or -1 on error / no packet
{
	int ret = -1;
	int last_prio = 4;
	int last_addr = -1;
	SERVER_ACCESS_LOCK;
	for ( int i = 0; i < 256; i++ )
	{
		uint32_t head = tx_fifos[i].head;
		uint32_t tail = tx_fifos[i].tail;
		head %= TXRX_QUEUE_LEN;
		tail %= TXRX_QUEUE_LEN;
		if ( head != tail )
		{
			if ( tx_fifos[i].priority[tail] < last_prio )
			{
				last_prio = tx_fifos[i].priority[tail];
				last_addr = i;
				if ( last_prio == 0 ) break;	// here is no higher priority
			}
		}
	}
	if ( last_addr >= 0 )
	{
		uint32_t tail = tx_fifos[last_addr].tail;
		tail %= TXRX_QUEUE_LEN;
		ret = tx_fifos[last_addr].data_len[tail];
		memcpy( data, tx_fifos[last_addr].fifo[tail], ret );
		tail += 1;
		tail %= TXRX_QUEUE_LEN;
		tx_fifos[last_addr].tail = tail;
	}
	SERVER_ACCESS_UNLOCK;
	
	return ret;
}

static int hub_raw_add_packet( uint8_t * data, int data_len )	// returns 0 if raw packet taken, or -1 if cannot take at the moment (try later)
{
	if ( ( data_len >= RAW_PACKET_HEADER_SIZE ) && ( data_len <= RAW_PACKET_MAX_SIZE ) )
	{
		uint8_t src_addr = data[2];
		SERVER_ACCESS_LOCK;
		txrx_fifo_t * fifo = rx_fifos[src_addr];
		if ( fifo != NULL )
		{
			uint32_t head = fifo->head;
			uint32_t tail = fifo->tail;
			head %= TXRX_QUEUE_LEN;
			tail %= TXRX_QUEUE_LEN;
			uint32_t next_head = head + 1;
			next_head %= TXRX_QUEUE_LEN;
			if ( next_head == tail )
			{
				fprintf( stderr, "SERVER OVERFLOW: from source address %d\n", src_addr );
			}
			else
			{
				memcpy( fifo->fifo[head], data, data_len );
				fifo->data_len[head] = data_len;
				fifo->head = next_head;
			}
		}
		SERVER_ACCESS_UNLOCK;
	}
	return 0;
}

typedef struct
{
	int fd;
	int pqp_address;
	uint64_t my_uniq;
	txrx_fifo_t rx_fifo;
} pqp_server_thread_function_arg_t;

static void * pqp_server_thread_function( void * arg )
{
	if ( arg == NULL )
	{
		fprintf( stderr, "SERVER ERROR: NULL arg\n" );
		pthread_exit( NULL );
	}
	
	pqp_server_thread_function_arg_t * args = (pqp_server_thread_function_arg_t *)arg;
	uint8_t readbuf[1024];
	uint8_t writebuf[1024];
	bool rx_data_valid;
	uint32_t trx_data_len;
	uint8_t trx_data[RAW_PACKET_MAX_SIZE];
	int readbuf_pos = 0;
	bool connected = true;
	bool abandoned;
	
	uint8_t pqp_priority = 2;
	uint8_t pqp_flags = 0;
	uint8_t pqp_ttl = 8;
	int pqp_dst_port = -1;
	
	while ( 1 )
	{
		if ( stop_flag ) break;
		rx_data_valid = false;
		abandoned = false;
		SERVER_ACCESS_LOCK;
		if ( args->rx_fifo.head != args->rx_fifo.tail )
		{
			uint32_t tail = args->rx_fifo.tail;
			tail %= TXRX_QUEUE_LEN;
			memcpy( trx_data, args->rx_fifo.fifo[tail], RAW_PACKET_MAX_SIZE );
			trx_data_len = args->rx_fifo.data_len[tail];
			rx_data_valid = true;
			tail += 1;
			tail %= TXRX_QUEUE_LEN;
			args->rx_fifo.tail = tail;
			
			abandoned |= ( pqp_reservations[args->pqp_address] != args->my_uniq );
			abandoned |= ( rx_fifos[args->pqp_address] != &( args->rx_fifo ) );
		}
		SERVER_ACCESS_UNLOCK;
		
		if ( rx_data_valid )
		{
			do {
				if ( trx_data_len < RAW_PACKET_HEADER_SIZE )
				{
					const char * error_msg = "rerr=length\n";
					if ( verbose_mode )
					{
						fprintf( stderr, "{%d,%d,%lu} SEND: %s", args->fd, args->pqp_address, args->my_uniq, error_msg );
					}
					send( args->fd, error_msg, strlen( error_msg ), MSG_NOSIGNAL );
					break;
				}
				if ( trx_data[3] != SRC_ADDR )
				{
					if ( trx_data[3] == 255 )
					{
						const char * error_msg = "rerr=broadcast\n";
						if ( verbose_mode )
						{
							fprintf( stderr, "{%d,%d,%lu} SEND: %s", args->fd, args->pqp_address, args->my_uniq, error_msg );
						}
						//send( args->fd, error_msg, strlen( error_msg ), MSG_NOSIGNAL );
					}
					else
					{
						const char * error_msg = "rerr=dst_addr\n";
						if ( verbose_mode )
						{
							fprintf( stderr, "{%d,%d,%lu} SEND: %s", args->fd, args->pqp_address, args->my_uniq, error_msg );
						}
						//send( args->fd, error_msg, strlen( error_msg ), MSG_NOSIGNAL );
					}
					break;
				}
				if ( trx_data[2] != args->pqp_address )
				{
					const char * error_msg = "rerr=src_addr\n";
					if ( verbose_mode )
					{
						fprintf( stderr, "{%d,%d,%lu} SEND: %s", args->fd, args->pqp_address, args->my_uniq, error_msg );
					}
					send( args->fd, error_msg, strlen( error_msg ), MSG_NOSIGNAL );
					break;
				}
				
				bool is_csp = ( ( ( trx_data[1] >> 6 ) & 0x03 ) == 0x01 );
				
				if ( args->pqp_address >= 254 )	// src port
				{
					if ( trx_data[5] != args->pqp_address - 126 )
					{
						const char * error_msg = "rerr=dst_port\n";
						if ( verbose_mode )
						{
							fprintf( stderr, "{%d,%d,%lu} SEND: %s", args->fd, args->pqp_address, args->my_uniq, error_msg );
						}
						send( args->fd, error_msg, strlen( error_msg ), MSG_NOSIGNAL );
						break;
					}
				}
				else
				{
					if ( trx_data[5] != args->pqp_address + 130 - ( is_csp ? 100 : 0 ) )	// TODO currently no more than 124 devices supported
					{
						const char * error_msg = "rerr=dst_port\n";
						if ( verbose_mode )
						{
							fprintf( stderr, "{%d,%d,%lu} SEND: %s", args->fd, args->pqp_address, args->my_uniq, error_msg );
						}
						send( args->fd, error_msg, strlen( error_msg ), MSG_NOSIGNAL );
						break;
					}
				}
				if ( trx_data[4] != pqp_dst_port )
				{
					const char * error_msg = "rerr=src_port\n";
					if ( verbose_mode )
					{
						fprintf( stderr, "{%d,%d,%lu} SEND: %s", args->fd, args->pqp_address, args->my_uniq, error_msg );
					}
					send( args->fd, error_msg, strlen( error_msg ), MSG_NOSIGNAL );
					break;
				}
				
				uint32_t crc;
				pqp_crc32_init( &crc );
				pqp_crc32_update( &crc, trx_data[2]);	// src_addr
				pqp_crc32_update( &crc, trx_data[3]);	// dst_addr
				pqp_crc32_update( &crc, trx_data[4]);	// src_port
				pqp_crc32_update( &crc, trx_data[5]);	// dst_port
				
				int data_len = trx_data_len - RAW_PACKET_HEADER_SIZE;
				for ( int i = 0; i < data_len; i++ )
				{
					pqp_crc32_update( &crc, trx_data[i + RAW_PACKET_HEADER_SIZE] );
				}
				
				crc = pqp_crc32_final( &crc );
				
				if ( is_csp )
				{
					trx_data[5] += 100;
				}
				
				if ( memcmp( trx_data + 6, &crc, 4 ) )
				{
					const char * error_msg = "rerr=crc\n";
					if ( verbose_mode )
					{
						fprintf( stderr, "{%d,%d,%lu} SEND: %s", args->fd, args->pqp_address, args->my_uniq, error_msg );
					}
					send( args->fd, error_msg, strlen( error_msg ), MSG_NOSIGNAL );
					break;
				}
				
				int len = sprintf( writebuf, "recv=" );
				for ( int i = 0; i < data_len; i++ )
				{
					len += sprintf( writebuf + len, "%02X", trx_data[i + RAW_PACKET_HEADER_SIZE] );
				}
				len += sprintf( writebuf + len, "\n" );
				if ( verbose_mode )
				{
					fprintf( stderr, "{%d,%d,%lu} SEND: %s", args->fd, args->pqp_address, args->my_uniq, writebuf );
				}
				send( args->fd, writebuf, len, MSG_NOSIGNAL );
			} while (0);
		}
		if ( abandoned ) break;			// someone else connected to this PQP address
		if ( readbuf_pos > 768 ) break;		// too long
		int size = recv( args->fd, readbuf + readbuf_pos, 1024 - readbuf_pos, MSG_DONTWAIT | MSG_NOSIGNAL);
		if ( size == 0 )	// proper disconnetion
		{
			connected = false;
			break;
		}
		if ( size < 0 )
		{
			if ( ( errno == EAGAIN ) || ( errno == EWOULDBLOCK ) )	// no data yet
			{
				usleep( 1000 );
				continue;
			}
			else
			{
				connected = false;
				break;	// connection error
			}
		}
		if ( size > 0 )
		{
			int flag = 1; 
			setsockopt(args->fd, IPPROTO_TCP, TCP_QUICKACK, (char *) &flag, sizeof(int));
			int newline = -1;
			for ( int i = readbuf_pos; i < ( readbuf_pos + size ); i++ )
			{
				if ( readbuf[i] == '\n' )
				{
					readbuf[i] = 0;
					newline = i;
					break;
				}
			}
			readbuf_pos += size;
			
			if ( newline >= 0 )
			{
				if ( verbose_mode )
				{
					fprintf( stderr, "{%d,%d,%lu} RECV: %s\n", args->fd, args->pqp_address, args->my_uniq, readbuf );
				}
				int line_length = newline;
				if ( line_length > 0 )
				{
					if ( readbuf[line_length - 1] == '\r' )		// remove potential CR of CRLF
					{
						readbuf[line_length - 1] = 0;
						line_length -= 1;
					}
				}
				if ( ( line_length >= 5 ) && ( readbuf[4] == '=' ) )
				{
					bool error = true;
					if ( memcmp( readbuf, "send", 4 ) == 0 )
					{
						int data_len = ( line_length - 5 );
						if ( ( ( data_len % 2 ) == 0 ) && ( pqp_dst_port >= 0 ) )
						{
							bool hex_ok = true;
							uint8_t * hex_string = readbuf + 5;
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
									trx_data[i + RAW_PACKET_HEADER_SIZE] = ( nibble << 4 );
									
									nibble = hex_string[i*2 + 1];
									if ( nibble <= '9' )
									{
										nibble -= '0';
									}
									else
									{
										nibble -= ('A' - 10 );
									}
									trx_data[i + RAW_PACKET_HEADER_SIZE] += nibble;
								}
								
								trx_data[0] = ( ( pqp_priority << 6 ) | 0x3E );
								trx_data[1] = ( ( ( pqp_flags & 0x03 ) << 6 ) | ( 0x20 ) | ( ( pqp_ttl & 0x0F) << 0 ) );
								trx_data[2] = SRC_ADDR;
								trx_data[3] = args->pqp_address;
								if ( args->pqp_address >= 254 )	// src port
								{
									trx_data[4] = args->pqp_address - 126;
								}
								else
								{
									trx_data[4] = args->pqp_address + 130;	// TODO currently no more than 124 devices supported
								}
								trx_data[5] = pqp_dst_port;
								
								if (pqp_flags == 0x01) // CSP hack
								{
									trx_data[4] -= 100;
								}
								
								uint32_t crc;
								
								pqp_crc32_init( &crc );
								pqp_crc32_update( &crc, trx_data[2]);	// src_addr
								pqp_crc32_update( &crc, trx_data[3]);	// dst_addr
								pqp_crc32_update( &crc, trx_data[4]);	// src_port
								pqp_crc32_update( &crc, trx_data[5]);	// dst_port
								
								for ( int i = 0; i < data_len; i++ )
								{
									pqp_crc32_update( &crc, trx_data[i + RAW_PACKET_HEADER_SIZE] );
								}
								
								crc = pqp_crc32_final( &crc );
								
								memcpy( trx_data + 6, &crc, 4 );
								
								trx_data_len = RAW_PACKET_HEADER_SIZE + data_len;
								if( trx_data_len > RAW_PACKET_MAX_SIZE )
								{
									fprintf( stderr, "Data length overflow\n" );
									break;
								}
								
								abandoned = false;
								SERVER_ACCESS_LOCK;
								abandoned |= ( pqp_reservations[args->pqp_address] != args->my_uniq );
								abandoned |= ( rx_fifos[args->pqp_address] != &( args->rx_fifo ) );
								if ( !abandoned )
								{
									uint32_t head = tx_fifos[args->pqp_address].head;
									uint32_t tail = tx_fifos[args->pqp_address].tail;
									head %= TXRX_QUEUE_LEN;
									tail %= TXRX_QUEUE_LEN;
									uint32_t next_head = head + 1;
									next_head %= TXRX_QUEUE_LEN;
									
									if ( next_head != tail )
									{	
										tx_fifos[args->pqp_address].data_len[head] = trx_data_len;
										tx_fifos[args->pqp_address].priority[head] = pqp_priority;
										memcpy( tx_fifos[args->pqp_address].fifo[head], trx_data, trx_data_len );
										tx_fifos[args->pqp_address].head = next_head;
										error = false;
									}
								}
								SERVER_ACCESS_UNLOCK;
								if ( abandoned ) break;
							}
						}
					} else
					if ( line_length > 5 )
					{
						if ( memcmp( readbuf, "port", 4 ) == 0 )
						{
							char * endptr;
							int p = strtol( readbuf + 5, &endptr, 10 );
							if ( *endptr == 0 )
							{
								if ( ( p >= 0 ) && ( p < 255 ) )
								{
									error = false;
									pqp_dst_port = p;
								}
							}
						} else
						if ( memcmp( readbuf, "prio", 4 ) == 0 )
						{
							char * endptr;
							int p = strtol( readbuf + 5, &endptr, 10 );
							if ( *endptr == 0 )
							{
								if ( ( p >= 0 ) && ( p <= 3 ) )
								{
									error = false;
									pqp_priority = p;
								}
							}
						} else
						if ( memcmp( readbuf, "flag", 4 ) == 0 )
						{
							char * endptr;
							int p = strtol( readbuf + 5, &endptr, 10 );
							if ( *endptr == 0 )
							{
								if ( ( p >= 0 ) && ( p < 4 ) )
								{
									error = false;
									pqp_flags = p;
								}
							}
						} else
						if ( memcmp( readbuf, "ttlv", 4 ) == 0 )
						{
							char * endptr;
							int p = strtol( readbuf + 5, &endptr, 10 );
							if ( *endptr == 0 )
							{
								if ( ( p >= 0 ) && ( p < 16 ) )
								{
									error = false;
									pqp_ttl = p;
								}
							}
						}
					}
					
					if ( error )
					{
						send( args->fd, "error\n", strlen( "error\n" ), MSG_NOSIGNAL );
					}
				}
				
				newline += 1;
				if ( newline < readbuf_pos )
				{
					readbuf_pos -= newline;
					readbuf_pos += 1;
					memmove( readbuf, readbuf + newline, readbuf_pos );
				}
				else
				{
					readbuf_pos = 0;
				}
			}
		}
	}
	
	
	SERVER_ACCESS_LOCK;
	if ( rx_fifos[args->pqp_address] == &( args->rx_fifo ) )
	{
		rx_fifos[args->pqp_address] = NULL;
	}
	SERVER_ACCESS_UNLOCK;
	if ( connected )
	{
		send( args->fd, "exit\n", strlen( "exit\n" ), MSG_NOSIGNAL );
	}
	close( args->fd );
	fprintf( stderr, "DISCONNECTION from PQP device %d\n", args->pqp_address );
	free( arg );
	pthread_exit( NULL );
}

typedef struct
{
	int tcp_port;
	int pqp_address;
} tcp_server_thread_function_arg_t;

#define crash( ... )		do { fprintf ( stderr, __VA_ARGS__ ); exit(-1); } while ( 0 )

static void * tcp_server_thread_function( void * arg )
{
	if ( arg == NULL ) crash( "SERVER ERROR: NULL arg\n" );
	
	tcp_server_thread_function_arg_t * args = (tcp_server_thread_function_arg_t *)arg;
	int listenfd = -1;
	struct sockaddr_in serveraddr;
	struct sockaddr_in clientaddr;
	int clientlen = sizeof( clientaddr );
	struct pollfd pfds;
	
	listenfd = socket( AF_INET, SOCK_STREAM, 0 );
	if ( listenfd < 0 ) crash( "SERVER ERROR: socket(), port %d\n", args->tcp_port );

	int optval = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int))) crash( "SERVER ERROR: setsocketopt(), SO_REUSEADDR, port %d\n", args->tcp_port );

	bzero( (char *) &serveraddr, sizeof( serveraddr ) );
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl( INADDR_ANY );
	serveraddr.sin_port = htons( args->tcp_port );
	if ( bind( listenfd, (struct sockaddr *) &serveraddr, sizeof( serveraddr ) ) < 0 ) crash( "SERVER ERROR: bind(), port %d\n", args->tcp_port );
	if ( listen ( listenfd, SERVER_MAX_CONN ) < 0 ) crash( "SERVER ERROR: listen(), port %d\n", args->tcp_port );
	
	pfds.fd = listenfd;
	pfds.events = POLLIN;
	
	while (1)	// wait for connection
	{
		if ( stop_flag ) break;
		if ( poll( &pfds, 1, 100 ) > 0 )	// 100ms timeout
		{
			if ( pfds.revents & POLLIN )	// there is something to accept
			{
				int fd = accept( listenfd, (struct sockaddr *) &clientaddr, &clientlen );
				if (fd < 0)
				{
					fprintf( stderr, "SERVER ERROR: accept(), port %d\n", args->tcp_port );
				}
				else
				{
					char ip_str[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &(clientaddr.sin_addr), ip_str, INET_ADDRSTRLEN);
					fprintf( stderr, "CONNECTION from %s:%d to PQP device %d\n", ip_str, ntohs( clientaddr.sin_port ), args->pqp_address );
					
					struct timeval timeout;
					timeout.tv_sec  = 1;	// 1 second timeout
					timeout.tv_usec = 0;
					if ( setsockopt( fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof( timeout ) ) )
					{
						fprintf( stderr, "SERVER ERROR: setsocketopt(), SO_SNDTIMEO, port %d\n", args->tcp_port );
					}
					int flag = 1; 
					setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
					setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, (char *) &flag, sizeof(int));
					pthread_t pqp_server_thread;
					pqp_server_thread_function_arg_t * psarg = malloc( sizeof( pqp_server_thread_function_arg_t ));
					psarg->fd = fd;
					psarg->pqp_address = args->pqp_address;
					psarg->rx_fifo.head = 0;
					psarg->rx_fifo.tail = 0;
					SERVER_ACCESS_LOCK;
					server_uniq++;
					psarg->my_uniq = server_uniq;
					pqp_reservations[args->pqp_address] = server_uniq;
					rx_fifos[args->pqp_address] = &( psarg->rx_fifo );
					SERVER_ACCESS_UNLOCK;
					if ( pthread_create( &pqp_server_thread, NULL, pqp_server_thread_function, (void*)psarg ) != 0 )
					{
						fprintf( stderr, "SERVER ERROR: pthread_create(), port %d\n", args->tcp_port );
						SERVER_ACCESS_LOCK;
						rx_fifos[args->pqp_address] = NULL;
						SERVER_ACCESS_UNLOCK;
						close( fd );
						free( psarg );
					}
					else
					{
						pthread_detach( pqp_server_thread );
					}
				}
			}
		}
	}
	
	if ( listenfd >= 0 )
	{
		close( listenfd );
	}

	pthread_exit( NULL );
}

int main( int argc, char** argv )
{
	if ( argc >= 2 )
	{
		if ( strcmp( "--verbose", argv[1] ) == 0 )
		{
			verbose_mode = true;
		}
	}
	
	int tcp_port_base = TCP_PORT_OFFSET;
	if ( getenv( "PQP_HUB_PORT_BASE" ) != NULL )
	{
		int port_override = strtol(getenv( "PQP_HUB_PORT_BASE" ), NULL, 10);
		if (port_override > 0)
		{
			tcp_port_base = port_override;
		}
	}
	
	// block signals on all threads
	sigset_t sigset, oldset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	pthread_sigmask(SIG_BLOCK, &sigset, &oldset);

	struct sigaction s;
	s.sa_handler = SIGINT_handler;
	sigemptyset(&s.sa_mask);
	s.sa_flags = 0;
	sigaction(SIGINT, &s, NULL);
	
	init_pqp_usb_bridge( argc - 1 - ( verbose_mode ? 1 : 0 ), argv + 1 + ( verbose_mode ? 1 : 0 ) );
	
	if ( pthread_mutex_init( &server_mutex, NULL ) != 0 )
	{
		crash( "Cannot initialize mutex\n" );
	}
	for ( int i = 0; i < 256; i++ )
	{
		pqp_reservations[i] = 0;
		rx_fifos[i] = NULL;
		tx_fifos[i].head = 0;
		tx_fifos[i].tail = 0;
	}
	
	pthread_t tcp_threads[256];
	tcp_server_thread_function_arg_t tcp_thread_args[256];
	for ( int i = 0; i < 256; i++ )
	{
		if ( ( i > 124) && ( i < 254 ) ) continue;	// TODO currently no more than 124 devices supported
		tcp_thread_args[i].tcp_port = i + tcp_port_base;
		tcp_thread_args[i].pqp_address = i;
		if ( pthread_create( &tcp_threads[i], NULL, tcp_server_thread_function, (void*)( &( tcp_thread_args[i] ) ) ) != 0 )
		{
			crash( "Cannot create TCP server thread %d\n", i );
		}
	}
	
	pthread_sigmask(SIG_SETMASK, &oldset, NULL);

	while (1)
	{
		if ( stop_flag ) break;
		pqp_usb_bridge_process( );
		usleep(100);
	}
	fprintf( stderr, "Exit in 1 second\n" );
	sleep( 1 );
}
