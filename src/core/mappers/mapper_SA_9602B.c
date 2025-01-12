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
#include "irqA12.h"
#include "save_slot.h"

INLINE static void sa9602b_update_prg(void);

#define sa9602b_8000()\
	if (mmc3.prg_rom_cfg != old_prg_rom_cfg) {\
		mapper.rom_map_to[2] = sa9602b.prg_map[0];\
		mapper.rom_map_to[0] = sa9602b.prg_map[2];\
		sa9602b.prg_map[0] = mapper.rom_map_to[0];\
		sa9602b.prg_map[2] = mapper.rom_map_to[2];\
		sa9602b.prg_map[mmc3.prg_rom_cfg ^ 0x02] = 0x3E;\
	}
#define sa9602b_8001()\
	switch (mmc3.bank_to_update) {\
		case 0:\
		case 1:\
		case 2:\
		case 3:\
		case 4:\
		case 5:\
			sa9602b.reg[0] = value & 0xC0;\
			sa9602b.prg_map[mmc3.prg_rom_cfg] = sa9602b.reg[0] | (sa9602b.prg_map[mmc3.prg_rom_cfg] & 0x3F);\
			sa9602b.prg_map[1] = sa9602b.reg[0] | (sa9602b.prg_map[1] & 0x3F);\
			break;\
		case 6:\
			sa9602b.prg_map[mmc3.prg_rom_cfg] = sa9602b.reg[0] | (value & 0x3F);\
			break;\
		case 7:\
			sa9602b.prg_map[1] = sa9602b.reg[0] | (value & 0x3F);\
			break;\
	}

struct _sa9602b {
	WORD reg[4]; // ex prg_chip[4]
	WORD prg_map[4];
} sa9602b;

void map_init_SA_9602B(void) {
	EXTCL_CPU_WR_MEM(SA_9602B);
	EXTCL_SAVE_MAPPER(SA_9602B);
	EXTCL_CPU_EVERY_CYCLE(MMC3);
	EXTCL_PPU_000_TO_34X(MMC3);
	EXTCL_PPU_000_TO_255(MMC3);
	EXTCL_PPU_256_TO_319(MMC3);
	EXTCL_PPU_320_TO_34X(MMC3);
	EXTCL_UPDATE_R2006(MMC3);
	mapper.internal_struct[0] = (BYTE *)&sa9602b;
	mapper.internal_struct_size[0] = sizeof(sa9602b);
	mapper.internal_struct[1] = (BYTE *)&mmc3;
	mapper.internal_struct_size[1] = sizeof(mmc3);

	memset(&sa9602b, 0x00, sizeof(sa9602b));
	memset(&mmc3, 0x00, sizeof(mmc3));
	memset(&irqA12, 0x00, sizeof(irqA12));

	if (mapper.write_vram && !info.chr.rom.banks_8k) {
		info.chr.rom.banks_8k = 4;
	}

	sa9602b.prg_map[0] = 0;
	sa9602b.prg_map[1] = 1;
	sa9602b.prg_map[2] = 0x3E;
	sa9602b.prg_map[3] = 0x3F;

	sa9602b_update_prg();

	irqA12.present = TRUE;
	irqA12_delay = 1;
}
void extcl_cpu_wr_mem_SA_9602B(WORD address, BYTE value) {
	BYTE old_prg_rom_cfg = mmc3.prg_rom_cfg;

	switch (address & 0xE001) {
		case 0x8000:
			extcl_cpu_wr_mem_MMC3(address, value);
			sa9602b_8000()
			sa9602b_update_prg();
			return;
		case 0x8001:
			extcl_cpu_wr_mem_MMC3(address, value);
			sa9602b_8001()
			sa9602b_update_prg();
			return;
		default:
			extcl_cpu_wr_mem_MMC3(address, value);
			return;
	}
}
BYTE extcl_save_mapper_SA_9602B(BYTE mode, BYTE slot, FILE *fp) {
	save_slot_ele(mode, slot, sa9602b.reg);
	save_slot_ele(mode, slot, sa9602b.prg_map);
	extcl_save_mapper_MMC3(mode, slot, fp);

	return (EXIT_OK);
}

INLINE static void sa9602b_update_prg(void) {
	WORD value;

	value = sa9602b.prg_map[0];
	control_bank(info.prg.rom.max.banks_8k)
	map_prg_rom_8k(1, 0, value);

	value = sa9602b.prg_map[1];
	control_bank(info.prg.rom.max.banks_8k)
	map_prg_rom_8k(1, 1, value);

	value = sa9602b.prg_map[2];
	control_bank(info.prg.rom.max.banks_8k)
	map_prg_rom_8k(1, 2, value);

	value = sa9602b.prg_map[3];
	control_bank(info.prg.rom.max.banks_8k)
	map_prg_rom_8k(1, 3, value);

	map_prg_rom_8k_update();
}
