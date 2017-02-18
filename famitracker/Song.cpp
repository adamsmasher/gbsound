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

Instrument::Instrument(uint8_t volumeSeq, uint8_t arpeggioSeq, uint8_t pitchSeq, uint8_t hiPitchSeq, uint8_t dutyCycleSeq) :
  volumeSeq(volumeSeq),
  arpeggioSeq(arpeggioSeq),
  pitchSeq(pitchSeq),
  hiPitchSeq(hiPitchSeq),
  dutyCycleSeq(dutyCycleSeq)
{}

void SongMasterConfig::setTempo(uint8_t tempo) {
  this->tempo = tempo;
}

class SongImpl {
public:
  void setTempo(uint8_t tempo) {
    songMasterConfig.setTempo(tempo);
  }

  uint8_t addInstrSequence(const InstrSequence& sequence) {
    instrSequences.push_back(sequence);
    return instrSequences.size() - 1;
  }

  void pushNextPattern(PatternNumber patternNumber) {
    sequence.push_back(patternNumber);
  }

  void addInstrument(uint8_t volumeSeq, uint8_t arpeggioSeq, uint8_t pitchSeq, uint8_t hiPitchSeq, uint8_t dutyCycleSeq) {
    if(!isValidInstrSequence(volumeSeq)) {
      throw "Invalid volume sequence number";
    }
    if(!isValidInstrSequence(arpeggioSeq)) {
      throw "Invalid arpeggio sequence number";
    }
    if(!isValidInstrSequence(pitchSeq)) {
      throw "Invalid pitch sequence number";
    }
    if(!isValidInstrSequence(hiPitchSeq)) {
      throw "Invalid hipitch sequence number";
    }
    if(!isValidInstrSequence(dutyCycleSeq)) {
      throw "Invalid duty cycle sequence number";
    }

    instruments.push_back(Instrument(volumeSeq, arpeggioSeq, pitchSeq, hiPitchSeq, dutyCycleSeq));
  }

  friend std::ostream& operator<<(std::ostream&, const SongImpl&);

private:
  std::vector<InstrSequence> instrSequences;
  std::vector<Instrument> instruments;
  SongMasterConfig songMasterConfig;
  Channel1Config channel1Config;
  Channel2Config channel2Config;
  Channel3Config channel3Config;
  Channel4Config channel4Config;
  std::vector<Pattern> patterns;
  std::vector<PatternNumber> sequence;

  bool isValidInstrSequence(uint8_t seq) const {
    return seq < instrSequences.size();
  }
};

Song::Song() : impl(new SongImpl) {}

Song::~Song() {
  delete impl;
}

void Song::addInstrument(uint8_t volumeSeq, uint8_t arpeggioSeq, uint8_t pitchSeq, uint8_t hiPitchSeq, uint8_t dutyCycleSeq) {
  impl->addInstrument(volumeSeq, arpeggioSeq, pitchSeq, hiPitchSeq, dutyCycleSeq);
}

uint8_t Song::addInstrSequence(const InstrSequence& instrSequence) {
  return impl->addInstrSequence(instrSequence);
}

void Song::setTempo(uint8_t tempo) {
  impl->setTempo(tempo);
}

void Song::pushNextPattern(PatternNumber patternNumber) {
  impl->pushNextPattern(patternNumber);
}
  
PatternNumber::PatternNumber(uint8_t n) : patternNumber(n) {}

std::ostream& operator<<(std::ostream& ostream, const Song& song) {
  return ostream << *song.impl;
}

std::ostream& operator<<(std::ostream& ostream, const SongImpl& song) {
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
