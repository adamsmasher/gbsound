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

#include <vector>
#include <iostream>
#include <experimental/optional>

using namespace std::experimental;

class InstrSequence {
 public:
  void setLoopPoint(int loopPoint);
  void setReleasePoint(int releasePoint);
  void setArpeggioType(int arpeggioType);
  void pushBack(int i);
 private:
  std::vector<int> sequence;
  int loopPoint;
  int releasePoint;
};

class Instrument {
 public:
  Instrument(uint8_t volumeSeq, uint8_t arpeggioSeq, uint8_t pitchSeq, uint8_t hiPitchSeq, uint8_t dutyCycleSeq);
  friend std::ostream& operator<<(std::ostream&, const Instrument&);
 private:
  uint8_t volumeSeq;
  uint8_t arpeggioSeq;
  uint8_t pitchSeq;
  uint8_t hiPitchSeq;
  uint8_t dutyCycleSeq;
};
std::ostream& operator<<(std::ostream&, const std::vector<Instrument>&);

class SongMasterConfig {
 public:
  void setTempo(uint8_t tempo);
  friend std::ostream& operator<<(std::ostream&, const SongMasterConfig&);
 private:
  uint8_t tempo;
};

class Channel1Config {
 public:
  friend std::ostream& operator<<(std::ostream&, const Channel1Config&);
  // TODO
};

class Channel2Config {
 public:
  friend std::ostream& operator<<(std::ostream&, const Channel2Config&);
  // TODO
};

class Channel3Config {
 public:
  friend std::ostream& operator<<(std::ostream&, const Channel3Config&);
  // TODO
};

class Channel4Config {
 public:
  friend std::ostream& operator<<(std::ostream&, const Channel4Config&);
  // TODO
};

class Pattern {
 public:
  friend std::ostream& operator<<(std::ostream&, const Pattern&);
};
std::ostream& operator<<(std::ostream&, const std::vector<Pattern>&);

class PatternNumber {
 public:
  explicit PatternNumber(uint8_t);
  friend std::ostream& operator<<(std::ostream&, const PatternNumber&);
 private:
  uint8_t patternNumber;
};
std::ostream& operator<<(std::ostream&, const std::vector<PatternNumber>&);

struct SongImpl;

class Song {
 public:
  Song();
  ~Song();
  
  void addInstrument(uint8_t volumeSeq, uint8_t arpeggioSeq, uint8_t pitchSeq, uint8_t hiPitchSeq, uint8_t dutyCycleSeq);
  uint8_t addInstrSequence(const InstrSequence&);
  void setTempo(uint8_t tempo);
  void pushNextPattern(PatternNumber patternNumber);
  friend std::ostream& operator<<(std::ostream&, const Song&);
 private:
  SongImpl *impl;
};
