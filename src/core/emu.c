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

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "main.h"
#include "debugger.h"
#include "emu.h"
#include "settings.h"
#include "clock.h"
#include "cpu.h"
#include "mem_map.h"
#include "ppu.h"
#include "video/gfx_thread.h"
#include "mappers.h"
#include "vs_system.h"
#include "version.h"
#include "conf.h"
#include "save_slot.h"
#include "tas.h"
#include "ines.h"
#include "unif.h"
#include "fds.h"
#include "nsfe.h"
#include "patcher.h"
#include "recent_roms.h"
#include "../../c++/crc/crc.h"
#include "gui.h"
#include "video/effects/tv_noise.h"
#if defined (FULLSCREEN_RESFREQ)
#include "video/gfx_monitor.h"
#endif

#define RS_SCALE (1.0f / (1.0f + (float)RAND_MAX))

#if defined (DEBUG)
WORD PCBREAK = 0xC425;
#endif

INLINE static void emu_frame_started(void);
INLINE static void emu_frame_finished(void);
INLINE static void emu_frame_sleep(void);

static void emu_cpu_initial_cycles(void);
static BYTE emu_ctrl_if_rom_exist(void);
static uTCHAR *emu_ctrl_rom_ext(uTCHAR *file);
static void emu_recent_roms_add(BYTE *add, uTCHAR *file);

