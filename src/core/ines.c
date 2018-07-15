/*
 *  Copyright (C) 2010-2017 Fabio Cavallo (aka FHorse)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ines.h"
#include "fds.h"
#include "mem_map.h"
#include "mappers.h"
#include "emu.h"
#include "conf.h"
#include "clock.h"
#include "cheat.h"
#include "info.h"
#include "vs_system.h"

enum flags { FL6, FL7, FL8, FL9, FL10, FL11, FL12, FL13, FL14, FL15, TOTAL_FL };

void nes20_submapper(void);
BYTE nes20_ram_size(BYTE mode);

BYTE ines_load_rom(void) {
	BYTE tmp, flags[TOTAL_FL];
	FILE *fp;

	{
		BYTE i, found = TRUE;
		static const uTCHAR rom_ext[2][10] = { uL(".nes\0"), uL(".NES\0") };

		fp = ufopen(info.rom_file, uL("rb"));

		if (!fp) {
			found = FALSE;

			for (i = 0; i < LENGTH(rom_ext); i++) {
				uTCHAR rom_file[LENGTH_FILE_NAME_LONG];

				umemset(rom_file, 0x00, usizeof(rom_file));
				umemcpy(rom_file, info.rom_file, usizeof(rom_file) - 10 - 1);
				ustrcat(rom_file, rom_ext[i]);

				fp = ufopen(rom_file, uL("rb"));

				if (fp) {
					ustrncpy(info.rom_file, rom_file, usizeof(info.rom_file));
					found = TRUE;
					break;
				}
			}
		}

		if (!found) {
			return (EXIT_ERROR);
		}
	}

	if (cfg->cheat_mode == GAMEGENIE_MODE) {
		fp = gamegenie_load_rom(fp);
	}

	if ((fgetc(fp) == 'N') && (fgetc(fp) == 'E') && (fgetc(fp) == 'S') && (fgetc(fp) == '\32')) {
		info.prg.rom[0].banks_16k = fgetc(fp);
		info.chr.rom[0].banks_8k = fgetc(fp);

		if (!(fread(&flags[0], TOTAL_FL, 1, fp))) {
			;
		}

		if ((flags[FL7] & 0x0C) == 0x08) {
			/* NES 2.0 */
			info.format = NES_2_0;

			/*
			 * visto che con il NES_2_0 non eseguo la ricerca nel
			 * database inizializzo queste variabili.
			 */
			info.mirroring_db = info.id = DEFAULT;
			info.extra_from_db = 0;

			info.mapper.id = ((flags[FL8] & 0x0F) << 8) | (flags[FL7] & 0xF0) | (flags[FL6] >> 4);
			info.mapper.submapper = (flags[FL8] & 0xF0) >> 4;

			/* Submapper number. Mappers not using submappers set this to zero. */
			if (info.mapper.submapper == 0) {
				info.mapper.submapper = DEFAULT;
			}

			nes20_submapper();

			info.prg.rom[0].banks_16k |= ((flags[FL9] & 0x0F) << 8);
			info.chr.rom[0].banks_8k |= ((flags[FL9] & 0xF0) << 4);

			info.prg.ram.banks_8k_plus = nes20_ram_size(flags[FL10] & 0x0F);
			info.prg.ram.bat.banks = nes20_ram_size(flags[FL10] >> 4);

			if (info.prg.ram.bat.banks && !info.prg.ram.banks_8k_plus) {
				info.prg.ram.banks_8k_plus = info.prg.ram.bat.banks;
			}

			tmp = flags[FL12] & 0x01;

			vs_system.ppu = flags[FL13] & 0x0F;
			vs_system.special_mode.type = (flags[FL13] >> 4) & 0x0F;
		} else {
			/* iNES 1.0 */
			info.format = iNES_1_0;

			info.mapper.id = (flags[FL7] & 0xF0) | (flags[FL6] >> 4);
			info.prg.ram.bat.banks = (flags[FL6] & 0x02) >> 1;

			if (info.prg.ram.bat.banks) {
				info.prg.ram.banks_8k_plus = 1;
			}

			tmp = flags[FL9] & 0x01;
		}

		switch (tmp) {
			case 0:
				info.machine[HEADER] = NTSC;
				break;
			case 1:
				info.machine[HEADER] = PAL;
				break;
		}

		info.trainer = flags[FL6] & 0x04;

		if (flags[FL6] & 0x08) {
			mirroring_FSCR();
		} else {
			if (flags[FL6] & 0x01) {
				mirroring_V();
			} else {
				mirroring_H();
			}
		}

		/*
		 * inizializzo qui il writeVRAM per la mapper 96 perche'
		 * e' l'unica mapper che utilizza 32k di CHR Ram e che
		 * si permette anche il lusso di swappare. Quindi imposto
		 * a FALSE qui in modo da poter cambiare impostazione nel
		 * emu_search_in_database.
		 */
		mapper.write_vram = FALSE;

		if ((info.format != NES_2_0) && emu_search_in_database(fp)) {
			fclose(fp);
			return (EXIT_ERROR);
		}

		// gestione Vs. System
		if ((info.mapper.id != 99) && !vs_system.ppu && !vs_system.special_mode.type) {
			vs_system.enabled = FALSE;
			vs_system.special_mode.r5e0x = NULL;
			vs_system.special_mode.index = 0;
			vs_system.rc2c05.enabled = FALSE;
		} else {
			vs_system.enabled = TRUE;

			switch(vs_system.ppu) {
				case RP2C03B:
				case RP2C03G:
				case RP2C04:
				case RP2C04_0002:
				case RP2C04_0003:
				case RP2C04_0004:
				case RC2C03B:
				case RC2C03C:
				default:
					vs_system.rc2c05.enabled = FALSE;
					vs_system.rc2c05.r2002 = 0x00;
					break;
				case RC2C05_01:
					vs_system.rc2c05.enabled = TRUE;
					vs_system.rc2c05.r2002 = 0x1B;
					break;
				case RC2C05_02:
					vs_system.rc2c05.enabled = TRUE;
					vs_system.rc2c05.r2002 = 0x3D;
					break;
				case RC2C05_03:
					vs_system.rc2c05.enabled = TRUE;
					vs_system.rc2c05.r2002 = 0x1C;
					break;
				case RC2C05_04:
					vs_system.rc2c05.enabled = TRUE;
					vs_system.rc2c05.r2002 = 0x1B;
					break;
				case RC2C05_05:
					vs_system.rc2c05.enabled = TRUE;
					vs_system.rc2c05.r2002 = 0x00;
					break;
			}

			switch(vs_system.special_mode.type) {
				case VS_SM_Normal:
				default:
					vs_system.special_mode.r5e0x = NULL;
					break;
				case VS_SM_RBI_Baseball:
					vs_system.special_mode.r5e0x = (BYTE *) &vs_protection_data[1][0];
					break;
				case VS_SM_TKO_Boxing:
					vs_system.special_mode.r5e0x = (BYTE *) &vs_protection_data[0][0];
					break;
				case VS_SM_Super_Xevious:
					vs_system.special_mode.r5e0x = NULL;
					break;
			}
			vs_system.special_mode.index = 0;
		}

		if (info.trainer) {
			if (!(fread(&trainer.data, sizeof(trainer.data), 1, fp))) {
				;
			}
		} else {
			memset(&trainer.data, 0x00, sizeof(trainer.data));
		}

