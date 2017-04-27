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

#include <sstream>

Wave::Wave() : samples{0} {}

void Wave::writeGb(std::ostream& ostream) const {
  for(size_t i = 0; i < Wave::SAMPLE_CNT; i += 2) {
    ostream.put(samples[i] << 4 | samples[i+1]);
  }
}

void Wave::setSample(size_t sampleNum, uint8_t value) {
  if(sampleNum >= Wave::SAMPLE_CNT) {
    throw "Sample index out of bounds";
  }
  if(value >= 16) {
    throw "Sample does not fit into four bits";
  }
  samples[sampleNum] = value;
}

class SongMasterConfig {
 public:
  SongMasterConfig() : tempo(0), channelControl(0x77), outputTerminals(0xFF) {}

  void setTempo(uint8_t tempo) {
    this->tempo = tempo;
  }

  void writeGb(std::ostream& ostream) const {
    ostream.put(channelControl);
    ostream.put(outputTerminals);
    ostream.put(tempo);
  }

  static const uint16_t GB_SIZE = 3;

 private:
  uint8_t tempo;
  uint8_t channelControl;
  uint8_t outputTerminals;
};

class Pattern {
 public:
  void writeGb(std::ostream& ostream) const {
    uint16_t length = getLength();
    ostream.put(length & 0x00FF);
    ostream.put(length >> 8);
    for (const auto& row : rows) {
      row.writeGb(ostream);
    }
  }

  void addRow(const Row& row) {
    rows.push_back(row);
  }

  void addJump(PatternNumber to) {
    Row row;
    row.jump(to.toInt());
    rows.push_back(row);
  }

  uint16_t getLength(void) const {
    uint16_t length = 0;
    for (const auto& row : rows) {
      length += row.getLength();
    }
    length++; // for the end of pattern byte
    return length;
  }

 private:
  std::vector<Row> rows;
};

bool operator==(const PatternNumber& n, const PatternNumber& n_) {
  return n.patternNumber == n_.patternNumber;
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

  void addRow(const Row& row, PatternNumber i) {
    patterns.at(i.toInt()).addRow(row);
  }

  void addJump(PatternNumber from, PatternNumber to) {
    patterns.at(from.toInt()).addJump(to);
  }

  void addInstrument(const GbInstrument& instrument) {
    instruments.push_back(instrument);
  }

  void writeGb(std::ostream& ostream) const {
    Writer writer(*this, ostream);
    writer.writeGb();
  }

  void addPattern(void) {
    patterns.push_back(Pattern());
  }

  void terminateLastPattern(void) {
    Pattern& lastPattern = patterns.back();
    Row terminatingRow;
    terminatingRow.stop();
    lastPattern.addRow(terminatingRow);
  }

  void addWave(const Wave& wave) {
    waves.push_back(wave);
  }

private:
  std::vector<GbInstrument> instruments;
  SongMasterConfig songMasterConfig;
  std::vector<Pattern> patterns;
  std::vector<Wave> waves;

  class Writer {
  public:
    Writer(const SongImpl& song, std::ostream& ostream) : song(song), ostream(ostream), opcodeAddress(0) {}

    void writeGb(void) {
      opcodeAddress = computeOpcodeAddress();
      writeSongMasterConfig();
      writeWaves();
      writePatternTable();
      writeInstrumentTable();
      writePatterns();
      writeInstruments();
    }

  private:
    const SongImpl& song;
    std::ostream& ostream;
    uint16_t opcodeAddress;

    uint16_t computeOpcodeAddress() const {
      return 
	SongMasterConfig::GB_SIZE
	+ (song.waves.size() ? song.waves.size() * 16 : 1) + 1
	+ song.patterns.size() * 2 + 1
	+ song.instruments.size() * 2 + 1;
    }

    void writeSongMasterConfig(void) {
      song.songMasterConfig.writeGb(ostream);
    }

    void writeWaves(void) {
      uint8_t waveBytes = song.waves.size() * 16;
      if(waveBytes) {
	ostream.put(waveBytes);
	for(const auto& wave : song.waves) {
	  wave.writeGb(ostream);
	}
      } else {
	// since 0 => 256, put a dummy byte in
	ostream.put(1);
	ostream.put(0);
      }
    }

    void writeInstrumentTable(void) {
      ostream.put(song.instruments.size() * 2);
      for (const auto& instrument : song.instruments) {
	ostream.put(opcodeAddress & 0x00FF);
	ostream.put(opcodeAddress >> 8);
	opcodeAddress += instrument.getLength() + 1; // extra byte for length
      }
    }

    void writePatternTable(void) {
      ostream.put(song.patterns.size() * 2);
      for (const auto& pattern : song.patterns) {
	ostream.put(opcodeAddress & 0x00FF);
	ostream.put(opcodeAddress >> 8);
	opcodeAddress += pattern.getLength() + 2; // two extra bytes for length
      }
    }

     void writeInstruments(void) {
       for (const auto& instrument : song.instruments) {
	instrument.writeGb(ostream);
      }
    }

    // TODO: compression
    void writePatterns(void) {
      for (const auto& pattern : song.patterns) {
	pattern.writeGb(ostream);
      }
    }
  };
};

