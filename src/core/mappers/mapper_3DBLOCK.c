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
#include "mem_map.h"
#include "info.h"
#include "mappers.h"
#include "cpu.h"
#include "save_slot.h"
#include "../../c++/pic16c5x/pic16c5x.h"

uint8_t pic16c5x_rd(int port);
void pic16c5x_wr(int port, int val);

static BYTE eprom_3d_block[] = {
	0xFE, 0x0C, 0x05, 0x00, 0x00, 0x0C, 0x25, 0x00, 0x00, 0x00, 0xFF, 0x0C, 0x05, 0x00, 0x00, 0x08,
	0x28, 0x00, 0xE8, 0x02, 0x09, 0x0A, 0x00, 0x08, 0x28, 0x00, 0xE8, 0x02, 0x0D, 0x0A, 0xE9, 0x02,
	0x0D, 0x0A, 0x00, 0x08, 0xE2, 0x0C, 0x86, 0x01, 0x43, 0x06, 0x12, 0x0A, 0x00, 0x08, 0xE2, 0x01,
	0x00, 0x08, 0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0x00, 0x08, 0xFF, 0x08, 0xFF, 0x08, 0xFF, 0x08,
	0xE2, 0x01, 0xFF, 0x08, 0xFF, 0x08, 0x00, 0x08, 0x01, 0x08, 0x01, 0x08, 0x01, 0x08, 0x00, 0x08,
	0xFF, 0x08, 0xFF, 0x0C, 0x05, 0x00, 0xFF, 0x0C, 0x06, 0x00, 0xE2, 0x0C, 0x86, 0x01, 0x43, 0x07,
	0x2D, 0x0A, 0xE2, 0x0C, 0x86, 0x01, 0x43, 0x07, 0x2D, 0x0A, 0xE2, 0x0C, 0x86, 0x01, 0x43, 0x07,
	0x2D, 0x0A, 0x0E, 0x0C, 0x45, 0x01, 0x2A, 0x00, 0x43, 0x07, 0x44, 0x0A, 0x05, 0x0C, 0x29, 0x00,
	0x03, 0x0C, 0x0C, 0x09, 0x00, 0x09, 0x2D, 0x0A, 0x02, 0x0C, 0x8A, 0x01, 0x43, 0x07, 0x55, 0x0A,
	0x02, 0x0C, 0x29, 0x00, 0x05, 0x0C, 0x0C, 0x09, 0x00, 0x09, 0x0B, 0x0C, 0x29, 0x00, 0x90, 0x0C,
	0x08, 0x09, 0x00, 0x09, 0xE9, 0x02, 0x4F, 0x0A, 0x2D, 0x0A, 0x0E, 0x0C, 0x8A, 0x01, 0x43, 0x07,
	0x5C, 0x0A, 0xA0, 0x0C, 0x2B, 0x00, 0x2D, 0x0A, 0x04, 0x0C, 0x8A, 0x01, 0x43, 0x07, 0x98, 0x0A,
	0x02, 0x0C, 0x8B, 0x01, 0x43, 0x06, 0x2D, 0x0A, 0xEB, 0x00, 0xEB, 0x00, 0x03, 0x0C, 0x29, 0x00,
	0x2C, 0x0C, 0x0C, 0x09, 0x00, 0x09, 0x30, 0x0C, 0x2C, 0x00, 0x00, 0x0C, 0x2D, 0x00, 0x0B, 0x02,
	0x28, 0x00, 0xE2, 0x0C, 0x86, 0x01, 0x43, 0x07, 0x79, 0x0A, 0xE2, 0x0C, 0x86, 0x01, 0x43, 0x06,
	0x2D, 0x0A, 0xE8, 0x02, 0x71, 0x0A, 0x00, 0x09, 0x3B, 0x0C, 0x28, 0x00, 0xE2, 0x0C, 0x86, 0x01,
	0x43, 0x07, 0x86, 0x0A, 0xE2, 0x0C, 0x86, 0x01, 0x43, 0x06, 0x2D, 0x0A, 0xE8, 0x02, 0x7E, 0x0A,
	0x00, 0x09, 0x0B, 0x02, 0xEC, 0x01, 0x03, 0x07, 0x8E, 0x0A, 0xAD, 0x02, 0x3B, 0x0C, 0xEC, 0x01,
	0x03, 0x07, 0x93, 0x0A, 0xAD, 0x02, 0x02, 0x0C, 0x8D, 0x00, 0x03, 0x07, 0x6F, 0x0A, 0x2D, 0x0A,
	0x08, 0x0C, 0x8A, 0x01, 0x43, 0x07, 0xA1, 0x0A, 0x08, 0x0C, 0x2E, 0x00, 0xF0, 0x0C, 0x24, 0x00,
	0x5A, 0x0B, 0x0A, 0x0C, 0x8A, 0x01, 0x43, 0x07, 0xA7, 0x0A, 0x03, 0x04, 0xAC, 0x0A, 0x0C, 0x0C,
	0x8A, 0x01, 0x43, 0x07, 0x5D, 0x0B, 0x03, 0x05, 0x60, 0x03, 0xEE, 0x00, 0x43, 0x07, 0x5A, 0x0B,
	0x08, 0x0C, 0x2E, 0x00, 0xA4, 0x02, 0xF4, 0x0C, 0x84, 0x01, 0x43, 0x07, 0x5A, 0x0B, 0x1F, 0x0C,
	0x53, 0x01, 0x37, 0x00, 0x33, 0x03, 0x33, 0x03, 0x33, 0x03, 0x33, 0x03, 0x33, 0x03, 0x07, 0x0C,
	0x53, 0x01, 0x3A, 0x00, 0x0F, 0x0C, 0x52, 0x01, 0x39, 0x00, 0x28, 0x00, 0x32, 0x03, 0x32, 0x03,
	0x32, 0x03, 0x32, 0x03, 0x0F, 0x0C, 0x52, 0x01, 0x38, 0x00, 0x07, 0x0C, 0x51, 0x01, 0x35, 0x00,
	0x31, 0x03, 0x31, 0x03, 0x31, 0x03, 0x07, 0x0C, 0x51, 0x01, 0x34, 0x00, 0x31, 0x03, 0x31, 0x03,
	0x31, 0x03, 0x03, 0x0C, 0x51, 0x01, 0x33, 0x00, 0x07, 0x0C, 0x50, 0x01, 0x32, 0x00, 0x30, 0x03,
	0x30, 0x03, 0x30, 0x03, 0x07, 0x0C, 0x50, 0x01, 0x31, 0x00, 0x30, 0x03, 0x30, 0x03, 0x30, 0x03,
	0x03, 0x0C, 0x50, 0x01, 0x30, 0x00, 0x07, 0x0C, 0x79, 0x01, 0x28, 0x03, 0x04, 0x0C, 0x48, 0x01,
	0x33, 0x01, 0x08, 0x0C, 0x57, 0x01, 0x43, 0x06, 0xF6, 0x0A, 0xB8, 0x02, 0x10, 0x0C, 0x57, 0x01,
	0x43, 0x06, 0x06, 0x0B, 0x07, 0x0C, 0x77, 0x01, 0x17, 0x02, 0x17, 0x09, 0xF9, 0x01, 0xF9, 0x06,
	0xB9, 0x00, 0x17, 0x02, 0x20, 0x09, 0xFA, 0x01, 0xFA, 0x06, 0xBA, 0x00, 0x7B, 0x00, 0x02, 0x0C,
	0xB1, 0x00, 0x71, 0x02, 0xB1, 0x02, 0xF1, 0x06, 0x13, 0x0B, 0x19, 0x02, 0x91, 0x00, 0x03, 0x07,
	0x13, 0x0B, 0x11, 0x02, 0x39, 0x00, 0x02, 0x0C, 0xB2, 0x00, 0x72, 0x02, 0xB2, 0x02, 0xF2, 0x06,
	0x1F, 0x0B, 0x1A, 0x02, 0x92, 0x00, 0x03, 0x07, 0x1F, 0x0B, 0x12, 0x02, 0x3A, 0x00, 0x02, 0x0C,
	0xB0, 0x00, 0x70, 0x02, 0xB0, 0x02, 0xF0, 0x06, 0x2B, 0x0B, 0x18, 0x02, 0x90, 0x00, 0x03, 0x07,
	0x2B, 0x0B, 0x10, 0x02, 0x38, 0x00, 0x02, 0x0C, 0xB4, 0x00, 0x05, 0x0C, 0xB4, 0x00, 0x74, 0x02,
	0xB4, 0x02, 0xF4, 0x06, 0x39, 0x0B, 0x19, 0x02, 0x94, 0x00, 0x03, 0x06, 0x39, 0x0B, 0x14, 0x02,
	0x39, 0x00, 0x02, 0x0C, 0xB5, 0x00, 0x05, 0x0C, 0xB5, 0x00, 0x75, 0x02, 0xB5, 0x02, 0xF5, 0x06,
	0x47, 0x0B, 0x1A, 0x02, 0x95, 0x00, 0x03, 0x06, 0x47, 0x0B, 0x15, 0x02, 0x3A, 0x00, 0x02, 0x0C,
	0xB3, 0x00, 0x0A, 0x0C, 0xB3, 0x00, 0x73, 0x02, 0xB3, 0x02, 0xF3, 0x06, 0x58, 0x0B, 0x18, 0x02,
	0x93, 0x00, 0x43, 0x06, 0x57, 0x0B, 0x03, 0x06, 0x58, 0x0B, 0x13, 0x02, 0x38, 0x00, 0xBB, 0x02,
	0xF8, 0x0C, 0x24, 0x00, 0x12, 0x09, 0x00, 0x09, 0x2D, 0x0A, 0x12, 0x09, 0x00, 0x02, 0x43, 0x06,
	0x67, 0x0B, 0x03, 0x0C, 0x08, 0x09, 0x00, 0x09, 0xE0, 0x00, 0x43, 0x07, 0x61, 0x0B, 0xA4, 0x02,
	0x2D, 0x0A, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x29, 0x0A
};
static BYTE eprom_block_force[] = {
	0xFE, 0x0C, 0x05, 0x00, 0x05, 0x05, 0xFF, 0x0C, 0x06, 0x00, 0x02, 0x0C, 0x3A, 0x00, 0x00, 0x0C,
	0x3B, 0x00, 0x05, 0x0C, 0x3C, 0x00, 0x06, 0x0C, 0x3D, 0x00, 0x00, 0x0C, 0x3E, 0x00, 0x00, 0x0C,
	0x3F, 0x00, 0xDD, 0x0C, 0x86, 0x01, 0x43, 0x07, 0x11, 0x0A, 0xDD, 0x0C, 0x86, 0x01, 0x43, 0x06,
	0x15, 0x0A, 0xDF, 0x0C, 0x86, 0x01, 0x43, 0x07, 0x11, 0x0A, 0x0F, 0x0C, 0x45, 0x01, 0xE2, 0x01,
	0x30, 0x0A, 0x30, 0x0A, 0x38, 0x0A, 0x38, 0x0A, 0x34, 0x0A, 0x34, 0x0A, 0x48, 0x0A, 0x48, 0x0A,
	0x32, 0x0A, 0x32, 0x0A, 0x44, 0x0A, 0x44, 0x0A, 0x36, 0x0A, 0x36, 0x0A, 0x4B, 0x0A, 0x4B, 0x0A,
	0x64, 0x00, 0x11, 0x0A, 0xA4, 0x02, 0x11, 0x0A, 0x60, 0x00, 0x11, 0x0A, 0xA0, 0x02, 0x11, 0x0A,
	0x10, 0x02, 0x32, 0x00, 0x11, 0x02, 0x33, 0x00, 0xF2, 0x02, 0x3C, 0x0A, 0xF3, 0x02, 0x3C, 0x0A,
	0x05, 0x04, 0x05, 0x04, 0x05, 0x05, 0x11, 0x0A, 0x08, 0x02, 0xE2, 0x01, 0x50, 0x0A, 0x88, 0x0A,
	0x00, 0x02, 0x27, 0x00, 0x11, 0x0A, 0xE7, 0x02, 0x11, 0x0A, 0x05, 0x04, 0x05, 0x05, 0x11, 0x0A,
	0x19, 0x02, 0x9F, 0x00, 0x43, 0x06, 0x57, 0x0A, 0x03, 0x07, 0x7B, 0x0A, 0x11, 0x0A, 0x18, 0x02,
	0x9E, 0x00, 0x43, 0x06, 0x5E, 0x0A, 0x03, 0x07, 0x7B, 0x0A, 0x11, 0x0A, 0x17, 0x02, 0x9D, 0x00,
	0x43, 0x06, 0x65, 0x0A, 0x03, 0x07, 0x7B, 0x0A, 0x11, 0x0A, 0x16, 0x02, 0x9C, 0x00, 0x43, 0x06,
	0x6C, 0x0A, 0x03, 0x07, 0x7B, 0x0A, 0x11, 0x0A, 0x15, 0x02, 0x9B, 0x00, 0x43, 0x06, 0x73, 0x0A,
	0x03, 0x07, 0x7B, 0x0A, 0x11, 0x0A, 0x14, 0x02, 0x9A, 0x00, 0x43, 0x06, 0x7A, 0x0A, 0x03, 0x07,
	0x7B, 0x0A, 0x11, 0x0A, 0x11, 0x0A, 0x14, 0x02, 0x3A, 0x00, 0x15, 0x02, 0x3B, 0x00, 0x16, 0x02,
	0x3C, 0x00, 0x17, 0x02, 0x3D, 0x00, 0x18, 0x02, 0x3E, 0x00, 0x19, 0x02, 0x3F, 0x00, 0x11, 0x0A,
	0x8B, 0x09, 0x29, 0x00, 0x11, 0x0A, 0x09, 0x02, 0xE2, 0x01, 0x00, 0x08, 0x19, 0x08, 0x33, 0x08,
	0x4C, 0x08, 0x66, 0x08, 0x80, 0x08, 0x99, 0x08, 0xB3, 0x08, 0xCC, 0x08, 0xE6, 0x08, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F,
	0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x00, 0x0A
};

