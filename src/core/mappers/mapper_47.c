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

#define m47_chr_1k(vl) bank = (m47.reg << 7) | (vl & 0x7F)
#define m47_prg_8k(vl) value = (m47.reg << 4) | (vl & 0x0F)
#define m47_chr_1k_update()\
{\
	BYTE i;\
	for (i = 0; i < 8; i++) {\
		WORD bank;\
		m47_chr_1k(m47.chr_map[i]);\
		_control_bank(bank, info.chr.rom.max.banks_1k)\
		chr.bank_1k[i] = chr_pnt(bank << 10);\
	}\
}
#define m47_prg_8k_update()\
{\
	BYTE i;\
	for (i = 0; i < 4; i++) {\
		m47_prg_8k(m47.prg_map[i]);\
		control_bank(info.prg.rom.max.banks_8k)\
		map_prg_rom_8k(1, i, value);\
	}\
	map_prg_rom_8k_update();\
}
#define m47_swap_chr_bank_1k(src, dst)\
{\
	BYTE *chr_bank_1k = chr.bank_1k[src];\
	chr.bank_1k[src] = chr.bank_1k[dst];\
	chr.bank_1k[dst] = chr_bank_1k;\
	WORD map = m47.chr_map[src];\
	m47.chr_map[src] = m47.chr_map[dst];\
	m47.chr_map[dst] = map;\
}
#define m47_8000()\
{\
	const BYTE chr_rom_cfg_old = mmc3.chr_rom_cfg;\
	const BYTE prg_rom_cfg_old = mmc3.prg_rom_cfg;\
	mmc3.bank_to_update = value & 0x07;\
	mmc3.prg_rom_cfg = (value & 0x40) >> 5;\
	mmc3.chr_rom_cfg = (value & 0x80) >> 5;\
	/*\
	 * se il tipo di configurazione della chr cambia,\
	 * devo swappare i primi 4 banchi con i secondi\
	 * quattro.\
	 */\
	if (mmc3.chr_rom_cfg != chr_rom_cfg_old) {\
		m47_swap_chr_bank_1k(0, 4)\
		m47_swap_chr_bank_1k(1, 5)\
		m47_swap_chr_bank_1k(2, 6)\
		m47_swap_chr_bank_1k(3, 7)\
	}\
	if (mmc3.prg_rom_cfg != prg_rom_cfg_old) {\
		WORD p0 = mapper.rom_map_to[0];\
		WORD p2 = mapper.rom_map_to[2];\
		mapper.rom_map_to[0] = p2;\
		mapper.rom_map_to[2] = p0;\
		p0 = m47.prg_map[0];\
		p2 = m47.prg_map[2];\
		m47.prg_map[0] = p2;\
		m47.prg_map[2] = p0;\
		m47.prg_map[mmc3.prg_rom_cfg ^ 0x02] = info.prg.rom.max.banks_8k_before_last;\
		/*\
		 * prg_rom_cfg 0x00 : $C000 - $DFFF fisso al penultimo banco\
		 * prg_rom_cfg 0x02 : $8000 - $9FFF fisso al penultimo banco\
		 */\
		m47_prg_8k(info.prg.rom.max.banks_8k_before_last);\
		control_bank(info.prg.rom.max.banks_8k)\
		map_prg_rom_8k(1, mmc3.prg_rom_cfg ^ 0x02, value);\
		map_prg_rom_8k_update();\
	}\
}
#define m47_8001()\
{\
	WORD bank;\
	switch (mmc3.bank_to_update) {\
		case 0:\
			m47.chr_map[mmc3.chr_rom_cfg] = value;\
			m47.chr_map[mmc3.chr_rom_cfg | 0x01] = value + 1;\
			m47_chr_1k(value);\
			bank &= 0xFFE;\
			_control_bank(bank, info.chr.rom.max.banks_1k)\
			chr.bank_1k[mmc3.chr_rom_cfg] = chr_pnt(bank << 10);\
			chr.bank_1k[mmc3.chr_rom_cfg | 0x01] = chr_pnt((bank + 1) << 10);\
			return;\
		case 1:\
			m47.chr_map[mmc3.chr_rom_cfg | 0x02] = value;\
			m47.chr_map[mmc3.chr_rom_cfg | 0x03] = value + 1;\
			m47_chr_1k(value);\
			bank &= 0xFFE;\
			_control_bank(bank, info.chr.rom.max.banks_1k)\
			chr.bank_1k[mmc3.chr_rom_cfg | 0x02] = chr_pnt(bank << 10);\
			chr.bank_1k[mmc3.chr_rom_cfg | 0x03] = chr_pnt((bank + 1) << 10);\
			return;\
		case 2:\
			m47.chr_map[mmc3.chr_rom_cfg ^ 0x04] = value;\
			m47_chr_1k(value);\
			_control_bank(bank, info.chr.rom.max.banks_1k)\
			chr.bank_1k[mmc3.chr_rom_cfg ^ 0x04] = chr_pnt(bank << 10);\
			return;\
		case 3:\
			m47.chr_map[(mmc3.chr_rom_cfg ^ 0x04) | 0x01] = value;\
			m47_chr_1k(value);\
			_control_bank(bank, info.chr.rom.max.banks_1k)\
			chr.bank_1k[(mmc3.chr_rom_cfg ^ 0x04) | 0x01] = chr_pnt(bank << 10);\
			return;\
		case 4:\
			m47.chr_map[(mmc3.chr_rom_cfg ^ 0x04) | 0x02] = value;\
			m47_chr_1k(value);\
			_control_bank(bank, info.chr.rom.max.banks_1k)\
			chr.bank_1k[(mmc3.chr_rom_cfg ^ 0x04) | 0x02] = chr_pnt(bank << 10);\
			return;\
		case 5:\
			m47.chr_map[(mmc3.chr_rom_cfg ^ 0x04) | 0x03] = value;\
			m47_chr_1k(value);\
			_control_bank(bank, info.chr.rom.max.banks_1k)\
			chr.bank_1k[(mmc3.chr_rom_cfg ^ 0x04) | 0x03] = chr_pnt(bank << 10);\
			return;\
		case 6:\
			m47.prg_map[mmc3.prg_rom_cfg] = value;\
			m47_prg_8k(value);\
			control_bank(info.prg.rom.max.banks_8k)\
			map_prg_rom_8k(1, mmc3.prg_rom_cfg, value);\
			map_prg_rom_8k_update();\
			return;\
		case 7:\
			m47.prg_map[1] = value;\
			m47_prg_8k(value);\
			control_bank(info.prg.rom.max.banks_8k)\
			map_prg_rom_8k(1, 1, value);\
			map_prg_rom_8k_update();\
			return;\
	}\
}