Song::Song() : impl(std::make_unique<SongImpl>()) {}
Song::Song(Song&& song) : impl(std::move(song.impl)) {}
Song::~Song() {}

void Song::addInstrument(const GbInstrument& instrument) {
  impl->addInstrument(instrument);
}

void Song::setTempo(uint8_t tempo) {
  impl->setTempo(tempo);
}

void Song::writeGb(std::ostream& ostream) const {
  impl->writeGb(ostream);
}

void Song::addPattern(void) {
  impl->addPattern();
}

void Song::terminateLastPattern(void) {
  impl->terminateLastPattern();
}

std::ostream& operator<<(std::ostream& ostream, const PatternNumber& patternNumber) {
  ostream << (unsigned)patternNumber.patternNumber;
  return ostream;
}

PatternNumber::PatternNumber(void) : patternNumber(-1) {}

PatternNumber::PatternNumber(uint8_t n) : patternNumber(n) {}

PatternNumber PatternNumber::next(void) const {
  if(patternNumber == 0xFF) {
    return PatternNumber(0);
  } else {
    return PatternNumber(patternNumber + 1);
  }
}

GbNote::GbNote() : pitch(0) {}

GbNote::GbNote(uint8_t pitch) : pitch(pitch) {}

Row::Row() : hasFlowControlCommand(false) {}

void Row::setSquareNote1(const GbNote& note) {
  squareNote1 = note;
}

void Row::setSquareNote2(const GbNote& note) {
  squareNote2 = note;
}

void Row::setWaveNote(const GbNote& note) {
  waveNote = note;
}

void Row::setNoiseNote(const GbNote& note) {
  noiseNote = note;
}

void Song::addRow(const Row& row, PatternNumber i) {
  impl->addRow(row, i);
}

void Song::addJump(PatternNumber from, PatternNumber to) {
  impl->addJump(from, to);
}

void Song::addWave(const Wave& wave) {
  impl->addWave(wave);
}

int PatternNumber::toInt() const {
  return patternNumber;
}

void Row::writeGb(std::ostream& ostream) const {
  if(this->hasFlowControlCommand) {
    engineCommands.at(0).writeGb(ostream);
  } else {
    for (const auto& engineCommand : engineCommands) {
      engineCommand.writeGb(ostream);
    }
    // end engine commands
    ostream.put(0);

    squareNote1.writeGb(ostream);
    squareNote2.writeGb(ostream);
    waveNote.writeGb(ostream);
    noiseNote.writeGb(ostream);
  }
}

uint16_t Row::getLength(void) const {
  if(this->hasFlowControlCommand) {
    return 1;
  } else {
    uint16_t length = 0;
    for (const auto& engineCommand : engineCommands) {
      length += engineCommand.getLength();
    }
    length++; // end of engine commands

    length += squareNote1.getLength();
    length += squareNote2.getLength();
    length += waveNote.getLength();
    length += noiseNote.getLength();

    return length;
  }
}

void EngineCommand::writeGb(std::ostream& ostream) const {
  // engine commands increment by two to make table lookup faster
  // also, 0 is a NOP, so they start at 1 (3, 5, ...)
  ostream.put(type);
  switch(type) {
  case ENGINE_CMD_SET_RATE: ostream.put(newRate); break;
  case ENGINE_CMD_STOP: break;
  case ENGINE_CMD_END_OF_PAT: break;
  case ENGINE_CMD_JMP_FRAME: ostream.put(newFrame); break;
  }
}

uint16_t EngineCommand::getLength(void) const {
  switch(type) {
  case ENGINE_CMD_SET_RATE: return 2;
  case ENGINE_CMD_STOP: return 1;
  case ENGINE_CMD_END_OF_PAT: return 1;
  case ENGINE_CMD_JMP_FRAME: return 2;
  }
  
  std::stringstream err;
  err << "internal error - invalid engine command " << type;
  throw err.str();
}

