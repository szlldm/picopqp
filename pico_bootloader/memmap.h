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

#ifndef MEMMAP_H
#define MEMMAP_H

#define PROTECTED_DATA_OFFSET	65536	// 64k; 2 sector
#define ROUTING_TABLE_OFFSET	73728	// 72k; 1 sector
#define USER_CONFIG_OFFSET	77824	// 76k; 5 sector
#define APP_OFFSET		98304	// 96k

#define FLASH_SIZE		2097152 // 2048k
#define APP_SIZE		(FLASH_SIZE - APP_OFFSET)

#endif //MEMMAP_H