struct _m47 {
	BYTE reg;
	WORD prg_map[4];
	WORD chr_map[8];
} m47;

void map_init_47(void) {
	EXTCL_CPU_WR_MEM(47);
	EXTCL_SAVE_MAPPER(47);
	EXTCL_CPU_EVERY_CYCLE(MMC3);
	EXTCL_PPU_000_TO_34X(MMC3);
	EXTCL_PPU_000_TO_255(MMC3);
	EXTCL_PPU_256_TO_319(MMC3);
	EXTCL_PPU_320_TO_34X(MMC3);
	EXTCL_UPDATE_R2006(MMC3);
	mapper.internal_struct[0] = (BYTE *)&m47;
	mapper.internal_struct_size[0] = sizeof(m47);
	mapper.internal_struct[1] = (BYTE *)&mmc3;
	mapper.internal_struct_size[1] = sizeof(mmc3);

	memset(&mmc3, 0x00, sizeof(mmc3));
	memset(&irqA12, 0x00, sizeof(irqA12));
	memset(&m47, 0x00, sizeof(m47));

	{
		BYTE value, i;

		map_prg_rom_8k_reset();
		map_chr_bank_1k_reset();

		for (i = 0; i < 8; i++) {
			if (i < 4) {
				m47.prg_map[i] = mapper.rom_map_to[i];
			}
			m47.chr_map[i] = i;
		}

		m47_prg_8k_update()
		m47_chr_1k_update()
	}

	info.mapper.extend_wr = TRUE;

	irqA12.present = TRUE;
	irqA12_delay = 1;
}
void extcl_cpu_wr_mem_47(WORD address, BYTE value) {
	if (address > 0x7FFF) {
		switch (address & 0xE001) {
			case 0x8000:
				m47_8000()
				return;
			case 0x8001:
				m47_8001()
				return;
		}
		extcl_cpu_wr_mem_MMC3(address, value);
		return;
	}

	if (address >= 0x6000) {
		value &= 0x01;

		if (m47.reg != value) {
			m47.reg = value;

			m47_prg_8k_update()
			m47_chr_1k_update()
		}
	}
}
BYTE extcl_save_mapper_47(BYTE mode, BYTE slot, FILE *fp) {
	save_slot_ele(mode, slot, m47.reg);
	save_slot_ele(mode, slot, m47.prg_map);
	save_slot_ele(mode, slot, m47.chr_map);
	extcl_save_mapper_MMC3(mode, slot, fp);

	return (EXIT_OK);
}
