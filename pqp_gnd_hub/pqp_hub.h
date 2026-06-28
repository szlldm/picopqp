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

#ifndef PQP_HUB_H
#define PQP_HUB_H


#define MAX_DEVICE_ADDR		(120)

#define SRC_ADDR		(0)		// GND address, valid: 0 or 1

#define TCP_PORT_OFFSET		10000

#define TXRX_QUEUE_LEN		(256)

#define SERVER_MAX_CONN		(3)	// per TCP port

#endif // PQP_HUB_H