#if !defined (RELEASE)
		fprintf(stderr, "mapper %u\n8k rom = %u\n4k vrom = %u\n", info.mapper.id,
				info.prg.rom[0].banks_16k * 2, info.chr.rom[0].banks_8k * 2);
		fprintf(stderr, "sha1prg = %40s\n", info.sha1sum.prg.string);
		fprintf(stderr, "sha1chr = %40s\n", info.sha1sum.chr.string);
#endif

		if (!info.chr.rom[0].banks_8k) {
			mapper.write_vram = TRUE;
			if (info.format == NES_2_0) {
				info.chr.rom[0].banks_8k = nes20_ram_size(flags[FL11] & 0x0F);
			}
			if (!info.chr.rom[0].banks_8k) {
				if (info.format == iNES_1_0) {
					if (info.extra_from_db & CHRRAM32K) {
						info.chr.rom[0].banks_8k = 4;
					} else if (info.extra_from_db & CHRRAM256K) {
						info.chr.rom[0].banks_8k = 32;
					} else {
						info.chr.rom[0].banks_8k = 1;
					}
				} else {
					info.chr.rom[0].banks_8k = 1;
				}
			}
		}
		info.prg.rom[0].banks_8k = info.prg.rom[0].banks_16k * 2;
		info.chr.rom[0].banks_4k = info.chr.rom[0].banks_8k * 2;
		info.chr.rom[0].banks_1k = info.chr.rom[0].banks_4k * 4;

		map_set_banks_max_prg(0);
		map_set_banks_max_chr(0);
		info.prg.chips = info.chr.chips = 0;

		/* alloco la PRG Ram */
		if (map_prg_ram_malloc(0x2000) != EXIT_OK) {
			fclose(fp);
			return (EXIT_ERROR);
		}

		/* alloco e carico la PRG Rom */
		if (map_prg_chip_malloc(0, info.prg.rom[0].banks_16k * 0x4000, 0x00) == EXIT_ERROR) {
			fclose(fp);
			return (EXIT_ERROR);
		}

		if (!(fread(prg_chip(0), 16384, info.prg.rom[0].banks_16k, fp))) {
			;
		}

		/*
		 * se e' settato mapper.write_vram, vuol dire
		 * che la rom non ha CHR Rom e che quindi la CHR Ram
		 * la trattero' nell'inizializzazione della mapper
		 * (perche' alcune mapper ne hanno 16k, altre 8k).
		 */
		if (mapper.write_vram == FALSE) {
			/* alloco la CHR Rom */
			if (map_chr_chip_malloc(0, info.chr.rom[0].banks_8k * 0x2000, 0x00) == EXIT_ERROR) {
				fclose(fp);
				return (EXIT_ERROR);
			}

			if (!(fread(chr_chip(0), 0x2000, info.chr.rom[0].banks_8k, fp))) {
				;
			}
			map_chr_bank_1k_reset();
		}

		info.prg.max_chips = info.prg.chips - 1;
		if (info.chr.chips > 0) {
			info.chr.max_chips = info.chr.chips - 1;
		}

		/* la CHR ram extra */
		memset(&chr.extra, 0x00, sizeof(chr.extra));
	} else {
		fprintf(stderr, "Format not supported.\n");
		fclose(fp);
		return (EXIT_ERROR);
	}
	fclose(fp);

	return (EXIT_OK);
}