void GbNote::writeGb(std::ostream& ostream) const {
  for (const auto& command : commands) {
    command.writeGb(ostream);
  }

  // notes are odd numbered... (1, 3, ...)
  // (except for 0, which is a NOP)
  ostream.put(pitch ? pitch * 2 + 1 : 0);
}

uint16_t GbNote::getLength(void) const {
  uint16_t length = 0;
  for (const auto& command : commands) {
    length += command.getLength();
  }
  length++; // for the note
  return length;
}

void ChannelCommand::writeGb(std::ostream& ostream) const {
  // channel commands are even numbered, starting at 2
  ostream.put(type * 2 + 2);
  switch(type) {
  case CHANNEL_CMD_KEY_OFF: break;
  case CHANNEL_CMD_SET_SND_LEN: /* TODO */ throw "Unimplemented";
  case CHANNEL_CMD_OCTAVE_UP: break;
  case CHANNEL_CMD_OCTAVE_DOWN: break;
  case CHANNEL_CMD_SET_INSTRUMENT: ostream.put(newInstrument * 2); break;
  case CHANNEL_CMD_SET_WAVE: ostream.put(newWave); break;
  }      
}

uint16_t ChannelCommand::getLength(void) const {
  switch(type) {
  case CHANNEL_CMD_KEY_OFF: return 1;
  case CHANNEL_CMD_SET_SND_LEN: return 2;
  case CHANNEL_CMD_OCTAVE_UP: return 1;
  case CHANNEL_CMD_OCTAVE_DOWN: return 1;
  case CHANNEL_CMD_SET_INSTRUMENT: return 2;
  case CHANNEL_CMD_SET_WAVE: return 2;
  }
  std::stringstream err;
  err << "Internal error - invalid channel command " << type;
  throw err.str();
}

// TODO: use some tasteful inheritence for commands and stuff
// to get rid of switch statements
void GbInstrument::writeGb(std::ostream& ostream) const {
  ostream.put(getLength());
  for (const auto& command : commands) {
    command.writeGb(ostream);
  }
}

uint8_t GbInstrument::getLength(void) const {
  uint8_t length = 0;
  for (const auto& command : commands) {
    length += command.getLength();
  }
  return length;
}

void GbNote::addCommand(const ChannelCommand& command) {
  commands.push_back(command);
}

void GbInstrument::addCommand(const InstrumentCommand& command) {
  commands.push_back(command);
}

void InstrumentCommand::writeGb(std::ostream& ostream) const {
  ostream.put(type);
  switch(type) {
  case INSTR_END: break;
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
  case INSTR_SETWAVE: ostream.put(newWave); break;
  }
}

uint8_t InstrumentCommand::getLength(void) const {
  switch(type) {
  case INSTR_END: return 1;
  case INSTR_END_FRAME: return 1;
  case INSTR_VOL: return 2;
  case INSTR_MARK: return 1;
  case INSTR_LOOP: return 1;
  case INSTR_PITCH: return 2;
  case INSTR_HPITCH: return 2;
  case INSTR_DUTY_LO: return 1;
  case INSTR_DUTY_25: return 1;
  case INSTR_DUTY_50: return 1;
  case INSTR_DUTY_75: return 1;
  case INSTR_SETWAVE: return 2;
  }
  std::stringstream err;
  err <<  "internal error - invalid instrument command type " << type;
  throw err.str();
}

void Row::ensureUnlocked(void) const {
  if(this->hasFlowControlCommand) {
    throw "Multiple flow control commands in a single row are forbidden.";
  }
}

void Row::setFlowControlCommand(EngineCommand command) {
  this->ensureUnlocked();
  engineCommands.clear();
  engineCommands.push_back(command);
  this->hasFlowControlCommand = true;
}

void Row::endOfPattern(void) {
  EngineCommand command;
  command.type = ENGINE_CMD_END_OF_PAT;
  setFlowControlCommand(command);
}

void Row::stop(void) {
  EngineCommand command;
  command.type = ENGINE_CMD_STOP;
  setFlowControlCommand(command);
}

// TODO: maybe use patternnumber here
void Row::jump(uint8_t newFrame) {
  EngineCommand command;
  command.type = ENGINE_CMD_JMP_FRAME;
  command.newFrame = newFrame;
  setFlowControlCommand(command);
}    

size_t std::hash<PatternNumber>::operator()(const PatternNumber& patternNumber) const {
  std::hash<uint8_t> byteHash;

  return byteHash(patternNumber.patternNumber);
}

