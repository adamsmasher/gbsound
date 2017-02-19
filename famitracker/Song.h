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

// TODO: move as many of these as possible into the implementation

enum ArpeggioType {
  NON_ARPEGGIO,
  ABSOLUTE,
  FIXED,
  RELATIVE
};

class InstrSequence {
 public:
  InstrSequence(sequence_t);
  void setLoopPoint(int loopPoint);
  void setReleasePoint(int releasePoint);
  void setArpeggioType(ArpeggioType arpeggioType);
  void pushBack(int i);
  void writeGb(std::ostream&) const;
 private:
  std::vector<uint8_t> sequence;
  sequence_t type;
  int loopPoint;
  int releasePoint;
  ArpeggioType arpeggioType;
};

class Instrument {
 public:
  Instrument(uint8_t volumeSeq, uint8_t arpeggioSeq, uint8_t pitchSeq, uint8_t hiPitchSeq, uint8_t dutyCycleSeq);
  void writeGb(std::ostream&) const;
 private:
  uint8_t volumeSeq;
  uint8_t arpeggioSeq;
  uint8_t pitchSeq;
  uint8_t hiPitchSeq;
  uint8_t dutyCycleSeq;
};

class SongMasterConfig {
 public:
  SongMasterConfig();
  void setTempo(uint8_t tempo);
  void writeGb(std::ostream&) const;
 private:
  uint8_t tempo;
  uint8_t channelControl;
  uint8_t outputTerminals;
};

enum ChannelCommandType {
  NO_CHANNEL_COMMAND,
  CHANGE_INSTRUMENT
};

struct ChangeInstrument {
  uint8_t newInstrument;

  void writeGb(std::ostream&) const;
};

struct ChannelCommand {
  ChannelCommandType type;
  union {
    ChangeInstrument changeInstrument;
  };

  void writeGb(std::ostream&) const;
};

enum EngineCommandType {
  NO_ENGINE_COMMAND
};

struct EngineCommand {
  EngineCommandType type;
  
  void writeGb(std::ostream&) const;
};

class GbNote {
 public:
  GbNote();
  GbNote(const ChannelCommand&, uint8_t pitch);

  void writeGb(std::ostream&) const;
 private:
  ChannelCommand command;
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

class Pattern {
 public:
  void writeGb(std::ostream&) const;
  void pushBack(const Row&);
 private:
  std::vector<Row> rows;
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

  void addInstrument(uint8_t volumeSeq, uint8_t arpeggioSeq, uint8_t pitchSeq, uint8_t hiPitchSeq, uint8_t dutyCycleSeq);

  // returns the index of the new sequence
  uint8_t addInstrSequence(const InstrSequence&);

  void setTempo(uint8_t tempo);

  void pushNextPattern(PatternNumber patternNumber);

  void addRow(const Row&, PatternNumber);

  void writeGb(std::ostream&) const;
 private:
  SongImpl *impl;
};