void nes20_submapper(void) {
	switch (info.mapper.id) {
		case 2:
			switch (info.mapper.submapper) {
				case 0:
				case 1:
					info.mapper.submapper = UNLROM;
					break;
				case 2:
					info.mapper.submapper = UXROM;
					break;
			}
			break;
		case 3:
			switch (info.mapper.submapper) {
				case 0:
				case 1:
					info.mapper.submapper = DEFAULT;
					break;
				case 2:
					info.mapper.submapper = CNROM_CNFL;
					break;
			}
			break;
		case 7:
			switch (info.mapper.submapper) {
				case 0:
				case 1:
					info.mapper.submapper = DEFAULT;
					break;
				case 2:
					info.mapper.submapper = AMROM;
					break;
			}
			break;
		case 78:
			switch (info.mapper.submapper) {
				case 3:
					info.mapper.submapper = HOLYDIVER;
					break;
			}
			break;
	}
}
BYTE nes20_ram_size(BYTE mode) {
	switch (mode) {
		case 0:
			return (0);
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			return (1);
		case 8:
			return (2);
		case 9:
			return (4);
		case 10:
			return (8);
		case 11:
			return (16);
		case 12:
			return (32);
		case 13:
			return (64);
		case 14:
			return (128);
		case 15:
			return (0);
	}
	return (0);
}
