/*
 *  Copyright (C) 2010-2023 Fabio Cavallo (aka FHorse)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "mappers.h"
#include "info.h"
#include "mem_map.h"

void map_init_225(void) {
	EXTCL_CPU_WR_MEM(225);

	if (info.reset >= HARD) {
		map_prg_rom_8k(4, 0, 0);
	}
}
void extcl_cpu_wr_mem_225(WORD address, BYTE value) {
	DBWORD bank;

	value = (address >> 7) & 0x1F;

	if (info.prg.rom.max.banks_32k > 0x1F) {
		value |= ((address >> 9) & 0x20);
	}

	if (address & 0x1000) {
		value = (value << 1) | ((address >> 6) & 0x01);
		control_bank(info.prg.rom.max.banks_16k)
		map_prg_rom_8k(2, 0, value);
		map_prg_rom_8k(2, 2, value);
	} else {
		control_bank(info.prg.rom.max.banks_32k)
		map_prg_rom_8k(4, 0, value);
	}
	map_prg_rom_8k_update();

	if (address & 0x2000) {
		mirroring_H();
		/*
		 * workaround per far funzionare correttamente il 52esimo gioco
		 * della rom "52 Games [p1].nes" che non utilizza ne il mirroring
		 * verticale ne quello orizzontale.
		 */
		if ((info.id == BMC52IN1) && (address == 0xA394)) {
			mirroring_FSCR();
		}
	} else {
		mirroring_V();
	}

	value = address & 0x3F;

	if (info.chr.rom.max.banks_8k > 0x3F) {
		value |= ((address >> 8) & 0x40);
	}

	control_bank(info.chr.rom.max.banks_8k)
	bank = value << 13;
	chr.bank_1k[0] = chr_pnt(bank);
	chr.bank_1k[1] = chr_pnt(bank | 0x0400);
	chr.bank_1k[2] = chr_pnt(bank | 0x0800);
	chr.bank_1k[3] = chr_pnt(bank | 0x0C00);
	chr.bank_1k[4] = chr_pnt(bank | 0x1000);
	chr.bank_1k[5] = chr_pnt(bank | 0x1400);
	chr.bank_1k[6] = chr_pnt(bank | 0x1800);
	chr.bank_1k[7] = chr_pnt(bank | 0x1C00);
}
