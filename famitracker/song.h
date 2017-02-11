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

#pragma once

class Instrument {
  // TODO
};

class SongMasterConfig {
  // TODO
};

class Channel1Config {
  // TODO
};

class Channel2Config {
  // TODO
};

class Channel3Config {
  // TODO
};

class Channel4Config {
  // TODO
};

class Pattern {
  // TODO
};

class PatternNumber {
  // TODO
};

class Song {
 public:
  void addInstrument(int volumeSeq, int arpeggioSeq, int pitchSeq, int hiPitchSeq, int dutyCycleSeq);
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
