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

#include "Song.h"

void SongMasterConfig::setTempo(uint8_t tempo) {
  this->tempo = tempo;
}

void Song::setTempo(uint8_t tempo) {
  songMasterConfig.setTempo(tempo);
}

void Song::addInstrument(const Instrument& instrument) {
  instruments.push_back(instrument);
}

std::ostream& operator<<(std::ostream& ostream, const Song& song) {
  ostream
    << song.songMasterConfig
    << song.channel1Config
    << song.channel2Config
    << song.channel3Config
    << song.channel4Config
    << song.patterns
    << song.sequence
    << song.instruments;
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream, const std::vector<Pattern>& patterns) {
  for(auto i = patterns.begin(); i != patterns.end(); ++i) {
    ostream << *i;
  }
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream, const std::vector<PatternNumber>& patternNumbers) {
  for(auto i = patternNumbers.begin(); i != patternNumbers.end(); ++i) {
    ostream << *i;
  }
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream, const std::vector<Instrument>& instruments) {
  for(auto i = instruments.begin(); i != instruments.end(); ++i) {
    ostream << *i;
  }
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream, const PatternNumber& patternNumber) {
  ostream << patternNumber.patternNumber;
  return ostream;
}
