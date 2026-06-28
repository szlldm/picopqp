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

#ifndef LIBPQP_DEFINES_H
#define LIBPQP_DEFINES_H


#ifdef __cplusplus
extern "C" {
#endif

#define LIBPQP_BROADCAST_ADDRESS		(255)
#define PQP_MAX_PAYLOAD				(264)		// firmware block must fit... 

typedef enum{
	FREE = 0,
	RECEIVED,
	USERSPACE,
	TO_BE_SENT,
	INTERFACE,
} pqp_packet_status_t;

typedef enum{
	MGMT_PORT_BEGIN = 16,
	MGMT_PORT_PING = 16,
	MGMT_PORT_TRANSFER_TEST,
	MGMT_PORT_REBOOT,
	MGMT_PORT_BOOTLOADER,
	MGMT_PORT_CHECKSUM,
	MGMT_PORT_RUN,
	MGMT_PORT_ERASE,
	MGMT_PORT_CLEAR_MAP,
	MGMT_PORT_GET_MAP,
	MGMT_PORT_FW_WRITE,
	MGMT_PORT_FW_CLONE,
	MGMT_PORT_FW_CLONE_CTRL,
	MGMT_PORT_CLEAR_ROUTING_TABLE,
	MGMT_PORT_ROUTING_TABLE_ENTRY,
	MGMT_PORT_EXTENSIONS,
	MGMT_PORT_TTY,
	MGMT_PORT_END = MGMT_PORT_TTY
} pqp_management_ports;

typedef enum{
	CUE_CHECKSUM_FW = 0,
	CUE_CHECKSUM_SECTION,
	CUE_ERASED_SECTION,
	CUE_CHECKSUM_ROUTING_TABLE
} pqp_management_port_checksum_cues;

typedef enum{
	CUE_ERASE_FIRMWARE = 0xAA,
	CUE_ERASE_FW_ALL = 0x55,
	CUE_ERASE_SECTION = 0x69
} pqp_management_port_erase_cues;

#define PQP_REBOOT_MAGIC		("ReB0")
#define PQP_BOOTLOADER_LATCH_MAGIC	("LaTC")
#define PQP_BOOTLOADER_FWUPD_MAGIC	("BlFg")
#define PQP_ERASE_MAGIC			("Er4S")
#define PQP_ERASE_ALL_MAGIC		("!EaL")
#define PQP_ERASE_SECTION_MAGIC		("eRSC")
#define PQP_WRITE_MAGIC			("wRT3")
#define PQP_WRITE_MAGIC_NOREPLY		("NyWr")
#define PQP_CLEAR_ROUTING_TABLE_MAGIC	("ClRt")

#define PQP_FW_CLONE_FLAG_SKIP_FF_BLOCK		0x01


#ifdef __cplusplus
}
#endif

#endif // LIBPQP_DEFINES_H
