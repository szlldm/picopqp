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
#include <string.h>
#include "pqp_usb_bridge.h"

#define PQP_MAX_PAYLOAD			( 264 )
#define RAW_PACKET_HEADER_SIZE		( 10 )
#define RAW_PACKET_MAX_SIZE		( RAW_PACKET_HEADER_SIZE + PQP_MAX_PAYLOAD )

extern _Atomic bool stop_flag;
static FILE * IOR = NULL;
static FILE * IOW = NULL;
static _Atomic uint32_t rx_buffer_head, rx_buffer_tail;
static uint8_t rx_buffer[RX_BUFFER_SIZE][RAW_PACKET_MAX_SIZE];
static uint32_t rx_buffer_lengths[RX_BUFFER_SIZE];
static uint8_t tx_buffer[RAW_PACKET_MAX_SIZE];
static uint8_t tx_encoded_buffer[RAW_PACKET_MAX_SIZE*2 + 2 + 256];
static uint32_t tx_encoded_length;

static const uint32_t _crc32_table[256] = {
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

static uint32_t crc32_init( void )
{
	return 0xFFFFFFFF;
}

static uint32_t  crc32_update( uint32_t crc, const uint8_t data )
{
	return _crc32_table[( crc ^ data ) & 0xFFUL] ^ ( crc >> 8 );
}


static void * receiver_thread_function(void * arg)
{
	uint8_t rxbuf[2048];
	int rxbuf_len;
	bool rxbuf_escape;
	
	rxbuf_escape = false;
	rxbuf_len = 0;
	
	char rbuf[2];
	while (fread(rbuf, 1, 1, IOR) > 0)
	{
		if ( rxbuf_escape )
		{
			if (rxbuf_len<2048)
			{
				if ((uint8_t)rbuf[0]==0xFD)
				{
					rxbuf[rxbuf_len] = '\n';
					rxbuf_len++;
				} else
				if ((uint8_t)rbuf[0]==0xFC)
				{
					rxbuf[rxbuf_len] = 0xFE;
					rxbuf_len++;
				}
			}
			rxbuf_escape = false;
		}
		else
		{
			if (rbuf[0]=='\n')
			{
				if ( (rxbuf_len >= RAW_PACKET_HEADER_SIZE) && (rxbuf_len <= RAW_PACKET_MAX_SIZE) )
				{
					if (((rxbuf[0]&0x3F) == 0x3E) && ((rxbuf[1]&0x30) == 0x20))	// preamble && hlen
					{
						uint32_t head = rx_buffer_head;
						uint32_t tail = rx_buffer_tail;
						head %= RX_BUFFER_SIZE;
						tail %= RX_BUFFER_SIZE;
						
						uint32_t head_next = ( ( head + 1 ) % RX_BUFFER_SIZE );
						
						if (head_next != tail)
						{
							uint8_t * buf = rx_buffer[head];
							// USB packet to RAW packet: move CRC at the end between header and payload
							
							memcpy( buf +  0, rxbuf + 0, 6 );
							memcpy( buf +  6, rxbuf + ( rxbuf_len - 4 ), 4 );	// CRC
							memcpy( buf + RAW_PACKET_HEADER_SIZE, rxbuf + 6, ( rxbuf_len - RAW_PACKET_HEADER_SIZE ) );	// payload
							
							rx_buffer_lengths[head] = rxbuf_len;
							rx_buffer_head = head_next;
						}
					}
				}
				rxbuf_len = 0;
			} else
			if ((uint8_t)rbuf[0]==0xFE)
			{
				rxbuf_escape = true;
			} else
			{
				if (rxbuf_len<2048)
				{
					rxbuf[rxbuf_len] = rbuf[0];
					rxbuf_len++;
				}
			}
		}
	}
	stop_flag = true;
	fprintf(stderr, "USB read error, exit...\n");
	pthread_exit(NULL);
}

static void encode_tx_byte(uint8_t data)
{
	if (data == '\n')
	{
		tx_encoded_buffer[tx_encoded_length] = 0xFE;
		tx_encoded_length++;
		tx_encoded_buffer[tx_encoded_length] = 0xFD;
		tx_encoded_length++;
	} else
	if (data == 0xFE)
	{
		tx_encoded_buffer[tx_encoded_length] = 0xFE;
		tx_encoded_length++;
		tx_encoded_buffer[tx_encoded_length] = 0xFC;
		tx_encoded_length++;
	}
	else
	{
		tx_encoded_buffer[tx_encoded_length] = data;
		tx_encoded_length++;
	}
}

void init_pqp_usb_bridge( int argc, char** argv )
{
	rx_buffer_head = 0;
	rx_buffer_tail = 0;
	
	const char * device = USB_DEVICE;
	if ( argc > 0 )
	{
		device = argv[0];
	}
	else
	{
		printf("Using default device %s\n", device);
	}
	
	IOR = fopen(device, "rb");
	if (!IOR)
	{
		printf("Error: cannot open %s\n",device);
		exit(-1);
	}
	IOW = fopen(device, "wb");
	if (!IOW)
	{
		printf("Error: cannot open %s\n",device);
		exit(-1);
	}
	struct termios options;
	int fd = fileno(IOR);
	tcgetattr(fd, &options);
	options.c_iflag &= ~(INLCR | IGNCR | ICRNL | IXON | IXOFF);
	options.c_oflag &= ~(ONLCR | OCRNL);
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tcsetattr(fd, TCSANOW, &options);
	fd = fileno(IOW);
	tcgetattr(fd, &options);
	options.c_iflag &= ~(INLCR | IGNCR | ICRNL | IXON | IXOFF);
	options.c_oflag &= ~(ONLCR | OCRNL);
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tcsetattr(fd, TCSANOW, &options);
	
	pthread_t rx_thread;
	pthread_create(&rx_thread, NULL, receiver_thread_function, NULL);
}

void pqp_usb_bridge_process( void )			// must be periodically called
{
	uint32_t head = rx_buffer_head;
	uint32_t tail = rx_buffer_tail;
	head %= RX_BUFFER_SIZE;
	tail %= RX_BUFFER_SIZE;
	
	if ( head != tail )
	{
		pqp_usb_bridge_raw_add_packet( rx_buffer[tail], rx_buffer_lengths[tail] );
		rx_buffer_tail = ( ( tail + 1 ) % RX_BUFFER_SIZE );
	}
	
	int tx_len = pqp_usb_bridge_raw_get_packet( tx_buffer );
	
	if ( tx_len >= RAW_PACKET_HEADER_SIZE )
	{
		// RAW packet to USB packet: move CRC from between header and payload  to the end (after payload)
		tx_encoded_length = 0;
		
		for ( int i = 0; i < 6; i++ )
		{
			encode_tx_byte( tx_buffer[i] );
		}
		for ( int i = RAW_PACKET_HEADER_SIZE; i < tx_len; i++ )		// payload
		{
			encode_tx_byte( tx_buffer[i] );
		}
		for ( int i = 6; i < RAW_PACKET_HEADER_SIZE; i++ )		// CRC
		{
			encode_tx_byte( tx_buffer[i] );
		}
		
		tx_encoded_buffer[tx_encoded_length] = '\n';
		tx_encoded_length += 1;
		
		if ( fwrite( tx_encoded_buffer, tx_encoded_length, 1, IOW ) != 1 )
		{
			printf( "TX: send ERROR\n" );
		}
	}
}



