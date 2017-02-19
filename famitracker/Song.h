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

#include "FamiTrackerTypes.h"

#include <vector>
#include <iostream>
#include <experimental/optional>

using namespace std::experimental;

enum InstrumentCommandType {
  INSTR_VOL,
  INSTR_MARK,
  INSTR_LOOP,
  INSTR_PITCH,
  INSTR_HPITCH,
  INSTR_DUTY_LO,
  INSTR_DUTY_25,
  INSTR_DUTY_50,
  INSTR_DUTY_75
};

struct InstrumentCommand {
  InstrumentCommandType type;
  union {
    uint8_t newVolume;
  };
  
  void writeGb(std::ostream& ostream) const;
};

class Instrument {
 public:
  void addCommand(const InstrumentCommand&);
  void writeGb(std::ostream&) const;
 private:
  std::vector<InstrumentCommand> commands;
};

enum ChannelCommandType {
  CHANNEL_CMD_KEY_OFF,
  CHANNEL_CMD_SET_SND_LEN,
  CHANNEL_CMD_OCTAVE_UP,
  CHANNEL_CMD_OCTAVE_DOWN,
  CHANNEL_CMD_SET_INSTRUMENT
};

struct ChannelCommand {
  ChannelCommandType type;
  union {
    uint8_t newInstrument;
  };

  void writeGb(std::ostream&) const;
};

enum EngineCommandType {
  ENGINE_CMD_SET_RATE,
  ENGINE_CMD_STOP,
  ENGINE_CMD_END_OF_PAT,
  ENGINE_CMD_JMP_FRAME
};

struct EngineCommand {
  EngineCommandType type;
  
  void writeGb(std::ostream&) const;
};

class GbNote {
 public:
  GbNote();
  GbNote(uint8_t pitch);

  void writeGb(std::ostream&) const;
  void addCommand(const ChannelCommand&);
 private:
  std::vector<ChannelCommand> commands;
  uint8_t pitch;
};

class Row {
 public:
  void setNote(int, GbNote);
  void writeGb(std::ostream&) const;
 private:
  std::vector<EngineCommand> engineCommands;
  GbNote notes[4];
};

class PatternNumber {
 public:
  explicit PatternNumber(uint8_t);
  friend std::ostream& operator<<(std::ostream&, const PatternNumber&);
  void writeGb(std::ostream&) const;
  friend bool operator!=(const PatternNumber&, const PatternNumber&);
  int toInt() const;
 private:
  uint8_t patternNumber;
};

struct SongImpl;
class Song {
 public:
  Song();
  ~Song();

  void addInstrument(const Instrument&);

  void setTempo(uint8_t tempo);

  void pushNextPattern(PatternNumber patternNumber);

  void addRow(const Row&, PatternNumber);

  void writeGb(std::ostream&) const;
 private:
  SongImpl *impl;
};
