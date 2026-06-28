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
const char * arguments_list = "<routing csv file path>";
const int arguments_required_num = 0;
/////////////////////////////////////

#define ROUTING_TABLE_ADDR_SPACE		(64U)

static uint8_t routing_table[ROUTING_TABLE_ADDR_SPACE][ROUTING_TABLE_ADDR_SPACE];

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

static void crc32_init( uint32_t * crc )
{
	if ( crc )
	{
		*crc = 0xFFFFFFFF;
	}
}

static void crc32_update( uint32_t * crc, const uint8_t data )
{
	if ( crc )
	{
		*crc = _crc_table[( (*crc) ^ data ) & 0xFFUL] ^ ( (*crc) >> 8 );
	}
}

static uint32_t crc32_final( uint32_t * crc )
{
	if ( crc )
	{
		return ( (*crc) ^ 0xFFFFFFFFUL );
	}
	return 0;
}

static uint32_t local_table_checksum( void )
{
	uint32_t crc;
	uint8_t * table_ptr = &routing_table[0][0];
	crc32_init( &crc );
	for ( int i = 0; i < ( ROUTING_TABLE_ADDR_SPACE * ROUTING_TABLE_ADDR_SPACE ); i++ )
	{
		crc32_update(  &crc, table_ptr[i] );
	}
	return crc32_final( &crc );
}

static uint32_t erased_table_checksum( void )
{
	uint32_t crc;
	crc32_init( &crc );
	for ( int i = 0; i < ( ROUTING_TABLE_ADDR_SPACE * ROUTING_TABLE_ADDR_SPACE ); i++ )
	{
		crc32_update(  &crc, 0xFF );
	}
	return crc32_final( &crc );
}

