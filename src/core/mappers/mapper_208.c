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

#include <string.h>
#include "mappers.h"
#include "info.h"
#include "mem_map.h"
#include "cpu.h"
#include "irqA12.h"
#include "save_slot.h"

struct _m208 {
	BYTE ctrl;
	BYTE reg[4];
} m208;
static const BYTE vlu208[256] = {
		0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59,
		0x59, 0x49, 0x19, 0x09, 0x59, 0x49, 0x19, 0x09,
		0x59, 0x59, 0x59, 0x59,	0x59, 0x59, 0x59, 0x59,
		0x51, 0x41, 0x11, 0x01, 0x51, 0x41, 0x11, 0x01,
		0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59,
		0x59, 0x49, 0x19, 0x09,	0x59, 0x49, 0x19, 0x09,
		0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59,
		0x51, 0x41, 0x11, 0x01, 0x51, 0x41, 0x11, 0x01,
		0x00, 0x10, 0x40, 0x50,	0x00, 0x10, 0x40, 0x50,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x18, 0x48, 0x58, 0x08, 0x18, 0x48, 0x58,
		0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00,
		0x00, 0x10, 0x40, 0x50, 0x00, 0x10, 0x40, 0x50,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x18, 0x48, 0x58,	0x08, 0x18, 0x48, 0x58,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59,
		0x58, 0x48, 0x18, 0x08,	0x58, 0x48, 0x18, 0x08,
		0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59,
		0x50, 0x40, 0x10, 0x00, 0x50, 0x40, 0x10, 0x00,
		0x59, 0x59, 0x59, 0x59,	0x59, 0x59, 0x59, 0x59,
		0x58, 0x48, 0x18, 0x08, 0x58, 0x48, 0x18, 0x08,
		0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59,
		0x50, 0x40, 0x10, 0x00,	0x50, 0x40, 0x10, 0x00,
		0x01, 0x11, 0x41, 0x51, 0x01, 0x11, 0x41, 0x51,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x09, 0x19, 0x49, 0x59,	0x09, 0x19, 0x49, 0x59,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x11, 0x41, 0x51, 0x01, 0x11, 0x41, 0x51,
		0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00,
		0x09, 0x19, 0x49, 0x59, 0x09, 0x19, 0x49, 0x59,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void map_init_208(void) {
	EXTCL_CPU_WR_MEM(208);
	EXTCL_CPU_RD_MEM(208);
	EXTCL_SAVE_MAPPER(208);
	EXTCL_CPU_EVERY_CYCLE(MMC3);
	EXTCL_PPU_000_TO_34X(MMC3);
	EXTCL_PPU_000_TO_255(MMC3);
	EXTCL_PPU_256_TO_319(MMC3);
	EXTCL_PPU_320_TO_34X(MMC3);
	EXTCL_UPDATE_R2006(MMC3);
	mapper.internal_struct[0] = (BYTE *)&m208;
	mapper.internal_struct_size[0] = sizeof(m208);
	mapper.internal_struct[1] = (BYTE *)&mmc3;
	mapper.internal_struct_size[1] = sizeof(mmc3);

	info.mapper.extend_wr = TRUE;

	if (info.reset >= HARD) {
		memset(&m208, 0x00, sizeof(m208));
		memset(&mmc3, 0x00, sizeof(mmc3));

		/* sembra proprio che parta in questa modalita' */
		mmc3.prg_rom_cfg = 0x02;
	}

	memset(&irqA12, 0x00, sizeof(irqA12));

	irqA12.present = TRUE;
	irqA12_delay = 1;
}
void extcl_cpu_wr_mem_208(WORD address, BYTE value) {
	if (address >= 0x8000) {
		extcl_cpu_wr_mem_MMC3(address, value);
		return;
	}

	if ((address < 0x4800) || (address > 0x5FFF)) {
		return;
	}

	switch (address & 0xF800) {
		case 0x4800:
			value = ((value >> 3) & 0x02) | (value & 0x01);
			control_bank(info.prg.rom.max.banks_32k)
			map_prg_rom_8k(4, 0, value);
			map_prg_rom_8k_update();
			return;
		case 0x5000:
			m208.ctrl = value;
			return;
		case 0x5800:
			m208.reg[address & 0x0003] = value ^ vlu208[m208.ctrl];
			return;
	}
}
BYTE extcl_cpu_rd_mem_208(WORD address, BYTE openbus, UNUSED(BYTE before)) {
	if ((address < 0x5800) || (address > 0x5FFF)) {
		return (openbus);
	}

	return (m208.reg[address & 0x0003]);
}
BYTE extcl_save_mapper_208(BYTE mode, BYTE slot, FILE *fp) {
	save_slot_ele(mode, slot, m208.ctrl);
	save_slot_ele(mode, slot, m208.reg);
	extcl_save_mapper_MMC3(mode, slot, fp);

	return (EXIT_OK);
}