struct _m3dblock {
	WORD address;
} m3dblock;

void map_init_3DBLOCK(void) {
	EXTCL_MAPPER_QUIT(3DBLOCK);
	EXTCL_CPU_WR_MEM(3DBLOCK);
	EXTCL_CPU_RD_MEM(3DBLOCK);
	EXTCL_SAVE_MAPPER(3DBLOCK);
	EXTCL_CPU_EVERY_CYCLE(3DBLOCK);
	mapper.internal_struct[0] = (BYTE *)&m3dblock;
	mapper.internal_struct_size[0] = sizeof(m3dblock);

	memset(&m3dblock, 0x00, sizeof(m3dblock));

	if ((info.reset == CHANGE_ROM) || (info.reset == POWER_UP)) {
		BYTE *eprom = NULL;

		if ((info.mapper.misc_roms == 1) & (mapper.misc_roms.size == 1024)) {
			eprom = mapper.misc_roms.data;
		} else {
			if (info.crc32.prg == 0x86DBA660) { // 3D Block (Hwang Shinwei) [!].nes
				eprom = &eprom_3d_block[0];
			} else if (
				(info.crc32.prg == 0x3C43939D) | // Block Force.nes
				(info.crc32.prg == 0xB655C53A))  // Block Force (Hwang Shinwei).nes
			{
				eprom = &eprom_block_force[0];
			}
		}
		if (eprom == NULL) {
			// TODO : Aggiungere un warning
		} else {
			pic16c5x_init(eprom, pic16c5x_rd, pic16c5x_wr);
		}
	}

	pic16c5x_reset(info.reset);

	info.mapper.extend_wr = info.mapper.extend_rd = TRUE;
}
void extcl_mapper_quit_3DBLOCK(void) {
	pic16c5x_quit();
}
void extcl_cpu_wr_mem_3DBLOCK(WORD address, UNUSED(BYTE value)) {
	m3dblock.address = address;
}
BYTE extcl_cpu_rd_mem_3DBLOCK(WORD address, BYTE openbus, UNUSED(BYTE before)) {
	m3dblock.address = address;
	return (openbus);
}
BYTE extcl_save_mapper_3DBLOCK(BYTE mode, BYTE slot, FILE *fp) {
	save_slot_ele(mode, slot, m3dblock.address);
	pic16c5x_save_mapper(mode, slot, fp);

	return (EXIT_OK);
}
void extcl_cpu_every_cycle_3DBLOCK(void) {
	pic16c5x_run();
}

