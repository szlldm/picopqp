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

// pqp_defines.h

#ifndef PQP_DEFINES
#define PQP_DEFINES

#include "pqp_addresses.h"

#define MY_BASE_ADDRESS			(PQP_USB_BRIDGE)

#define PQP_NUMBER_OF_TX_BUFFERS		(64)
#define PQP_NUMBER_OF_RX_BUFFERS		(64)
#define PQP_SEND_TTL				(6)



// default interface is NULL
// additional interfaces:
#define LIBPQP_HAS_CAN1
#define LIBPQP_HAS_HDUPLEX_UART1
//#define LIBPQP_HAS_I2C1
//#define LIBPQP_HAS_CAN2
//#define LIBPQP_HAS_HDUPLEX_UART2
//#define LIBPQP_HAS_I2C2

// for Pi Pico stdio test:
#define LIBPQP_HAS_STDIOIF

// CAN interface options:
#define LIBPQP_IF_CAN_RX_FRAGMENT_BUFFER_SIZE		(256)		// fragments (RAM: size * 12bytes)
#define LIBPQP_IF_CAN_RX_MAX_PACKET_BUFFERS		(8)		// simultaneous packet storage
#define CAN2040_RX_IRQ_RING_BUFFER_SIZE			(64)		// fragments (RAM: size * 16bytes)
#define LIBPQP_IF_CAN_RX_DISCARD_INCOMPLETE_PACKETS_AFTER_MS	(30000)		// 30 sec
#define LIBPQP_IF_CAN_TX_RESET_TRANSMITTER_AFTER_MS		(20000)		// 20 sec

#define CAN2040_CAN_BITRATE				(500000)
#define CAN2040_CAN1_GPIO_RX				(17)
#define CAN2040_CAN1_GPIO_TX				(16)
#define CAN2040_CAN2_GPIO_RX				(6)
#define CAN2040_CAN2_GPIO_TX				(7)

// HDUART interface options:
#define HDUART_BAUD_RATE				(100000)
#define HDUART_RX_IRQ_RING_BUFFER_SIZE			(256)
#define UART0_HDUART1_GPIO_RX				(1)
#define UART0_HDUART1_GPIO_TX				(0)
#define UART1_HDUART2_GPIO_RX				(5)
#define UART1_HDUART2_GPIO_TX				(4)

// I2C interface options
#define I2C_RX_IRQ_RING_BUFFER_SIZE			(256)
#define I2C_BITRATE					(100000)
#define I2C0_I2CI1_GPIO_SDA				(16)
#define I2C0_I2CI1_GPIO_SCL				(17)
#define I2C1_I2CI2_GPIO_SDA				(14)
#define I2C1_I2CI2_GPIO_SCL				(15)

#endif // PQP_DEFINES