void emu_quit(void) {
	if (cfg->save_on_exit) {
		settings_save();
	}

#if defined (WITH_FFMPEG)
	recording_quit();
#endif

	map_quit();

	cheatslist_quit();
	nsf_quit();
	fds_quit();
	ppu_quit();
	snd_quit();
	gfx_quit();

	rewind_quit();

	js_quit(TRUE);

	gamegenie_quit();
	uncompress_quit();
	patcher_quit();

#if defined (FULLSCREEN_RESFREQ)
	if (gfx.type_of_fscreen_in_use == FULLSCR) {
		gfx_monitor_restore_res();
	}
	gfx_monitor_quit();
#endif

	chinaersan2_quit();

	gui_quit();
}
void emu_frame(void) {
	// gestione uscita
	if (info.stop) {
		return;
	}

	info.start_frame_0 = FALSE;

	gui_control_visible_cursor();

	// eseguo un frame dell'emulatore
	if (info.no_rom) {
		tv_noise_effect();
		gfx_draw_screen();
		emu_frame_sleep();
		return;
	} else if (nsf.state & (NSF_PAUSE | NSF_STOP)) {
		int i;

		gui_decode_all_input_events();

		for (i = PORT1; i < PORT_MAX; i++) {
			if (port_funct[i].input_add_event) {
				port_funct[i].input_add_event(i);
			}
		}

		extcl_audio_samples_mod_nsf(NULL, 0);
		nsf_main_screen_event();
		nsf_effect();
		gfx_draw_screen();
		emu_frame_sleep();
		return;
	}

	// se nel debugger passo dal DBG_STEP al DBG_GO
	// arrivo qui che l'emu_frame_started e' stato gia' fatto
	// e non devo ripeterlo (soprattuto per il TAS). Lo capisco
	// dal info.frame_status che e' gia' impostato su FRAME_STARTED.
	if (info.frame_status == FRAME_FINISHED) {
		emu_frame_started();
	}

	while (info.frame_status == FRAME_STARTED) {
#if defined (DEBUG)
		if (cpu.PC == PCBREAK) {
			BYTE pippo = 5;
			pippo = pippo + 1;
		}
#endif
		// eseguo CPU, PPU e APU
		cpu_exe_op();
	}

	emu_frame_finished();
	emu_frame_sleep();
	emu_frame_input_and_rewind();
}
void emu_frame_debugger(void) {
	if (info.stop || (debugger.mode >= DBG_BREAKPOINT)) {
		return;
	}

	if (info.frame_status == FRAME_FINISHED) {
		if (debugger.mode == DBG_GO) {
			if (info.no_rom) {
				tv_noise_effect();
				gfx_draw_screen();
				emu_frame_sleep();
				return;
			} else if (nsf.state & (NSF_PAUSE | NSF_STOP)) {
				int i;

				gui_decode_all_input_events();

				for (i = PORT1; i < PORT_MAX; i++) {
					if (port_funct[i].input_add_event) {
						port_funct[i].input_add_event(i);
					}
				}

				extcl_audio_samples_mod_nsf(NULL, 0);
				nsf_main_screen_event();
				nsf_effect();
				gfx_draw_screen();
				emu_frame_sleep();
				return;
			}
		}

		if (info.start_frame_0) {
			info.start_frame_0 = FALSE;
			goto emu_frame_debugger_start_frame_0;
		}

		emu_frame_started();
	}

	// eseguo CPU, PPU e APU
	if (debugger.mode == DBG_GO) {
		// posso passare dal DBG_GO al DBG_STEP durante l'esecuzione di un frame intero
		while ((info.frame_status == FRAME_STARTED) && (debugger.mode == DBG_GO)) {
			if ((debugger.breakpoint == cpu.PC) && !debugger.breakpoint_after_step) {
				debugger.mode = DBG_BREAKPOINT;
				//gui_dlgdebugger_click_step();
				break;
			} else {
				debugger.breakpoint_after_step = FALSE;
				info.CPU_PC_before = cpu.PC;
				cpu_exe_op();
			}
		}
	} else if (debugger.mode == DBG_STEP) {
		info.CPU_PC_before = cpu.PC;
		cpu_exe_op();
	}

	if (info.frame_status == FRAME_FINISHED) {
		emu_frame_finished();

		if (debugger.mode == DBG_GO) {
			emu_frame_sleep();
		}

		emu_frame_input_and_rewind();

emu_frame_debugger_start_frame_0:
		if (debugger.breakframe) {
			debugger.mode = DBG_BREAKPOINT;
			debugger.breakframe = FALSE;
			//gui_dlgdebugger_click_step();
		}
	}
}
BYTE emu_make_dir(const uTCHAR *fmt, ...) {
	static uTCHAR path[LENGTH_FILE_NAME_MID];
	struct ustructstat status;
	va_list ap;

	va_start(ap, fmt);
	uvsnprintf(path, usizeof(path), fmt, ap);
	va_end(ap);

	if (!(uaccess(path, 0))) {
		// se esiste controllo che sia una directory
		ustat(path, &status);

		if (!(status.st_mode & S_IFDIR)) {
			// non e' una directory
			return (EXIT_ERROR);
		}
	} else {
#if defined (_WIN32)
		if (_wmkdir(path)) {
			return (EXIT_ERROR);
		}
#else
		if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
			return (EXIT_ERROR);
		}
#endif
	}

	return (EXIT_OK);
}
BYTE emu_file_exist(const uTCHAR *file) {
	struct ustructstat status;

	if (!(uaccess(file, 0))) {
		ustat(file, &status);
		if (status.st_mode & S_IFREG) {
			return (EXIT_OK);
		}
	}
	return (EXIT_ERROR);
}
char *emu_file2string(const uTCHAR *path) {
	FILE *fd;
	size_t len;
	char *str;

	if (!(fd = ufopen(path, uL("r")))) {
		log_error(uL("opengl;can't open file '" uPs("") "' for reading"), path);
		return (NULL);
	}

	fseek(fd, 0, SEEK_END);
	len = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	if (!(str = (char *)malloc((len + 1) * sizeof(char)))) {
		fclose(fd);
		log_error(uL("opengl;can't malloc space for '" uPs("")), path);
		return (NULL);
	}

	memset(str, 0x00, len + 1);

	if (fread(str, sizeof(char), len, fd) < len) {
		if(feof(fd)) {
			log_warning(uL("opengl;EOF intercepted before the end of the '" uPs("")), path);
		}
		if (ferror(fd)) {
			log_error(uL("opengl;error in reading from '" uPs("")), path);
			free(str);
			str = NULL;
		}
	}
	fclose(fd);

	return (str);
}
BYTE emu_load_rom(void) {
	BYTE recent_roms_permit_add = !info.block_recent_roms_update;

	gui_egds_stop_unnecessary();

	elaborate_rom_file:
	info.format = HEADER_UNKOWN;
	info.no_rom = FALSE;
	info.cpu_rw_extern = FALSE;

	nsf_quit();
	fds_quit();
	map_quit();

	ustrncpy(info.rom.file, info.rom.change_rom, usizeof(info.rom.file));
	info.rom.file[usizeof(info.rom.file) - 1] = 0x00;

	info.doublebuffer = TRUE;

	if (info.rom.file[0]) {
		uTCHAR *ext = emu_ctrl_rom_ext(info.rom.file);

		if (!ustrcasecmp(ext, uL(".fds"))) {
			if (fds_load_rom() == EXIT_ERROR) {
				info.rom.file[0] = 0;
				info.rom.change_rom[0] = 0;
				goto elaborate_rom_file;
			}
			emu_recent_roms_add(&recent_roms_permit_add, info.rom.file);
		} else if (!ustrcasecmp(ext, uL(".nsf"))) {
			if (nsf_load_rom() == EXIT_ERROR) {
				log_error(uL("error loading;" uPs("")), info.rom.file);
				info.rom.file[0] = 0;
				info.rom.change_rom[0] = 0;
				gui_overlay_info_append_msg_precompiled(5, NULL);
				goto elaborate_rom_file;
			}
			emu_recent_roms_add(&recent_roms_permit_add, info.rom.file);
		} else if (!ustrcasecmp(ext, uL(".nsfe"))) {
			if (nsfe_load_rom() == EXIT_ERROR) {
				log_error(uL("error loading;" uPs("")), info.rom.file);
				info.rom.file[0] = 0;
				info.rom.change_rom[0] = 0;
				gui_overlay_info_append_msg_precompiled(5, NULL);
				goto elaborate_rom_file;
			}
			emu_recent_roms_add(&recent_roms_permit_add, info.rom.file);
		} else if (!ustrcasecmp(ext, uL(".fm2"))) {
			tas_file(ext, info.rom.file);
			if (!info.rom.file[0]) {
				gui_overlay_info_append_msg_precompiled(5, NULL);
				log_error(uL("error loading rom"));
			}
			emu_recent_roms_add(&recent_roms_permit_add, tas.file);
			ustrncpy(info.rom.change_rom, info.rom.file, usizeof(info.rom.change_rom));
			// rielaboro il nome del file
			goto elaborate_rom_file;
		} else {
			// carico la rom in memoria
			if ((ines_load_rom() == EXIT_ERROR) && (unif_load_rom() == EXIT_ERROR)) {
				log_error(uL("unknow format;" uPs("")), info.rom.file);
				info.rom.file[0] = 0;
				info.rom.change_rom[0] = 0;
				gui_overlay_info_append_msg_precompiled(5, NULL);
				goto elaborate_rom_file;
			}
			emu_recent_roms_add(&recent_roms_permit_add, info.rom.file);
			info.turn_off = FALSE;
			info.block_recent_roms_update = FALSE;
		}
	} else if (info.gui) {
		// impostazione primaria
		info.prg.rom.banks_16k = info.chr.rom.banks_8k = 1;

		info.prg.rom.banks_8k = info.prg.rom.banks_16k * 2;
		info.chr.rom.banks_4k = info.chr.rom.banks_8k * 2;
		info.chr.rom.banks_1k = info.chr.rom.banks_4k * 4;

		// PRG Ram
		if (map_prg_ram_malloc(0x2000) != EXIT_OK) {
			return (EXIT_ERROR);
		}

		// PRG Rom
		if (map_prg_malloc(info.prg.rom.banks_16k * 0x4000, 0xEA, TRUE) == EXIT_ERROR) {
			return (EXIT_ERROR);
		}

		info.no_rom = TRUE;
	}

#if defined (FULLSCREEN_RESFREQ)
	// mi salvo la vecchia modalita'
	info.old_machine_type = machine.type;
#endif

	// setto il tipo di sistema
	switch (cfg->mode) {
		case AUTO:
			switch (info.machine[DATABASE]) {
				case NTSC:
				case PAL:
				case DENDY:
					machine = machinedb[info.machine[DATABASE] - 1];
					break;
				case DEFAULT:
					if (info.machine[HEADER] == info.machine[DATABASE]) {
						// posso essere nella condizione
						// info.machine[DATABASE] == DEFAULT && info.machine[HEADER] == DEFAULT
						// solo quando avvio senza caricare nessuna rom.
						machine = machinedb[NTSC - 1];
					} else {
						machine = machinedb[info.machine[HEADER] - 1];
					}
					break;
				default:
					machine = machinedb[NTSC - 1];
					break;
			}
			break;
		default:
			machine = machinedb[cfg->mode - 1];
			break;
	}

	if (!nsf.enabled) {
		cheatslist_read_game_cheats();
	}

	return (EXIT_OK);
}
void emu_set_title(uTCHAR *title, int len) {
	uTCHAR name[30];

	if (!info.gui) {
		usnprintf(name, usizeof(name), uL("" NAME " v" VERSION));
	} else {
		usnprintf(name, usizeof(name), uL("" NAME));
	}

	if (info.portable && (cfg->scale != X1)) {
		ustrcat(name, uL("_p"));
	}

	if (cfg->scale == X1) {
		usnprintf(title, len, uL("" uPs("") " (" uPs("")), name, opt_mode[machine.type].lname);
	} else if (cfg->filter == NTSC_FILTER) {
		usnprintf(title, len,
			uL("" uPs("") " (" uPs("") ", " uPs("") ", " uPs("") ", "),
			name, opt_mode[machine.type].lname,
			opt_scale[cfg->scale - 1].sname, opt_ntsc[cfg->ntsc_format].lname);
	} else {
		usnprintf(title, len,
			uL("" uPs("") " (" uPs("") ", " uPs("") ", " uPs("") ", "),
			name, opt_mode[machine.type].lname,
			opt_scale[cfg->scale - 1].sname, opt_filter[cfg->filter].lname);
	}

	if (cfg->scale != X1) {
		if ((cfg->palette == PALETTE_FILE) && ustrlen(cfg->palette_file) != 0) {
			ustrcat(title, uL("extern"));
		} else {
			ustrcat(title, opt_palette[cfg->palette].lname);
		}
	}

#if !defined (RELEASE)
	if (cfg->scale != X1) {
		uTCHAR mapper_id[10];
		usnprintf(mapper_id, usizeof(mapper_id), uL(", %d"), info.mapper.id);
		ustrcat(title, mapper_id);
	}
#endif

	ustrcat(title, uL(")"));
}
BYTE emu_turn_on(void) {
	info.reset = POWER_UP;

	info.first_illegal_opcode = FALSE;

	// per produrre una serie di numeri pseudo-random
	// ad ogni avvio del programma inizializzo il seed
	// con l'orologio.
	srand(time(0));

	// l'inizializzazione della memmap della cpu e della ppu
	memset(&mmcpu, 0x00, sizeof(mmcpu));
	memset(&prg, 0x00, sizeof(prg));
	memset(&chr, 0x00, sizeof(chr));
	memset(&ntbl, 0x00, sizeof(ntbl));
	memset(&mmap_palette, 0x00, sizeof(mmap_palette));
	memset(&oam, 0x00, sizeof(oam));
	memset(&ppu_screen, 0x00, sizeof(ppu_screen));
	memset(&vs_system, 0x00, sizeof(vs_system));

	tas.lag_next_frame = TRUE;
	tas.lag_actual_frame = TRUE;

	cfg->extra_vb_scanlines = cfg->extra_pr_scanlines = 0;

	vs_system.watchdog.next = vs_system_wd_next()

	info.r2002_jump_first_vblank = FALSE;
	info.r2002_race_condition_disabled = FALSE;
	info.r4014_precise_timing_disabled = FALSE;
	info.r4016_dmc_double_read_disabled = FALSE;

	cheatslist_init();

#if defined (WITH_OPENGL)
	gui_screen_info();
#endif

	if (gui_create() == EXIT_ERROR) {
		gui_critical(uL("GUI initialization failed."));
		return (EXIT_ERROR);
	}

	nsf_init();
	fds_init();

	if (gfx_palette_init()) {
		return (EXIT_ERROR);
	}

	// carico la rom in memoria
	{
		emu_ctrl_if_rom_exist();

		if (emu_load_rom()) {
			return (EXIT_ERROR);
		}
		emu_ctrl_doublebuffer();
	}

	overscan_set_mode(machine.type);

	// ...nonche' dei puntatori alla PRG Rom...
	map_prg_rom_8k_reset();

	settings_pgs_parse();

	// APU
	apu_turn_on();

	// PPU
	if (ppu_turn_on()) {
		return (EXIT_ERROR);
	}

	// CPU
	cpu_turn_on();

	// gestione grafica
	if (gfx_init()) {
		return (EXIT_ERROR);
	}

	// ...e inizializzazione della mapper (che
	// deve necessariamente seguire quella della PPU.
	if (map_init()) {
		return (EXIT_ERROR);
	}

	cpu_init_PC();

	// controller
	input_init(NO_SET_CURSOR);

	// joystick
	js_init(TRUE);

	// non viene eseguito nell'input_init() perche' la gui non e' ancora avviata quindi devo eseguirlo dopo il gfx_init().
	gui_nes_keyboard();

	// setto il cursore
	gfx_cursor_init();

	// fps
	fps_init();

	// setto l'fps del timer ext_gfx_draw_screen
	gui_egds_set_fps();

	// gestione sonora
	if (snd_init()) {
		return (EXIT_ERROR);
	}

	if (rewind_init()) {
		return (EXIT_ERROR);
	}

	save_slot_count_load();

	// ritardo della CPU
	emu_cpu_initial_cycles();

	ext_win.vs_system = vs_system.enabled;
	if (vs_system.enabled) {
		if ((cfg->dipswitch == 0xFF00) && (info.default_dipswitches != 0xFF00)) {
			cfg->dipswitch = info.default_dipswitches;
		}
	}

	gui_external_control_windows_show();
	gui_wdgrewind_play();

	info.bat_ram_frames_snap = machine.fps * (60 * 3);

	info.reset = FALSE;
	info.start_frame_0 = TRUE;

	// The End
	return (EXIT_OK);
}
void emu_pause(BYTE mode) {
	if (mode) {
		if (++info.pause == 1) {
			gui_egds_start_pause();
		}
	} else {
		if (--info.pause == 0) {
			gui_egds_stop_pause();
		} else if (info.pause < 0) {
			info.pause = 0;
		}
	}

	if (info.pause == 0) {
		if (nsf.enabled) {
			nsf_reset_timers();
		}
	}

	emu_ctrl_doublebuffer();
}
BYTE emu_reset(BYTE type) {
	if (info.turn_off && (type <= HARD)) {
		return (EXIT_OK);
	}

	gui_wdgrewind_play();

	if ((type == CHANGE_ROM) && (emu_ctrl_if_rom_exist() == EXIT_ERROR)) {
		return (EXIT_OK);
	}

	info.reset = type;

	info.first_illegal_opcode = FALSE;

	srand(time(0));

	tas.lag_next_frame = TRUE;
	tas.lag_actual_frame = TRUE;

	if (info.reset == CHANGE_ROM) {
		info.r4014_precise_timing_disabled = FALSE;
		info.r2002_race_condition_disabled = FALSE;
		info.r4016_dmc_double_read_disabled = FALSE;

		// se carico una rom durante un tas faccio un bel quit dal tas
		if (tas.type != NOTAS) {
			tas_quit();
		}

		// carico la rom in memoria
		if (emu_load_rom()) {
			return (EXIT_ERROR);
		}
		emu_ctrl_doublebuffer();

		ext_win.vs_system = vs_system.enabled;

		overscan_set_mode(machine.type);

		settings_pgs_parse();

		gfx_set_screen(NO_CHANGE, NO_CHANGE, NO_CHANGE, NO_CHANGE, NO_CHANGE, TRUE, FALSE);

		gui_update_gps_settings();
	}

	if ((info.reset == CHANGE_MODE) && overscan_set_mode(machine.type)) {
		gfx_set_screen(NO_CHANGE, NO_CHANGE, NO_CHANGE, NO_CHANGE, NO_CHANGE, TRUE, FALSE);
	}

#if defined (FULLSCREEN_RESFREQ)
	if ((gfx.type_of_fscreen_in_use == FULLSCR) &&
		cfg->adaptive_rrate &&
		(info.old_machine_type != machine.type)) {
		gfx_monitor_set_res(cfg->fullscreen_res_w, cfg->fullscreen_res_h, cfg->adaptive_rrate, TRUE);
	}
#endif

	map_chr_bank_1k_reset();

	if (info.reset >= HARD) {
		map_prg_rom_8k_reset();
	}

	// APU
	apu_turn_on();

	// PPU
	if (ppu_turn_on()) {
		return (EXIT_ERROR);
	}

	// CPU
	cpu_turn_on();

	// mapper
	if (map_init()) {
		return (EXIT_ERROR);
	}

	cpu_init_PC();

	if (info.no_rom) {
		info.reset = FALSE;
		if (info.pause_from_gui) {
			emu_pause((info.pause_from_gui = FALSE));
		}
		return (EXIT_OK);
	}

	// controller
	input_init(SET_CURSOR);

	if (rewind_init()) {
		return (EXIT_ERROR);
	}

	if (info.reset == CHANGE_ROM) {
		save_slot_count_load();
	}

	fps_init();

	if (info.reset >= CHANGE_ROM) {
		gui_egds_set_fps();

		if (snd_playback_start()) {
			return (EXIT_ERROR);
		}
	}

	// ritardo della CPU
	emu_cpu_initial_cycles();

	if (vs_system.enabled) {
		if (type >= HARD) {
			vs_system.shared_mem = 0;
		}
		if ((cfg->dipswitch == 0xFF00) && (info.default_dipswitches != 0xFF00)) {
			cfg->dipswitch = info.default_dipswitches;
		}
	}
	memset(&vs_system.watchdog, 0x00, sizeof(vs_system.watchdog));
	memset(&vs_system.r4020, 0x00, sizeof(vs_system.r4020));
	memset(&vs_system.coins, 0x00, sizeof(vs_system.coins));
	vs_system.watchdog.next = vs_system_wd_next()

	gui_emit_et_external_control_windows_show();

	info.bat_ram_frames = 0;

	info.reset = FALSE;
	info.start_frame_0 = TRUE;

	if ((info.format == NSF_FORMAT) || (info.format == NSFE_FORMAT)) {
		nsf.draw_mask_frames = 2;
	}

	if (info.pause_from_gui) {
		emu_pause((info.pause_from_gui = FALSE));
	}

	return (EXIT_OK);
}
WORD emu_round_WORD(WORD number, WORD round) {
	WORD remainder = number % round;

	if (remainder < (round / 2)) {
		return (number - remainder);
	} else {
		return ((number - remainder) + round);
	}
}
unsigned int emu_power_of_two(unsigned int base) {
	unsigned int pot = 1;

	while (pot < base) {
		pot <<= 1;
	}
	return (pot == 1 ? 2 : pot);
}
double emu_drand(void) {
	double d;

	do {
		d = ((((double)rand() * RS_SCALE) + (double)rand()) * RS_SCALE + (double)rand()) * RS_SCALE;
	} while (d >= 1); // Round off
	return (d);
}
uTCHAR *emu_ustrncpy(uTCHAR *dst, uTCHAR *src) {
	uint32_t size;

	if (dst) {
		free(dst);
		dst = NULL;
	}

	if (src == NULL) {
		return (dst);
	}

	size = ustrlen(src) + 1;
	dst = (uTCHAR *)malloc(sizeof(uTCHAR) * size);
	umemset(dst, 0x00, size);
	ustrcpy(dst, src);

	return (dst);
}
uTCHAR *emu_rand_str(void) {
	static uTCHAR dest[10];
	uTCHAR charset[] = uL("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
	unsigned int i;

	for (i = 0; i < (usizeof(dest) - 1); i++) {
		size_t index = rand() / RAND_MAX * (usizeof(charset) - 1);

		dest[i] = charset[index];
	}
	dest[usizeof(dest) - 1] = '\0';

	return ((uTCHAR *)&dest);
}
void emu_ctrl_doublebuffer(void) {
	gfx_thread_pause();

	switch (info.format) {
		default:
		case iNES_1_0:
		case NES_2_0:
		case UNIF_FORMAT:
		case FDS_FORMAT:
			info.doublebuffer = TRUE;
			break;
		case NSF_FORMAT:
		case NSFE_FORMAT:
			info.doublebuffer = FALSE;
			break;
	}

	if (info.no_rom | info.turn_off) {
		info.doublebuffer = TRUE;
	} else if (info.pause) {
		info.doublebuffer = FALSE;
	}

	if (rwnd.active) {
		info.doublebuffer = FALSE;
	}

	switch (debugger.mode) {
		case DBG_STEP:
		case DBG_BREAKPOINT:
			info.doublebuffer = FALSE;
			break;
		case DBG_GO:
			if (debugger.breakframe) {
				info.doublebuffer = FALSE;
			}
			break;
	}

	gfx_thread_continue();
}
void emu_frame_input_and_rewind(void) {
	// controllo se ci sono eventi di input
	if (tas.type == NOTAS) {
		int i;

		gui_decode_all_input_events();

		for (i = PORT1; i < PORT_MAX; i++) {
			if (port_funct[i].input_add_event) {
				port_funct[i].input_add_event(i);
			}
		}
	} else {
		tas_frame();
	}

	rewind_snapshoot();
}
void emu_info_rom(void) {
	if (!info.rom.file[0]) {
		return;
	}

	log_info(uL("file;" uPs("")), info.rom.file);

	log_info_box_open(uL("format;"));
	if (info.format == NES_2_0) {
		log_close_box(uL("Nes 2.0"));
	} else if (info.format == iNES_1_0) {
		log_close_box(uL("iNES 1.0"));
	} else if (info.format == UNIF_FORMAT) {
		log_close_box(uL("UNIF"));
	} else if (info.format == NSF_FORMAT) {
		log_close_box(uL("NSF"));
		nsf_info();
		return;
	} else if (info.format == NSFE_FORMAT) {
		log_close_box(uL("NSFE"));
		nsfe_info();
		return;
	} else if (info.format == FDS_FORMAT) {
		log_close_box(uL("FDS"));
		fds_info_side(0);
		return;
	} else {
		log_close_box(uL("Unknow"));
		return;
	}

	{
		log_info_box_open(uL("console type;"));
		switch (info.mapper.ext_console_type) {
			case REGULAR_NES:
				log_close_box(uL("Regular NES/Famicom/Dendy"));
				break;
			case VS_SYSTEM:
				log_close_box(uL("Nintendo Vs. System"));
				break;
			case PLAYCHOICE10:
				log_close_box(uL("Playchoice 10"));
				break;
			case FAMICLONE_DECIMAL_MODE:
				log_close_box(uL("Regular Famiclone, but with CPU that supports Decimal Mode"));
				break;
			case EPSM:
				log_close_box(uL("Regular NES/Famicom with EPSM module or plug-through cartridge [unsupported]"));
				break;
			case VT01:
				log_close_box(uL("V.R. Technology VT01 with red/cyan STN palette [unsupported]"));
				break;
			case VT02:
				log_close_box(uL("V.R. Technology VT02 [unsupported]\n"));
				break;
			case VT03:
				log_close_box(uL("V.R. Technology VT03 [unsupported]"));
				break;
			case VT09:
				log_close_box(uL("V.R. Technology VT09 [unsupported]"));
				break;
			case VT32:
				log_close_box(uL("V.R. Technology VT32 [unsupported]"));
				break;
			case VT369:
				log_close_box(uL("V.R. Technology VT369 [unsupported]"));
				break;
			case UMC_UM6578:
				log_close_box(uL("UMC UM6578 [unsupported]"));
				break;
			case FAMICOM_NETWORK_SYSTEM:
				log_close_box(uL("Famicom Network System [unsupported]"));
				break;
			case 13:
			case 14:
			case 15:
				log_close_box(uL("reserved"));
				break;
		}
	}

	if (info.format == UNIF_FORMAT) {
		char *trimmed = &unif.board[0];
		size_t i;

		for (i = 0; i < strlen(unif.stripped_board); i++) {
			if ((*unif.stripped_board) != ' ') {
				break;
			}
			trimmed++;
		}
		log_info_box(uL("UNIF board;%s"), trimmed);

		if (strlen(unif.name) > 0) {
			log_info_box(uL("UNIF name;%s"), unif.name);
		}

		info.mapper.id == UNIF_MAPPER
			? log_info_box(uL("UNIF mapper;%u"), unif.internal_mapper)
			: log_info_box(uL("NES mapper;%u"), info.mapper.id);
	} else {
		log_info_box(uL("NES mapper;%u"), info.mapper.id);
	}

	{
		log_info_box_open(uL("submapper;"));
		if (info.format == NES_2_0) {
			info.mapper.submapper == DEFAULT
				? log_close_box(uL("%u (DEFAULT)"), info.mapper.submapper_nes20)
				: info.mapper.submapper == info.mapper.submapper_nes20
					? log_close_box(uL("%u"), info.mapper.submapper_nes20)
					: log_close_box(uL("%u (%u)"), info.mapper.submapper_nes20, info.mapper.submapper);
		} else {
			info.mapper.submapper == DEFAULT
				? log_close_box(uL("DEFAULT"))
				: log_close_box(uL("%u"), info.mapper.submapper);
		}
	}

	if (info.format == UNIF_FORMAT) {
		if (strlen(unif.dumped.by) > 0) {
			log_info_box_open(uL("dumped by;%s"), unif.dumped.by);

			if (strlen(unif.dumped.with) > 0) {
				log_append(uL(" with %s"), unif.dumped.with);
			}

			if (unif.dumped.month && unif.dumped.day && unif.dumped.year) {
				char *months[12] = {
					"January",   "February", "March",    "April",
					"May",       "June",     "July",     "August",
					"September", "October",  "November", "December"
				};

				log_append(uL(" on %s %d, %d"), months[(unif.dumped.month - 1) % 12], unif.dumped.day, unif.dumped.year);
			}
			log_close_box(uL(""));
		}

		if (chinaersan2.font.data) {
			log_info_box(uL("EXT font;%ld"), (long)chinaersan2.font.size);
		}
	}

	{
		log_info_box_open(uL("mirroring;"));
		if ((info.format == UNIF_FORMAT) && (info.mapper.mirroring == 5)) {
			log_close_box(uL("controlled by the mapper"));
		} else {
			switch (info.mapper.mirroring) {
				default:
				case MIRRORING_HORIZONTAL:
					log_close_box(uL("horizontal"));
					break;
				case MIRRORING_VERTICAL:
					log_close_box(uL("vertical"));
					break;
				case MIRRORING_SINGLE_SCR0:
					log_close_box(uL("scr0"));
					break;
				case MIRRORING_SINGLE_SCR1:
					log_close_box(uL("scr1"));
					break;
				case MIRRORING_FOURSCR:
					log_close_box(uL("4 screen"));
					break;
			}
		}
	}

	if (mapper.misc_roms.size) {
		log_info_box(uL("MISC rom;%-4lu [ %08X %ld ]"),
			(long unsigned)info.mapper.misc_roms,
			info.crc32.misc,
			(long)mapper.misc_roms.size);
	}

	if (info.mapper.trainer) {
		log_info_box(uL("trainer;yes"));
	}

	if (info.prg.ram.banks_8k_plus) {
		log_info_box_open(uL("RAM PRG 8k;%u"), info.prg.ram.banks_8k_plus);
		if (info.prg.ram.bat.banks) {
			log_append(uL(" ( bat : %d - "), info.prg.ram.bat.banks);

			info.prg.ram.bat.start == DEFAULT
				? log_append(uL("DEFAULT )"))
				: log_append(uL("%d )"), info.prg.ram.bat.start);
		}
		log_close_box(uL(""));
	}

	if (mapper.write_vram || info.chr.ram.banks_8k_plus) {
		log_info_box(uL("RAM CHR 8k;%-4u"),
			info.chr.ram.banks_8k_plus + (mapper.write_vram ? info.chr.rom.banks_8k : 0));
	}
	if (chr.extra.data) {
		log_info_box(uL("RAM CHR extra;%ld"), (long)chr.extra.size);
	}

	log_info_box(uL("PRG 8k rom;%-4lu [ %08X %ld ]"),
		(long unsigned)prg_size() / 0x2000,
		info.crc32.prg,
		(long)prg_size());

	if (info.format == UNIF_FORMAT) {
		if (unif.chips.prg > 1) {
			int chip;

			for (chip = 0; chip < unif.chips.prg; chip++) {
				log_info_box(uL(" 8k chip %d;%-4lu [ %08X %ld ]"),
					chip, (long unsigned)prg_chip_size(chip) / 0x2000,
					emu_crc32((void *)prg_chip_rom(chip), prg_chip_size(chip)),
					(long)prg_chip_size(chip));
			}
		}
	}

	if (!mapper.write_vram && chr_size()) {
		log_info_box(uL("CHR 4k vrom;%-4lu [ %08X %ld ]"),
			(long unsigned)chr_size() / 0x1000,
			info.crc32.chr,
			(long)chr_size());
	}

	if (info.format == UNIF_FORMAT) {
		if (unif.chips.chr > 1) {
			int chip;

			for (chip = 0; chip < unif.chips.chr; chip++) {
				log_info_box(uL(" 4k chip %d;%-4lu [ %08X %ld ]"),
					chip, (long unsigned)chr_chip_size(chip) / 0x1000,
					emu_crc32((void *)chr_chip_rom(chip), chr_chip_size(chip)),
					(long)chr_chip_size(chip));
			}
		}
	}

	if (info.format == iNES_1_0) {
		log_info_box(uL("sha1prg;%40s"), info.sha1sum.prg.string);
		if (!mapper.write_vram && chr_size()) {
			log_info_box(uL("shachr;%40s"), info.sha1sum.chr.string);
		}
	}

	log_info_box(uL("CRC32;%08X"), info.crc32.total);
}

INLINE static void emu_frame_started(void) {
	tas.lag_next_frame = TRUE;

	// riprendo a far correre la CPU
	info.frame_status = FRAME_STARTED;
}
INLINE static void emu_frame_finished(void) {
	if ((cfg->cheat_mode == GAMEGENIE_MODE) && (gamegenie.phase == GG_LOAD_ROM)) {
		gui_emit_et_gg_reset();
	}

	tas.lag_actual_frame = tas.lag_next_frame;

	if (tas.lag_actual_frame) {
		tas.total_lag_frames++;
		gui_update_ppu_hacks_lag_frames();
	}

	if (snd_end_frame) {
		snd_end_frame();
	}

	if (cfg->save_battery_ram_file && (++info.bat_ram_frames >= info.bat_ram_frames_snap)) {
		// faccio anche un refresh del file della battery ram
		info.bat_ram_frames = 0;
		map_prg_ram_battery_save();
	}

	if (vs_system.enabled & vs_system.watchdog.reset) {
		gui_emit_et_vs_reset();
	}

	r4011.frames++;

	gui_nes_keyboard_frame_finished();
}
INLINE static void emu_frame_sleep(void) {
	double diff, now = gui_get_ms();

	diff = fps.frame.expected_end - now;

	if (diff > 0) {
		gui_sleep(diff);
	} else {
		fps.info.emu_too_long++;
		fps.frame.expected_end = gui_get_ms();
	}
	fps.frame.expected_end += fps.frame.estimated_ms;
}

static void emu_cpu_initial_cycles(void) {
	BYTE i;

	for (i = 0; i < 8; i++) {
		if (info.mapper.id != NSF_MAPPER) {
			ppu_tick();
		}
		apu_tick(NULL);
		cpu.odd_cycle = !cpu.odd_cycle;
	}
}
static BYTE emu_ctrl_if_rom_exist(void) {
	BYTE found = FALSE;

	if (info.rom.from_load_menu) {
		ustrncpy(info.rom.change_rom, info.rom.from_load_menu, usizeof(info.rom.change_rom));
		info.rom.change_rom[usizeof(info.rom.change_rom) - 1] = 0x00;
		free(info.rom.from_load_menu);
		info.rom.from_load_menu = NULL;
	} else if (gamegenie.rom) {
		ustrncpy(info.rom.change_rom, gamegenie.rom, usizeof(info.rom.change_rom));
		info.rom.change_rom[usizeof(info.rom.change_rom) - 1] = 0x00;
	} else {
		if (info.rom.file[0] && (emu_file_exist(info.rom.file) == EXIT_ERROR)) {
			log_error(uL("emu;'" uPs("") "' not found"), info.rom.file);
			info.rom.file[0] = 0x00;
			return (EXIT_ERROR);
		}
		ustrncpy(info.rom.change_rom, info.rom.file, usizeof(info.rom.change_rom));
		info.rom.change_rom[usizeof(info.rom.change_rom) - 1] = 0x00;
	}

	if (info.rom.change_rom[0]) {
		_uncompress_archive *archive;
		BYTE rc;

		archive = uncompress_archive_alloc(info.rom.change_rom, &rc);

		if (rc == UNCOMPRESS_EXIT_OK) {
			BYTE is_rom = FALSE, is_patch = FALSE;
			uTCHAR *patch = NULL;

			if (archive->rom.count > 0) {
				is_rom = TRUE;
			}
			if (archive->patch.count > 0) {
				is_patch = TRUE;
			}
			if (is_patch && !is_rom && !info.rom.file[0]) {
				is_patch = FALSE;
			}
			if (is_rom) {
				switch ((rc = uncompress_archive_extract_file(archive, UNCOMPRESS_TYPE_ROM))) {
					case UNCOMPRESS_EXIT_OK:
						ustrncpy(info.rom.change_rom, uncompress_archive_extracted_file_name(archive, UNCOMPRESS_TYPE_ROM),
							usizeof(info.rom.change_rom) - 1);
						found = TRUE;
						break;
					case UNCOMPRESS_EXIT_ERROR_ON_UNCOMP:
						break;
					case UNCOMPRESS_EXIT_IS_COMP_BUT_NOT_SELECTED:
					case UNCOMPRESS_EXIT_IS_COMP_BUT_NO_ITEMS:
						ustrncpy(info.rom.change_rom, info.rom.file, usizeof(info.rom.change_rom));
						info.rom.change_rom[usizeof(info.rom.change_rom) - 1] = 0x00;
						break;
					default:
						break;
				}
			}
			if (is_patch) {
				switch ((rc = uncompress_archive_extract_file(archive, UNCOMPRESS_TYPE_PATCH))) {
					case UNCOMPRESS_EXIT_OK:
						patch = uncompress_archive_extracted_file_name(archive, UNCOMPRESS_TYPE_PATCH);
						found = TRUE;
						break;
					case UNCOMPRESS_EXIT_ERROR_ON_UNCOMP:
						break;
					default:
						is_patch = FALSE;
						break;
				}
				if (patch) {
					patcher.file = emu_ustrncpy(patcher.file, patch);
				}
			}
			uncompress_archive_free(archive);
		} else if (rc == UNCOMPRESS_EXIT_IS_NOT_COMP) {
			found = TRUE;
		}
	}

	if (!found) {
		ustrncpy(info.rom.change_rom, info.rom.file, usizeof(info.rom.change_rom));
		info.rom.change_rom[usizeof(info.rom.change_rom) - 1] = 0x00;
		return (EXIT_ERROR);
	}

	if (patcher_ctrl_if_exist(NULL) == EXIT_OK) {
		log_info(uL("patch file;" uPs("")), patcher.file);
	}

	return (EXIT_OK);
}
static uTCHAR *emu_ctrl_rom_ext(uTCHAR *file) {
	static uTCHAR ext[10];
	uTCHAR name_file[255], *last_dot;

	gui_utf_basename(file, name_file, usizeof(name_file));

	last_dot = ustrrchr(name_file, uL('.'));

	if (last_dot == NULL) {
		ustrncpy((uTCHAR *)ext, uL(".nes"), usizeof(ext) - 1);
	} else {
		// salvo l'estensione del file
		ustrncpy((uTCHAR *)ext, last_dot, usizeof(ext) - 1);
	}

	return (ext);
}
static void emu_recent_roms_add(BYTE *add, uTCHAR *file) {
	if ((*add)) {
		(*add) = FALSE;
		recent_roms_add(file);
	}
}