uint8_t pic16c5x_rd(int port) {
	if (port == 0) {
		return (1 |
			(m3dblock.address & 0x0040 ? 0x02 : 0) | //  A6 -> RA1
			(m3dblock.address & 0x0020 ? 0x04 : 0) | //  A5 -> RA2
			(m3dblock.address & 0x0010 ? 0x08 : 0)); //  A4 -> RA3
	} else if (port == 1) {
		return (
			(m3dblock.address & 0x1000 ? 0x01 : 0) | // A12 -> RB0
			(m3dblock.address & 0x0080 ? 0x02 : 0) | //  A7 -> RB1
			(m3dblock.address & 0x0400 ? 0x04 : 0) | // A10 -> RB2
			(m3dblock.address & 0x0800 ? 0x08 : 0) | // A11 -> RB3
			(m3dblock.address & 0x0200 ? 0x10 : 0) | //  A9 -> RB4
			(m3dblock.address & 0x0100 ? 0x20 : 0) | //  A8 -> RB5
			(m3dblock.address & 0x2000 ? 0x40 : 0) | // A13 -> RB6
			(m3dblock.address & 0x4000 ? 0x80 : 0)); // A14 -> RB7
	}
	return (0xFF);
}
void pic16c5x_wr(int port, int val) {
	if (port == 0) {
		if (val & 0x1001) {
			irq.high &= ~EXT_IRQ;
		} else {
			irq.high |= EXT_IRQ;
		}
	}
}
