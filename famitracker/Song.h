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

#include <iostream>
#include <memory>
#include <vector>
#include <experimental/optional>

using namespace std::experimental;

// commands are even numbered, starting at 2
enum InstrumentCommandType {
  INSTR_END       = 0,
  INSTR_END_FRAME = 1,
  INSTR_VOL       = 2,
  INSTR_MARK      = 4,
  INSTR_LOOP      = 6,
  INSTR_PITCH     = 8,
  INSTR_HPITCH    = 10,
  INSTR_DUTY_LO   = 12,
  INSTR_DUTY_25   = 14,
  INSTR_DUTY_50   = 16,
  INSTR_DUTY_75   = 18
};

struct InstrumentCommand {
  InstrumentCommandType type;
  union {
    uint8_t newVolume;
    uint8_t newPitch;
    uint8_t newHiPitch;
  };

  void writeGb(std::ostream& ostream) const;
  uint8_t getLength(void) const;
};

class Instrument {
 public:
  void addCommand(const InstrumentCommand&);
  void writeGb(std::ostream&) const;
  uint8_t getLength(void) const;

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
  uint16_t getLength(void) const;
};

enum EngineCommandType {
  ENGINE_CMD_SET_RATE = 1,
  ENGINE_CMD_STOP = 3,
  ENGINE_CMD_END_OF_PAT = 5,
  ENGINE_CMD_JMP_FRAME = 7
};

struct EngineCommand {
  EngineCommandType type;

  union {
    uint8_t newRate;
    uint8_t newFrame;
  };

  void writeGb(std::ostream&) const;
  uint16_t getLength(void) const;
};

class GbNote {
 public:
  GbNote();
  GbNote(uint8_t pitch);

  void writeGb(std::ostream&) const;
  void addCommand(const ChannelCommand&);
  uint16_t getLength(void) const;
 private:
  std::vector<ChannelCommand> commands;
  uint8_t pitch;
};

class Row {
 public:
  void setNote(int, GbNote);
  void writeGb(std::ostream&) const;
  void jump(uint8_t newFrame);
  void endOfPattern(void);
  void stop(void);
  uint16_t getLength(void) const;
 private:
  bool hasFlowControlCommand;
  std::vector<EngineCommand> engineCommands;
  GbNote notes[4];

  void ensureUnlocked(void) const;
  void setFlowControlCommand(EngineCommand);
};

class PatternNumber {
 public:
  PatternNumber(void);
  explicit PatternNumber(uint8_t);
  friend std::ostream& operator<<(std::ostream&, const PatternNumber&);
  void writeGb(std::ostream&) const;
  friend bool operator==(const PatternNumber&, const PatternNumber&);
  friend bool operator!=(const PatternNumber&, const PatternNumber&);
  int toInt() const;
  PatternNumber next(void) const;
  friend std::hash<PatternNumber>;
 private:
  uint8_t patternNumber;
};

namespace std {
  template <> struct hash<PatternNumber> {
    size_t operator()(const PatternNumber&) const;
  };
}

struct SongImpl;
class Song {
 public:
  Song();
  Song(Song&&);
  ~Song();

  void addInstrument(const Instrument&);

  void setTempo(uint8_t tempo);

  void addRow(const Row&, PatternNumber);

  void addJump(PatternNumber from, PatternNumber to);

  void writeGb(std::ostream&) const;

  void addPattern(void);
 private:
  std::unique_ptr<SongImpl> impl;
};
