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

#include <vector>
#include <iostream>

#pragma once

class Instrument {
 public:
  friend std::ostream& operator<<(std::ostream&, const Instrument&);
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
  PatternNumber(uint8_t);
  friend std::ostream& operator<<(std::ostream&, const PatternNumber&);
 private:
  uint8_t patternNumber;
};
std::ostream& operator<<(std::ostream&, const std::vector<PatternNumber>&);

class Song {
 public:
  void addInstrument(const Instrument&);
  void setTempo(uint8_t tempo);
  friend std::ostream& operator<<(std::ostream&, const Song&);
 private:
  std::vector<Instrument> instruments;
  SongMasterConfig songMasterConfig;
  Channel1Config channel1Config;
  Channel2Config channel2Config;
  Channel3Config channel3Config;
  Channel4Config channel4Config;
  std::vector<Pattern> patterns;
  std::vector<PatternNumber> sequence;
};
