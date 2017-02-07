/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2014  Jonathan Liss
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
*/

#pragma once

enum Command {
  CT_COMMENTLINE,    // anything may follow
  // song info
  CT_TITLE,          // string
  CT_AUTHOR,         // string
  CT_COPYRIGHT,      // string
  CT_COMMENT,        // string (concatenates line)
  // global settings
  CT_MACHINE,        // uint (0=NTSC, 1=PAL)
  CT_FRAMERATE,      // uint (0=default)
  CT_EXPANSION,      // uint (0=none, 1=VRC6, 2=VRC7, 4=FDS, 8=MMC5, 16=N163, 32=S5B)
  CT_VIBRATO,        // uint (0=old, 1=new)
  CT_SPLIT,          // uint (32=default)
  // namco global settings
  CT_N163CHANNELS,   // uint
  // macros
  CT_MACRO,          // uint (type) uint (index) int (loop) int (release) int (setting) : int_list
  CT_MACROVRC6,      // uint (type) uint (index) int (loop) int (release) int (setting) : int_list
  CT_MACRON163,      // uint (type) uint (index) int (loop) int (release) int (setting) : int_list
  CT_MACROS5B,       // uint (type) uint (index) int (loop) int (release) int (setting) : int_list
  // dpcm samples
  CT_DPCMDEF,        // uint (index) uint (size) string (name)
  CT_DPCM,           // : hex_list
  // instruments
  CT_INST2A03,       // uint (index) int int int int int string (name)
  CT_INSTVRC6,       // uint (index) int int int int int string (name)
  CT_INSTVRC7,       // uint (index) int (patch) hex hex hex hex hex hex hex hex string (name)
  CT_INSTFDS,        // uint (index) int (mod enable) int (m speed) int (m depth) int (m delay) string (name)
  CT_INSTN163,       // uint (index) int int int int int uint (w size) uint (w pos) uint (w count) string (name)
  CT_INSTS5B,        // uint (index) int int int int int  string (name)
  // instrument data
  CT_KEYDPCM,        // uint (inst) uint (oct) uint (note) uint (sample) uint (pitch) uint (loop) uint (loop_point)
  CT_FDSWAVE,        // uint (inst) : uint_list x 64
  CT_FDSMOD,         // uint (inst) : uint_list x 32
  CT_FDSMACRO,       // uint (inst) uint (type) int (loop) int (release) int (setting) : int_list
  CT_N163WAVE,       // uint (inst) uint (wave) : uint_list
  // track info
  CT_TRACK,          // uint (pat length) uint (speed) uint (tempo) string (name)
  CT_COLUMNS,        // : uint_list (effect columns)
  CT_ORDER,          // hex (frame) : hex_list
  // pattern data
  CT_PATTERN,        // hex (pattern)
  CT_ROW,            // row data
  // end of command list
  CT_COUNT
};
