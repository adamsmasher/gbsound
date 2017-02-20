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

class SongMasterConfig {
 public:
  SongMasterConfig();

  void setTempo(uint8_t tempo) {
    this->tempo = tempo;
  }

  void writeGb(std::ostream& ostream) const {
    ostream.put(channelControl);
    ostream.put(outputTerminals);
    ostream.put(tempo);
  }

 private:
  uint8_t tempo;
  uint8_t channelControl;
  uint8_t outputTerminals;
};

class Pattern {
 public:
  void writeGb(std::ostream& ostream) const {
    for(auto i = rows.begin(); i != rows.end(); ++i) {
      i->writeGb(ostream);
    }
  }

  void addRow(const Row& row) {
    rows.push_back(row);
  }

 private:
  std::vector<Row> rows;
};

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

bool operator!=(const PatternNumber& n, const PatternNumber& n_) {
  return n.patternNumber != n_.patternNumber;
}

void PatternNumber::writeGb(std::ostream& ostream) const {
  // to make it faster to index the pattern table, pattern numbers increment by 2
  ostream.put(patternNumber * 2);
}

class SongImpl {
public:
  void setTempo(uint8_t tempo) {
    songMasterConfig.setTempo(tempo);
  }

  void pushNextPattern(PatternNumber patternNumber) {
    sequence.push_back(patternNumber);
  }

  void addRow(const Row& row, PatternNumber i) {
    patterns.at(i.toInt()).addRow(row);
  }

  void addInstrument(const Instrument& instrument) {
    instruments.push_back(instrument);
  }

  void writeChannelInstrumentsGb(std::ostream& ostream) const {
    ostream.write((const char*)channelInstruments, 4);
  }

  // TODO: you need to write tables and stuff
  void writeGb(std::ostream& ostream) const {
    songMasterConfig.writeGb(ostream);
    writeChannelInstrumentsGb(ostream);
    writePatternsGb(ostream, patterns);
    writeSequenceGb(ostream, sequence);
    writeInstrumentsGb(ostream, instruments);
  }
private:
  std::vector<Instrument> instruments;
  SongMasterConfig songMasterConfig;
  uint8_t channelInstruments[4];
  std::vector<Pattern> patterns;
  std::vector<PatternNumber> sequence;
};

Song::Song() : impl(new SongImpl) {}

Song::~Song() { delete impl; }

void Song::addInstrument(const Instrument& instrument) {
  impl->addInstrument(instrument);
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

GbNote::GbNote() {}

GbNote::GbNote(uint8_t pitch) : pitch(pitch) {}

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
  // engine commands increment by two to make table lookup faster
  // also, 0 is a NOP, so they start at 1 (3, 5, ...)
  ostream.put(type * 2 + 1);
}

void GbNote::writeGb(std::ostream& ostream) const {
  for(auto i = commands.begin(); i != commands.end(); ++i) {
    i->writeGb(ostream);
  }
  // notes are odd numbered... (1, 3, ...)
  ostream.put(pitch * 2 + 1);
}

void ChannelCommand::writeGb(std::ostream& ostream) const {
  // channel commands are even numbered, starting at 2
  ostream.put(type * 2 + 2);
  switch(type) {
  case CHANNEL_CMD_KEY_OFF: break;
  case CHANNEL_CMD_SET_SND_LEN: /* TODO */ throw "Unimplemented";
  case CHANNEL_CMD_OCTAVE_UP: break;
  case CHANNEL_CMD_OCTAVE_DOWN: break;
  case CHANNEL_CMD_SET_INSTRUMENT: ostream.put(newInstrument); break;
  }      
}

void Instrument::writeGb(std::ostream& ostream) const {
  for(auto i = commands.begin(); i != commands.end(); ++i) {
    i->writeGb(ostream);
  }
}

void GbNote::addCommand(const ChannelCommand& command) {
  commands.push_back(command);
}

void Instrument::addCommand(const InstrumentCommand& command) {
  commands.push_back(command);
}

void InstrumentCommand::writeGb(std::ostream& ostream) const {
  ostream.put(type);
  switch(type) {
  case INSTR_END_FRAME: break;
  case INSTR_VOL: ostream.put(newVolume); break;
  case INSTR_MARK: break;
  case INSTR_LOOP: break;
  case INSTR_PITCH: ostream.put(newPitch); break;
  case INSTR_HPITCH: ostream.put(newHiPitch); break;
  case INSTR_DUTY_LO: break;
  case INSTR_DUTY_25: break;
  case INSTR_DUTY_50: break;
  case INSTR_DUTY_75: break;
  }
}
