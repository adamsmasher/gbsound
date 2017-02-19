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

void SongMasterConfig::writeGb(std::ostream& ostream) const {
  ostream.write((const char*)&channelControl, 1);
  ostream.write((const char*)&outputTerminals, 1);
  ostream.write((const char*)&tempo, 1);
}

static void writePatternsGb(std::ostream& ostream, const std::vector<Pattern>& patterns) {
  for(auto i = patterns.begin(); i != patterns.end(); ++i) {
    i->writeGb(ostream);
  }
}

static void writeSequenceGb(std::ostream& ostream, const std::vector<PatternNumber>& sequence) {
  for(auto i = sequence.begin(); i != sequence.end(); ++i) {
    i->writeGb(ostream);
  }
}

static void writeInstrumentsGb(std::ostream& ostream, const std::vector<Instrument>& instruments) {
  for(auto i = instruments.begin(); i != instruments.end(); ++i) {
    i->writeGb(ostream);
  }
}

static void writeInstrSequencesGb(std::ostream& ostream, const std::vector<InstrSequence>& instrSequences) {
  for(auto i = instrSequences.begin(); i != instrSequences.end(); ++i) {
    i->writeGb(ostream);
  }
}

bool operator!=(const PatternNumber& n, const PatternNumber& n_) {
  return n.patternNumber != n_.patternNumber;
}

void PatternNumber::writeGb(std::ostream& ostream) const {
  ostream.write((const char*)&patternNumber, 1);
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

  void addRow(const Row& row, PatternNumber i) {
    patterns.at(i.toInt()).pushBack(row);
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

  void writeChannelInstrumentsGb(std::ostream& ostream) const {
    ostream.write((const char*)channelInstruments, 4);
  }

  void writeGb(std::ostream& ostream) const {
    songMasterConfig.writeGb(ostream);
    writeChannelInstrumentsGb(ostream);
    writePatternsGb(ostream, patterns);
    writeSequenceGb(ostream, sequence);
    writeInstrumentsGb(ostream, instruments);
    writeInstrSequencesGb(ostream, instrSequences);
  }
private:
  std::vector<InstrSequence> instrSequences;
  std::vector<Instrument> instruments;
  SongMasterConfig songMasterConfig;
  uint8_t channelInstruments[4];
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

void Song::writeGb(std::ostream& ostream) const {
  impl->writeGb(ostream);
}

std::ostream& operator<<(std::ostream& ostream, const PatternNumber& patternNumber) {
  ostream << patternNumber.patternNumber;
  return ostream;
}

PatternNumber::PatternNumber(uint8_t n) : patternNumber(n) {}

SongMasterConfig::SongMasterConfig() : tempo(0), channelControl(0), outputTerminals(0) {}

GbNote::GbNote() 
{
}

GbNote::GbNote(const ChannelCommand& command, uint8_t pitch) : command(command), pitch(pitch) 
{
}

void Row::setNote(int i, GbNote note) {
  if(i >= 4) {
    throw "setNote out of bounds";
  }
  
  notes[i] = note;
}

void Song::addRow(const Row& row, PatternNumber i) {
  impl->addRow(row, i);
}

int PatternNumber::toInt() const {
  return patternNumber;
}

void Pattern::pushBack(const Row& row) {
  rows.push_back(row);
}

void Pattern::writeGb(std::ostream& ostream) const {
  for(auto i = rows.begin(); i != rows.end(); ++i) {
    i->writeGb(ostream);
  }
}

void Row::writeGb(std::ostream& ostream) const {
  for(auto i = engineCommands.begin(); i != engineCommands.end(); ++i) {
    i->writeGb(ostream);
  }
  // end engine commands
  ostream.put(0);

  for(int i = 0; i < 4; i++) {
    notes[i].writeGb(ostream);    
  }
}

void EngineCommand::writeGb(std::ostream& ostream) const {
  ostream.put(type);
}

void GbNote::writeGb(std::ostream& ostream) const {
  command.writeGb(ostream);
  ostream.put(pitch);
}

// TODO: replace all write(_, 1) with put
void ChannelCommand::writeGb(std::ostream& ostream) const {
  ostream.put(type);
  switch(type) {
  case NO_CHANNEL_COMMAND: break;
  case CHANGE_INSTRUMENT: changeInstrument.writeGb(ostream);
  }      
}

void ChangeInstrument::writeGb(std::ostream& ostream) const {
  ostream.put(newInstrument);
}

InstrSequence::InstrSequence(sequence_t type) : type(type), loopPoint(0), releasePoint(0), arpeggioType(NON_ARPEGGIO) {}

void InstrSequence::setArpeggioType(ArpeggioType arpeggioType) {
  if(this->type != SEQ_ARPEGGIO) {
    throw "Cannot set arpeggio type for non-arpeggio sequence!";
  }
  this->arpeggioType = arpeggioType;
}
