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
const char * arguments_list = "[app.bin path] <-y : yes> <-e : erase anyway> <-a : erase all flash> <-o : erase only>";
const int arguments_required_num = 1;
/////////////////////////////////////

#define XIP_BASE 		(0x10000000UL)
#define APP_OFFSET		98304	// 96k
#define APP_HEADER_SIZE		( 256 )


time_t start_time = 0;

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

int app_main( int argc, char** argv )
{
	bool yes = false;
	bool do_erase = false;
	bool erase_only = false;
	bool erase_all_flash = false;
	
	for ( int i = 1; i < argc; i++ )
	{
		if ( strcmp( argv[i], "-y" ) == 0 )
		{
			yes = true;
		}
		if ( strcmp( argv[i], "-e" ) == 0 )
		{
			do_erase = true;
		}
		if ( strcmp( argv[i], "-a" ) == 0 )
		{
			erase_all_flash = true;
		}
		if ( strcmp( argv[i], "-o" ) == 0 )
		{
			do_erase = true;
			erase_only = true;
		}
	}
	FILE * fw_stream = fopen(argv[0], "rb");
	if ( fw_stream == NULL )
	{
		fprintf( stderr, "\033[31mFAILED\033[0m to open %s\n", argv[0] );
		return -1;
	}
	fseek(fw_stream,0, SEEK_END);
	int fw_size = ftell(fw_stream);
	fseek(fw_stream,0, SEEK_SET);
	if (fw_size <= 0)
	{
		fprintf( stderr, "Size error\n" );
		fclose( fw_stream );
		return -1;
	}
	fprintf( stderr, "FW binary is %d bytes long\n",fw_size);

	int block_count = ( fw_size + 255 ) / 256;

	uint8_t * fw_data = malloc( block_count * 256 );
	if ( !fw_data )
	{
		fprintf( stderr, "Error: malloc\n" );
		fclose( fw_stream );
		return -1;
	}
	memset( fw_data, 0xFF, ( block_count * 256 ) );
	
	if ( fread( fw_data, fw_size, 1, fw_stream ) != 1 )
	{
		fprintf( stderr, "Error: read\n" );
		fclose( fw_stream );
		return -1;
	}
	fclose( fw_stream );
	
	// check BINARY_INFO_MARKER_START
	if ( ( fw_data[0xD4 + 0] != 0xF2 ) || ( fw_data[0xD4 + 1] != 0xEB ) || ( fw_data[0xD4 + 2] != 0x88 ) || ( fw_data[0xD4 + 3] != 0x71 ) )
	{
		if ( ( fw_data[0x124 + 0] != 0xF2 ) || ( fw_data[0x124 + 1] != 0xEB ) || ( fw_data[0x124 + 2] != 0x88 ) || ( fw_data[0x124 + 3] != 0x71 ) )
		{
			fprintf( stderr, "Invalid BIN file\n" );
			return -1;
		}
	}
	fprintf( stderr, "Valid BIN file\n" );
	
	int r;
	

	r = ping( NULL, DEFAULT_TIMEOUT );						// returns -1 on error/timeout, 0 if bootloader, 1 if firmware
	
	if ( r < 0 )
	{
		fprintf( stderr, "PING \033[31mfailed\033[0m\n" );
		return -1;
	}
	fprintf( stderr, "PING \033[1;32mOK\033[0m\n" );
	
	for ( int i = 0; i < 4; i++ )
	{
		fprintf( stderr, "Send bootloader latch...\n" );
		r = bootloader_latch( DEFAULT_TIMEOUT );
		if ( r < 0 )
		{
			usleep(100000);
			continue;
		}
		if ( r == 0 )
		{
			fprintf( stderr, "Bootloader latch responded\n" );
			break;
		}
	}
	
	for ( int i = 0; i < 16; i++ )
	{
		uint32_t uptime;
		usleep(1000000);
		r = ping( &uptime, DEFAULT_TIMEOUT );
		if ( r < 0 )
		{
			fprintf( stderr, "PING \033[31mfailed\033[0m\n" );
			continue;
			//return -1;
		}
		if ( r != 0 )
		{
			fprintf( stderr, "PING \033[31mfailed\033[0m, not in bootloader mode\n" );
			return -1;
		}
		if ( uptime <= 5 )
		{
			fprintf( stderr, "PING \033[1;32mOK\033[0m, wait for uptime > 5 ...\n" );
			continue;
		}
		fprintf( stderr, "PING \033[1;32mOK\033[0m, in bootloader mode\n" );
		break;
	}

	uint32_t bootloader_chksum, fw_chksum, stored_chksum, fw_len;
	for ( int i = 0; i < 8; i++ )
	{
		fprintf(stderr,"get checksum\n");
		r = get_chksum( &bootloader_chksum, &fw_chksum, &stored_chksum, &fw_len, DEFAULT_CHECKSUM_TIMEOUT );		// returns -1 on error/timeout, 0 on success
		if ( r < 0 )
		{
			fprintf( stderr, "\033[31mFailed\033[0m to get checksum\n" );
			continue;
			return -1;
		}
		break;
	}

	if ( ( !do_erase ) && ( fw_len != 0xFFFFFFFF ) )
	{
		do_erase = yes;
		while ( !do_erase )
		{
			fprintf( stderr, "Needs firmware erase:\t(Y/n)\n" );
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
			else
			{
				fprintf( stderr, "User input error\n" );
				return -1;
			}
		}
	}
	if ( do_erase )
	{
		fprintf( stderr, "Erasing... Might take a long time!\n" );
		if ( erase_all_flash )
		{
			if ( erase_all( DEFAULT_ERASE_TIMEOUT ) < 0 )
			{
				fprintf( stderr, "Erase \033[31mfailed\033[0m\n" );
				return -1;
			}
		}
		else
		{
			if ( erase_firmware( DEFAULT_ERASE_TIMEOUT ) < 0 )
			{
				fprintf( stderr, "Erase \033[31mfailed\033[0m\n" );
				return -1;
			}
		}
		fprintf( stderr, "Erased!\n" );
	}
	else
	{
		fprintf( stderr, "Skip Erasing\n" );
	}
	if (erase_only)
	{
		fprintf( stderr, "Skip FW uploading\n" );
		return 0;
	}

	uint32_t fw_crc;
	crc32_init( &fw_crc );
	for ( int i = 0; i < fw_size; i++ )
	{
		crc32_update( &fw_crc, fw_data[i] );
	}
	fw_len = fw_size;
	crc32_update( &fw_crc, fw_len );
	fw_crc = crc32_final( &fw_crc );
	
	int current_block = 0;
	start_time = time(0);
	while ( current_block < block_count )
	{
		uint8_t * block = &( fw_data[current_block * 256] );
		uint32_t addr = XIP_BASE + APP_OFFSET + APP_HEADER_SIZE + ( current_block * 256 );
		int r = 0;
		fprintf( stderr, "Write block [%d]\t%.1f%%\n", current_block, (100.0*current_block) / block_count);
		
		r = fw_write( addr, block, false, DEFAULT_WRITE_TIMEOUT );		// returns -1 on error/timeout, 0 on success;
		if ( r < 0 )
		{
			fprintf( stderr, "Write \033[31mFAILED\033[0m!\n");
			return -1;
		}
		current_block += 1;
	}

	fprintf( stderr, "\n" );
	fprintf( stderr, "Write header...\n" );
	
	uint8_t fw_header[256];
	memset( fw_header, 0xFF, 256 );
	memcpy( fw_header + 240, "LIBPQP03", 8 );
	memcpy( fw_header + 248, &fw_len, 4 );
	memcpy( fw_header + 252, &fw_crc, 4 );
	
	r = fw_write( XIP_BASE + APP_OFFSET, fw_header, false, DEFAULT_WRITE_TIMEOUT );
	if ( r < 0 )
	{
		fprintf( stderr, "\033[31mFAILED\033[0m header write\n" );
		return -1;
	}

	r = get_chksum( &bootloader_chksum, &fw_chksum, &stored_chksum, &fw_len, DEFAULT_CHECKSUM_TIMEOUT );		// returns -1 on error/timeout, 0 on success
	if ( r < 0 )
	{
		fprintf( stderr, "\033[31mFailed\033[0m to get checksum\n" );
		return -1;
	}

	fprintf( stderr, "Firmware length:     %u vs %u (readback vs upload)\n", fw_len, fw_size );
	fprintf( stderr, "Firmware checksum:   0x%08X vs 0x%08X vs 0x%08X\t(readback(calculated vs stored) vs upload)\n", fw_chksum, stored_chksum, fw_crc );

	if ( ( fw_chksum != stored_chksum ) || ( fw_len != fw_size ) || ( fw_chksum != fw_crc ) )
	{
		fprintf( stderr, "Checksum check \033[31mFAILED\033[0m\n" );
		return -1;
	}

	fprintf(stderr, "Firmware upload \033[1;32mDONE\033[0m!\n");
	time_t end_time = time(0);
	fprintf(stderr, "Firmware upload time %ld sec\n", end_time - start_time);
	return 0;
}