int app_main( int argc, char** argv )
{
	memset( routing_table, 0xFF, ( ROUTING_TABLE_ADDR_SPACE * ROUTING_TABLE_ADDR_SPACE ) );
	bool empty_table = true;
	if ( argc > 0 )
	{
		FILE * f;
		f = fopen( argv[0], "r" );
		if ( f != NULL )
		{
			char * line = NULL;
			size_t len = 0;
			ssize_t read;
			int lineNumber = 0;
			while ( ( read = getline( &line, &len, f ) ) != -1 )
			{
				if ( read <= 1 ) continue;	//empty line
				int src_addr, dst_addr, fwd_interface, bwd_interface;
				src_addr = -1;
				dst_addr = -1;
				lineNumber++;
				if (line[0] == '#') continue;	// comment line
				if ( sscanf ( line, "%d,%d,%d,%d", &src_addr, &dst_addr, &fwd_interface, &bwd_interface) != 4 )
				{
					fprintf( stderr, "Invalid CSV file format error. line number: %d\n",lineNumber );
					return -1;
				}
				if ( 	( src_addr < 0 ) || ( src_addr > ( ROUTING_TABLE_ADDR_SPACE - 2 ) ) || 
					( dst_addr < 0 ) || ( ( dst_addr > ( ROUTING_TABLE_ADDR_SPACE - 2 ) ) && ( dst_addr != 0xFF ) ) || 
					( fwd_interface < 0 ) || ( fwd_interface > 255 ) || ( bwd_interface < 0 ) || ( bwd_interface > 255 )
				   )
				{
					fprintf( stderr, "Invalid CSV file address or interface number error\n" );
					return -1;
				}
				routing_table[src_addr][dst_addr] = fwd_interface;
				routing_table[dst_addr][src_addr] = bwd_interface;
			}
			fclose(f);
		}
		else
		{
			fprintf( stderr, "Cannot open the CSV file\n" );
			return -1;
		}
		
		
		for ( int s = 0; s < ROUTING_TABLE_ADDR_SPACE; s++ )
		{
			for ( int d = 0; d < ROUTING_TABLE_ADDR_SPACE; d++ )
			{
				if ( ( routing_table[s][d] > 0) && ( routing_table[s][d] < 255 ) )
				{
					empty_table = false;
					break;
				}
			}
		}
		
		if ( empty_table )
		{
			if ( !user_yesno( "The provided routing table seems empty, do you continue?" ) ) return 0;
		}
	}
	
	
	if ( get_dst_addr( ) == PQP_USB_BRIDGE )
	{
		if ( empty_table )
		{
			fprintf( stderr, "Interactive routing table set for USB Bridge:\n" );
			fprintf( stderr, "----------------------------------------------\n" );
			fprintf( stderr, "(0) NULL interface (choose to clear the routing table!)\n" );
			fprintf( stderr, "(1) CAN1 interface\n" );
			fprintf( stderr, "(2) CAN2 interface\n" );
			fprintf( stderr, "(3) HDUART1 interface\n" );
			fprintf( stderr, "(4) HDUART2 interface\n" );
			fprintf( stderr, "(8) I2C-1 interface\n" );
			fprintf( stderr, "(9) I2C-2 interface\n" );
			int selected_if = -1;
			while ( 1 )
			{
				fprintf( stderr, "Choose the interface:\t(0..4)\n" );
				char buf[4];
				buf[0] = 0;
				if ( buf == fgets( buf, 4, stdin ) )
				{
					if ( ( buf[0] >= '0' ) && ( buf[0] <= '4' ) )
					{
						int c = buf[0] - '0';
						selected_if = 0;
						if ( ( c >= 1 ) && ( c <= 2 ) ) selected_if = c + 0;
						if ( ( c >= 3 ) && ( c <= 4 ) ) selected_if = c + 9-3;
						break;
					}
					else
					if ( ( buf[0] >= '8' ) && ( buf[0] <= '9' ) )
					{
						int c = buf[0] - '0';
						selected_if = c + 9;
						break;
					}
					else
					{
						fprintf( stderr, "Invalid choice\n" );
						continue;
					}
				}
				else
				{
					fprintf( stderr, "User input error\n" );
					return -1;
				}
			}
			
			if ( selected_if > 0 )
			{
				// SS-to-GND routing through STDIOIF interface
				for ( int s = 2; s < ( ROUTING_TABLE_ADDR_SPACE - 1 ); s++ )
				{
					for ( int d = 0; d < 2; d++ )
					{
						routing_table[s][d] = 254;	// STDIOIF interface
					}
				}
				// GND-to-SS routing through the selected interface
				for ( int s = 0; s < 2; s++ )
				{
					for ( int d = 2; d < ( ROUTING_TABLE_ADDR_SPACE - 1 ); d++ )
					{
						routing_table[s][d] = selected_if;
					}
				}
				empty_table = false;
			}
		}
	}
	else
	{
		if ( empty_table )
		{
			if ( !user_yesno( "No routing table provided. Do you want to just clear the device's routing table?" ) ) return 0;
		}
	}
	
	int r;
	
	r = ping( NULL, DEFAULT_TIMEOUT );						// returns -1 on error/timeout, 0 if bootloader, 1 if firmware
	if ( r < 0 )
	{
		fprintf( stderr, "PING failed\n" );
		return -1;
	}
	
	r = clear_routing_table( DEFAULT_WRITE_TIMEOUT );
	if ( r < 0 )
	{
		fprintf( stderr, "Routing table clear failed\n" );
		return -1;
	}
	
	uint32_t table_chksum;
	r = get_routing_table_checksum(&table_chksum, DEFAULT_TIMEOUT );
	if ( r < 0 )
	{
		fprintf( stderr, "Routing table checksum read failed\n" );
		return -1;
	}
	if ( table_chksum != erased_table_checksum() )
	{
		fprintf( stderr, "WARNING: Erased routing table checksum mistmatch: 0x%08X vs 0x%08X\n", table_chksum, erased_table_checksum() );
		return -1;
	}
	fprintf( stderr, "Routing table ERASED\n" );
	
	if ( empty_table )
	{
		return 0;
	}
	
	int num_of_entries = 0;
	for ( int s = 0; s < ROUTING_TABLE_ADDR_SPACE; s++ )
	{
		for ( int d = 0; d < ROUTING_TABLE_ADDR_SPACE; d++ )
		{
			if ( ( routing_table[s][d] > 0) && ( routing_table[s][d] < 255 ) )
			{
				num_of_entries += 1;
			}
		}
	}
	num_of_entries += 1;	// last
	
	uint8_t * entries = malloc( 3 * num_of_entries );
	int entry_pos = 0;
	for ( int s = 0; s < ROUTING_TABLE_ADDR_SPACE; s++ )
	{
		for ( int d = 0; d < ROUTING_TABLE_ADDR_SPACE; d++ )
		{
			if ( ( routing_table[s][d] > 0) && ( routing_table[s][d] < 255 ) )
			{
				entries[entry_pos] = s;
				entry_pos += 1;
				entries[entry_pos] = d;
				entry_pos += 1;
				entries[entry_pos] = routing_table[s][d];
				entry_pos += 1;
			}
		}
	}
	// closing entry
	entries[entry_pos] = 0xFF;
	entry_pos += 1;
	entries[entry_pos] = 0xFF;
	entry_pos += 1;
	entries[entry_pos] = 0xFF;
	entry_pos += 1;
	
	if ( entry_pos != ( 3 * num_of_entries ) )
	{
		fprintf( stderr, "INTERNAL ERROR\n" );
		return -1;
	}
	int packet_count = ( ( num_of_entries + 87 ) / 88 );	// max PQP packet payload 264 bytes --> 264 / 3 = 88
	fprintf( stderr, "Sending the routing table in %d packet(s)\n", packet_count );
	
	for ( int p = 0; p < packet_count; p++ )
	{
		int entries_in_packet = num_of_entries - p * 88;
		if ( entries_in_packet > 88 ) entries_in_packet = 88;
		fprintf( stderr, "%d / %d\n", ( p + 1 ), packet_count );
		////for (int i=0; i<entries_in_packet; i++) printf("%d->%d via %d\n", entries[p * 264 + i*3 + 0], entries[p * 264 + i*3 + 1], entries[p * 264 + i*3 + 2]);
		r = set_routing_table_entries( entries + p * 264, entries_in_packet, DEFAULT_WRITE_TIMEOUT );
		if ( r < 0 )
		{
			fprintf( stderr, "Routing table write failed, try again...\n" );
			usleep(250000);
			r = set_routing_table_entries(entries + p * 264, entries_in_packet, DEFAULT_WRITE_TIMEOUT );
			if ( r < 0 )
			{
				fprintf( stderr, "Routing table write failed\n" );
				return -1;
			}
		}
	}
	
	r = get_routing_table_checksum(&table_chksum, DEFAULT_TIMEOUT );
	if ( r < 0 )
	{
		fprintf( stderr, "Routing table checksum read failed\n" );
		return -1;
	}
	if ( local_table_checksum() != table_chksum )
	{
		fprintf( stderr, "Routing table checksum mismatch: 0x%08X vs 0x%08X  (received vs sent)\n", table_chksum, local_table_checksum() );
		return -1;
	}
	
	fprintf( stderr, "Routing table check OK\n" );
	return 0;
}
