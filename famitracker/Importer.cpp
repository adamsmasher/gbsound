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

#include "Importer.h"

#include "APUTypes.h"
#include "FamiTrackerTypes.h"
#include "hash.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <math.h>
#include <strings.h>
#include <sstream>
#include <tuple>
#include <unordered_map>

const bool debug = true;

static const char* CT[CT_LAST + 1] = {
  // comment
  "#",
  // song info
  "TITLE",
  "AUTHOR",
  "COPYRIGHT",
  "COMMENT",
  // global settings
  "MACHINE",
  "FRAMERATE",
  "EXPANSION",
  "VIBRATO",
  "SPLIT",
  // namco global settings
  "N163CHANNELS",
  // macros
  "MACRO",
  "MACROVRC6",
  "MACRON163",
  "MACROS5B",
  // dpcm
  "DPCMDEF",
  "DPCM",
  // instruments
  "INST2A03",
  "INSTVRC6",
  "INSTVRC7",
  "INSTFDS",
  "INSTN163",
  "INSTS5B",
  // instrument data
  "KEYDPCM",
  "FDSWAVE",
  "FDSMOD",
  "FDSMACRO",
  "N163WAVE",
  // track info
  "TRACK",
  "COLUMNS",
  "ORDER",
  // pattern data
  "PATTERN",
  "ROW",
};

static uint8_t computeTempo(int speed, int tempo) {
  // the following is how we derived the much nicer formula below
  /*double bpm = (tempo * 6.0)/speed;
  double rowsPerMinute = bpm * 4;
  double rowsPerSecond = rowsPerMinute/60;
  double ticksPerRow = 60/rowsPerSecond;
  return (uint8_t)ceil(256/ticksPerRow);*/
  return (256 * tempo)/(150 * speed);
}

typedef uint8_t Chip;

class InstrSequenceIndex {
 public:
  typedef int SeqType;

  InstrSequenceIndex(Chip chip, SeqType seqType, int num) :
    index(std::make_tuple(chip, seqType, num))
  {}
  
  Chip getChip() const {
    return std::get<0>(index);
  }
  
  SeqType getType() const {
    return std::get<1>(index);
  }
  
  int getNum() const {
    return std::get<2>(index);
  }
  
  friend bool operator==(const InstrSequenceIndex&, const InstrSequenceIndex&);

  operator bool() const {
    return std::get<2>(index) >= 0;
  }
private:
  std::tuple<Chip, SeqType, int> index;
};

namespace std {
  template <> struct hash<InstrSequenceIndex> {
    size_t operator()(const InstrSequenceIndex&) const;
  };
}

size_t std::hash<InstrSequenceIndex>::operator()(const InstrSequenceIndex& sequenceIndex) const {
  std::hash<Chip> chipHash;
  std::hash<InstrSequenceIndex::SeqType> seqTypeHash;
  std::hash<uint8_t> byteHash;

  return hash_combine(chipHash(sequenceIndex.getChip()),
		      hash_combine(seqTypeHash(sequenceIndex.getType()),
				   byteHash(sequenceIndex.getNum())));
}

bool operator==(const InstrSequenceIndex& sequenceIndex, const InstrSequenceIndex& sequenceIndex_) {
  return sequenceIndex.index == sequenceIndex_.index;
}

class Note {
public:
  Note(int pitchClass, int octave) : pitchClass(pitchClass), octave(octave) {}
  int getPitchClass(void) const { return pitchClass; }
  int getOctave(void) const { return octave; }

  uint8_t toGbPitch(void) const {
    if (octave < 2 || (octave == 2 && pitchClass >= 9)) {
      // nothing lower than C-2 is representable, apparently
      throw "Note unrepresentable on GB.";
    }
    return 12 * (octave - 2) + pitchClass;
  }

  uint8_t toWavePitch(void) const {
    Note octaveUp(this->pitchClass, this->octave + 1);
    return octaveUp.toGbPitch();
  }

  operator bool() const {
    return pitchClass || octave;
  }
private:
  int pitchClass;
  int octave;
};

enum EffectType {
  EFFECT_NO_EFFECT,
  EFFECT_JUMP,
  EFFECT_END_OF_PATTERN,
  EFFECT_STOP
};

static EffectType effectTypeOfId(int id) {
  switch(id) {
  case 1: return EFFECT_JUMP;
  case 2: return EFFECT_END_OF_PATTERN;
  case 3: return EFFECT_STOP;
  default: throw "Unsupported effect";
  }
}

struct Effect {
  EffectType type;
  int param;
};

class Cell {
public:
  Cell(Note note, int instrumentId, Effect effect) :
    note(note), instrumentId(instrumentId), effect(effect)
  {}

  Note getNote(void) const { return note; }
  int getInstrumentId(void) const { return instrumentId; }
  Effect getEffect(void) const { return effect; }
private:
  Note note;
  int instrumentId;
  Effect effect;
};

enum ArpeggioType {
  NON_ARPEGGIO,
  ABSOLUTE,
  FIXED,
  RELATIVE
};

class InstrSequence {
 public:
  InstrSequence() : loopPoint(-1) {}
  InstrSequence(sequence_t type) 
    : type(type), loopPoint(0),  arpeggioType(NON_ARPEGGIO) {}

  size_t getLength(void) const { return sequence.size(); }

  int getLoopPoint(void) const { return loopPoint; }

  void setLoopPoint(int loopPoint) {
    if(loopPoint < -1 || (loopPoint >= 0 && (unsigned)loopPoint > sequence.size())) {
      throw "Loop point out of range";
    }
    this->loopPoint = loopPoint;
  }

  void setArpeggioType(ArpeggioType arpeggioType) {
    if(this->type != SEQ_ARPEGGIO) {
      throw "Cannot set arpeggio type for non-arpeggio sequence!";
    }
    this->arpeggioType = arpeggioType;
  }

  uint8_t at(size_t i) const { return sequence.at(i); }

  void pushBack(uint8_t i) {
    sequence.push_back(i);
  }

  static const InstrSequence emptySequence;

  operator bool() const {
    return sequence.size() > 0;
  }
 private:
  std::vector<uint8_t> sequence;
  sequence_t type;
  int loopPoint;
  ArpeggioType arpeggioType;
};

const InstrSequence InstrSequence::emptySequence = InstrSequence();

class ImporterImpl {
public:
  ImporterImpl(const std::string& text) :
    isExpired(false), t(text), track(0), pattern(-1), hasN163(false),
    lastWaveRead(-1)
  {}

  Song runImport(void) {
    if(isExpired) {
      throw "Cannot run the same Importer multiple times";
    }
    isExpired = true;

    // parse the file
    while (!t.finished()) {
      // read first token on line
      if (t.isEOL()) {
	continue; // blank line
      }
    
      std::string command = t.readToken();
      Command c = getCommandEnum(command);

      importCommand(c);
    }

    song.terminateLastPattern();

    return std::move(song);
  }

private:
  bool isExpired;
  Tokenizer t;
  unsigned int track;
  // this is used both to track the current pattern we're on as we read orders
  // and also to track the pattern we're on as we read patterns
  PatternNumber pattern;
  std::unordered_map<PatternNumber, PatternNumber> jumps;
  int channel;
  uint8_t currentInstruments[6]; // two dummy channels, one for triangle, one for DPCM
  bool hasN163;
  std::unordered_map<InstrSequenceIndex, InstrSequence> instrSequenceTable;
  Song song;
  uint8_t speedSplitPoint;
  enum {
    IMPORTING_ORDERS,
    IMPORTING_PATTERNS
  } state;
  std::unordered_map<int, int> waveForInstrument;
  int lastWaveRead;
  
  void ignoreVolId() {
    std::string sVol = t.readToken();

    if(sVol != ".") {
      std::stringstream errMsg;
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": unrecognized volume token '" << sVol << "'; volume column is unsupported";
      throw errMsg.str();
    }
  }
  
  int getInstrumentId(const std::string& sInst) const {
    std::ostringstream errMsg;
  
    if (sInst.size() != 2) {
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": instrument column should be 2 characters wide, '" << sInst << "' found.";
      throw errMsg.str();
    } else if (sInst == "..") {
      return MAX_INSTRUMENTS;
    } else {
      int h = importHex(sInst);

      if (h >= MAX_INSTRUMENTS) {
	errMsg << "Line " << t.getLine() << " column " << ": instrument '" << sInst << "' is out of bounds.";
	throw errMsg.str();
      }

      return h;
    }
  }

  Note readNote() {
    std::ostringstream errMsg;

    std::string sNote = t.readToken();
    if (sNote.size() != 3) {
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": note column should be 3 characters wide, '" << sNote << "' found.";
      throw errMsg.str();
    } else if (sNote == "...") {
      return Note(0, 0);
    } else if (sNote == "---") {
      return Note(HALT, 0);
    } else if (sNote == "===") {
      return Note(RELEASE, 0);
    } else if (channel == CHANID_NOISE) {
      int h = importHex(sNote.substr(0, 1));

      // importer is very tolerant about the second and third characters
      // in a noise note, they can be anything

      return Note(h % 12 + 1, h / 12);
    } else if (channel == CHANID_TRIANGLE) {
      std::ostringstream errMsg;
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": non-empty cell " << sNote << " in triangle channel";
      throw errMsg.str();
    } else if (channel == CHANID_DPCM) {
      std::ostringstream errMsg;
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": non-empty cell " << sNote  << " in DPCM channel";
      throw errMsg.str();
    } else {
      int n;
      switch (sNote.at(0)) {
      case 'c': case 'C': n = 0; break;
      case 'd': case 'D': n = 2; break;
      case 'e': case 'E': n = 4; break;
      case 'f': case 'F': n = 5; break;
      case 'g': case 'G': n = 7; break;
      case 'a': case 'A': n = 9; break;
      case 'b': case 'B': n = 11; break;
      default:
	errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": unrecognized note '" << sNote << "'.";
	throw errMsg.str();
      }

      switch (sNote.at(1)) {
      case '-': case '.': break;
      case '#': case '+': n += 1; break;
      case 'b': case 'f': n -= 1; break;
      default:
	errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": unrecognized note '" << sNote << "'.";
	throw errMsg.str();
      }

      while (n <   0) n += 12;
      while (n >= 12) n -= 12;

      int o = sNote.at(2) - '0';
      if (o < 0 || o >= OCTAVE_RANGE) {
	errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": unrecognized octave '" << sNote << "'.";
	throw errMsg.str();
      }

      return Note(n + 1, o);
    }
  }

  std::string readEffectColumn(void) {
    std::string effectColumn = t.readToken();
    
    if (effectColumn.size() != 3) {
      std::ostringstream errMsg;
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": effect column should be 3 characters wide, '" << effectColumn << "' found.";
      throw errMsg.str();
    }

    return effectColumn;
  }

  EffectType getEffectType(const std::string& effectColumn) const {
    char c = toupper(effectColumn.at(0));
    for (int i = 0; i < EF_COUNT; i++) {
      if (EFF_CHAR[i] == c) {
	return effectTypeOfId(i);
      }
    }

    std::ostringstream errMsg;
    errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": unrecognized effect '" << effectColumn << "'.";
    throw errMsg.str();
  }

  Effect readEffect(void) {
    Effect effect;

    std::string effectColumn = readEffectColumn();
    if (effectColumn == "...") {
      effect.type = EFFECT_NO_EFFECT;
    } else {
      effect.type = getEffectType(effectColumn);
      effect.param = importHex(effectColumn.substr(effectColumn.size() - 2));
    }

    return effect;
  }

  Cell importCell(void) {
    Note note = readNote();
    int instrumentId = getInstrumentId(t.readToken());
    ignoreVolId();

    // only one effect column per channel is allowed
    Effect effect = readEffect();

    return Cell(note, instrumentId, effect);
  }

  int hexCharVal(char c) const {
    const char HEX_TEXT[16] = {
      '0', '1', '2', '3', '4', '5', '6', '7',
      '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };

    for (int i = 0; i < 16; i++) {
      if (toupper(c) == HEX_TEXT[i]) {
	return i;
      }
    }

    std::ostringstream errMsg;
    errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": hexadecimal char expected, '" << c << "' found.";
    throw errMsg.str();
  }

  int importHex(const std::string& sToken) const {
    int n = 0;
    for (size_t i = 0; i < sToken.size(); i++) {
      n <<= 4;
      n += hexCharVal(sToken.at(i));
    }

    return n;
  }
  
  Command getCommandEnum(const std::string& command) const {
    for (int c = 0; c <= CT_LAST; ++c) {
      if (0 == strcasecmp(command.c_str(), CT[c])) {
        return (Command)c;
      }
    }
  
    std::ostringstream errMsg;
    errMsg << "Unrecognized command at line " << t.getLine() << ": '" << command << "'.";
    throw errMsg.str();
  }

  void importCommand(Command c) {
    if(debug) {
      std::cerr << commandName(c) << std::endl;
    }
    switch (c) {
    case CT_COMMENTLINE:
      t.finishLine();
      return;
    case CT_TITLE:
      std::cout << "Title: " << t.readToken() << std::endl;
      t.readEOL();
      return;
    case CT_AUTHOR:
      std::cout << "Author: " << t.readToken() << std::endl;
      t.readEOL();
      return;
    case CT_COPYRIGHT:
      std::cout << "Copyright: " << t.readToken() << std::endl;
      t.readEOL();
      return;
    case CT_COMMENT:
      std::cout << "Comment: " << t.readToken() << std::endl;
      t.readEOL();
      break;
    case CT_MACHINE:
      importMachine();
      return;
    case CT_FRAMERATE:
      importFramerate();
      return;
    case CT_EXPANSION:
      importExpansion();
      return;
    case CT_VIBRATO:
      importVibrato();
      return;
    case CT_SPLIT:
      importSplit();
      return;
    case CT_MACRO:
      importStandardInstrSequence();
      return;
    case CT_INST2A03:
      importStandardInstrument();
      return;
    case CT_INSTFDS:
    case CT_FDSWAVE:
    case CT_FDSMOD:
    case CT_FDSMACRO:
      throw "FDS not supported on the Game Boy.";
    case CT_KEYDPCM:
    case CT_DPCMDEF:
    case CT_DPCM:
      throw "DPCM not supported on the Game Boy.";
    case CT_INSTVRC6:
    case CT_MACROVRC6:
      throw "VRC6 not supported on the Game Boy.";
    case CT_INSTVRC7:
      throw "VRC7 not supported on the Game Boy.";
    case CT_INSTS5B:
    case CT_MACROS5B:
      throw "S5B not supported on the Game Boy.";
    case CT_INSTN163:
      importN163Instrument();
      return;
    case CT_N163WAVE:
      importN163Wave();
      return;
    case CT_MACRON163:
      importN163InstrSequence();
      return;
    case CT_N163CHANNELS:
      importN163Channels();
      return;
    case CT_TRACK:
      importTrack();
      return;
    case CT_COLUMNS:
      importColumns();
      return;
    case CT_ORDER:
      importOrder();
      return;
    case CT_PATTERN:
      importPattern();
      return;
    case CT_ROW:
      importRow();
      return;
    }
  }

  sequence_t readSequenceType(void) {
    return (sequence_t)t.readInt(0, SEQ_COUNT - 1);
  }

  ArpeggioType readArpeggioType(void) {
    return (ArpeggioType)(t.readInt(0, 255) + 1);
  }

  void ignoreReleasePoint(void) {
    int releasePoint = t.readInt(-1, MAX_SEQUENCE_ITEMS);
    if(releasePoint != -1) {
      throw "Instrument has release point, unsupported.";
    }
  }

  void importInstrSequence(Chip chip) {
    sequence_t sequenceType = readSequenceType();
    InstrSequence sequence(sequenceType);
    
    int sequenceNum = t.readInt(0, MAX_SEQUENCES - 1);

    int loopPoint    = t.readInt(-1, MAX_SEQUENCE_ITEMS);
    
    ignoreReleasePoint();

    ArpeggioType arpeggioType = readArpeggioType();
    if(sequenceType == SEQ_ARPEGGIO) {
      sequence.setArpeggioType(arpeggioType);
    } else if (arpeggioType != ABSOLUTE) {
      // in the file, 0 is both absolute and non-arpeggio sequence
      // so if it's absolute and a non-arpeggio sequence, we're ok
      // otherwise raise
      throw "Non-arpeggio sequence given arpeggio type!";
    }

    checkColon();

    while (!t.isEOL()) {
      int i = t.readInt(-128, 127);
      sequence.pushBack(i);
    }

    sequence.setLoopPoint(loopPoint);

    addInstrSequence(InstrSequenceIndex(chip, sequenceType, sequenceNum), sequence);
  }
  
  void importStandardInstrSequence(void) {
    importInstrSequence(SNDCHIP_NONE);
  }

  void importStandardInstrument(void) {
    skipInstrumentNumber();
  
    int volume    = readSequenceNumber();
    int arpeggio  = readSequenceNumber();
    int pitch     = readSequenceNumber();
    int hiPitch   = readSequenceNumber();
    int dutyCycle = readSequenceNumber();

    song.addInstrument(buildStdGbInstrument(volume, arpeggio, pitch, hiPitch, dutyCycle));

    skipInstrumentName();
    t.readEOL();
  }

  // thanks to the magic of std::vector, we don't need to know the pattern length
  // in advance
  void ignorePatternLength(void) {
    t.readInt(0, MAX_PATTERN_LENGTH);
  }

  void importTrack(void) {
    if (track != 0) {
      throw "Multiple tracks not supported";
    }

    ignorePatternLength();
    
    int speed = t.readInt(0, MAX_TEMPO);
    int tempo = t.readInt(0, MAX_TEMPO);
    song.setTempo(computeTempo(speed, tempo));

    skipTrackTitle();
    t.readEOL();
    ++track;
  }
    
  void importColumns(void) {
    checkColon();
    for (int c = 0; c < getChannelCount(); ++c) {
      int i = t.readInt(1, MAX_EFFECT_COLUMNS);
      if(i != 1) {
	throw "All channels must have 1 effect column.";
      }
    }
    t.readEOL();
  }

  void addJump(PatternNumber from, PatternNumber to) {
    jumps[from] = to;
  }
    
  void importOrder(void) {
    state = IMPORTING_ORDERS;

    skipFrameNumber();

    PatternNumber pattern = readPatternNumber();

    // unlike in Famitracker, in our driver there's only one
    // pattern per frame
    for (int c = 1; c < getChannelCount(); ++c) {
      PatternNumber pattern_ = readPatternNumber();
      if(pattern != pattern_) {
	std::ostringstream errMsg;
	errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": Mismatched pattern number, expected " << pattern << " got " << pattern_;
	throw errMsg.str();
      }
    }
    t.readEOL();

    if(pattern != this->pattern.next()) {
      addJump(this->pattern, pattern);
    }

    this->pattern = pattern;
  }

  void importRow(void) {
    if (track == 0) {
      std::ostringstream errMsg;
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": no TRACK defined, cannot add ROW data.";
      throw errMsg.str();
    }

    Row row;

    int i = t.readHex(0, MAX_PATTERN_LENGTH - 1);
    int gbChannel = 0;
    for (this->channel = 0; channel < getChannelCount(); channel++) {
      // read the cell - we need to actually read the data for all
      // channels, even if we're going to ignore some (non GB ones)
      checkColon();
      Cell cell = importCell();
      Note note = cell.getNote();
      int instrument = cell.getInstrumentId();
      Effect effect = cell.getEffect();

      // don't actually do anything if this isn't a GB channel
      switch(channel) {
      case CHANID_SQUARE1:
      case CHANID_SQUARE2:
      case N163_INDEX:
      case CHANID_NOISE:
	break;
      default:
	continue;
      }

      switch(effect.type) {
      case EFFECT_NO_EFFECT: break;
      case EFFECT_JUMP:
	row.jump(effect.param);
	break;
      case EFFECT_END_OF_PATTERN:
	row.endOfPattern();
	break;
      case EFFECT_STOP:
	row.stop();
	break;
      }

      if(note) {
	GbNote gbNote(channel == N163_INDEX ? note.toWavePitch() : note.toGbPitch());
	if(cell.getInstrumentId() != currentInstruments[channel]) {
	  ChannelCommand command;
	  command.type = CHANNEL_CMD_SET_INSTRUMENT;
	  command.newInstrument = instrument;
	  currentInstruments[channel] = instrument;
	  gbNote.addCommand(command);
	  if (channel == N163_INDEX) {
	    ChannelCommand command;
	    command.type = CHANNEL_CMD_SET_WAVE;
	    command.newWave = waveForInstrument.at(instrument) * 16;
	    gbNote.addCommand(command);
	  }
	}

	switch(channel) {
	case CHANID_SQUARE1:
	  row.setSquareNote1(gbNote);
	  break;
	case CHANID_SQUARE2:
	  row.setSquareNote2(gbNote);
	  break;
	case N163_INDEX:
	  row.setWaveNote(gbNote);
	  break;
	case CHANID_NOISE:
	  row.setNoiseNote(gbNote);
	  break;
	}
      }
      gbChannel++;
    }
    t.readEOL();

    song.addRow(row, pattern);
  }

  void importMachine(void) {
    int i = t.readInt(0, PAL);
    if(i == PAL) {
      throw "The Game Boy always runs at 60Hz. PAL songs are not supported.";
    }
    t.readEOL();
  }

  void importVibrato(void) {
    t.readInt(0, VIBRATO_NEW);
    std::cout << "Warning: ignoring vibrato command." << std::endl;
    t.readEOL();
  }

  void importSplit(void) {
    // changes the point where the speed adjustment effect, Fxx
    // switches to changing tempo (or vice versa)
    this->speedSplitPoint = t.readInt(0, 255);
    t.readEOL();
  }
    
  void importFramerate(void) {
    int i = t.readInt(0, 800);
    if(i != 0 && i != 60) {
      throw "Engine speed must be 60Hz.";
    }
    t.readEOL();
  }
    
  void importPattern(void) {
    PatternNumber pattern(t.readHex(0, MAX_PATTERN - 1));

    switch(state) {
    case IMPORTING_ORDERS:
      state = IMPORTING_PATTERNS;
      this->pattern = PatternNumber(0xFF);
      break;
    case IMPORTING_PATTERNS:
      if(jumps.count(this->pattern)) {
	song.addJump(this->pattern, jumps[this->pattern]);
      } else {
	Row row;
	row.endOfPattern();
	song.addRow(row, this->pattern);
      }
      break;
    }

    if(pattern != this->pattern.next()) {
      std::stringstream errMsg;
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": bad pattern order; expected " << this->pattern.next() << ", got " << pattern;
      throw errMsg.str();
    }

    this->pattern = pattern;
    song.addPattern();
    t.readEOL();
  }

  void importExpansion(void) {
    int i = t.readInt(0, 255);
    switch(i) {
    case SNDCHIP_NONE: hasN163 = false; break;
    case SNDCHIP_N163: hasN163 = true;  break;
    default: throw "Unsupported expansion.";
    }
    
    t.readEOL();
  }
    
  void checkColon(void) {
    checkSymbol(":");
  }
    
  void checkSymbol(const std::string& x) {
    std::string symbol = t.readToken();
    if (symbol != x) {
      std::ostringstream errMsg;
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": expected '" << x << "', '" << symbol << "' found.";
      throw errMsg.str();
    }
  }

  void importN163Wave(void) {
    int instrumentNumber = t.readInt(0, MAX_INSTRUMENTS - 1);

    int waveNumber = t.readInt(0, 1);
    checkColon();

    Wave wave;
    for (size_t sample = 0; sample < Wave::SAMPLE_CNT; sample++) {
      int value = t.readInt(0, 16);
      wave.setSample(sample, value);
    }
    t.readEOL();

    song.addWave(wave);
    waveForInstrument[instrumentNumber] = ++this->lastWaveRead;
  }

  void importN163Channels(void) {
    int i = t.readInt(1, 8);
    if(i > 1) {
      throw "Only 1 N163 wave channel is supported.";
    }
    t.readEOL();
  }

  void importN163Instrument(void) {
    skipInstrumentNumber();

    int volume    = readSequenceNumber();
    int arpeggio  = readSequenceNumber();
    int pitch     = readSequenceNumber();
    int hiPitch   = readSequenceNumber();
    int wave      = readSequenceNumber();

    int waveSize = t.readInt(0, Wave::SAMPLE_CNT);
    if(waveSize != Wave::SAMPLE_CNT) {
      std::stringstream errMsg;
      errMsg << "Line " << t.getLine() << " Column " << t.getColumn() << ": invalid wave size";
      throw errMsg.str();
    }

    // wave pos - where the sample gets loaded into wave RAM
    // this must be 0 in our engine
    int wavePos = t.readInt(0, 0);

    // TODO: clean this up
    // i.e. put it into a function
    int waveCount = t.readInt(1, 1);
    if(waveCount != 1) {
      throw "invalid wave count";
    }

    song.addInstrument(buildN163GbInstrument(volume, arpeggio, pitch, hiPitch, wave));

    skipInstrumentName();
    t.readEOL();
  }
  
  void importN163InstrSequence(void) {
    importInstrSequence(SNDCHIP_N163);
  }

  void skipInstrumentName(void) {
    t.readToken();
  }

  void skipInstrumentNumber(void) {
    t.readInt(0, MAX_INSTRUMENTS - 1);  
  }
  
  int readSequenceNumber(void) {
    return t.readInt(-1, MAX_SEQUENCES - 1);
  }

  void skipTrackTitle(void) {
    t.readToken();
  }
  
  PatternNumber readPatternNumber(void) {
    return PatternNumber(t.readHex(0, MAX_PATTERN - 1));
  }

  GbInstrument buildStdGbInstrument(int volumeNum, int arpeggioNum, int pitchNum, int hiPitchNum, int dutyCycleNum) {
    GbInstrument instrument;

    auto volumeSeq    = getInstrSequence(InstrSequenceIndex(SNDCHIP_NONE, SEQ_VOLUME,    volumeNum));
    auto arpeggioSeq  = getInstrSequence(InstrSequenceIndex(SNDCHIP_NONE, SEQ_ARPEGGIO,  arpeggioNum));
    auto pitchSeq     = getInstrSequence(InstrSequenceIndex(SNDCHIP_NONE, SEQ_PITCH,     pitchNum));
    auto hiPitchSeq   = getInstrSequence(InstrSequenceIndex(SNDCHIP_NONE, SEQ_HIPITCH,   hiPitchNum));
    auto dutyCycleSeq = getInstrSequence(InstrSequenceIndex(SNDCHIP_NONE, SEQ_DUTYCYCLE, dutyCycleNum));

    // the engine only has one "sequence" for each instrument, so we need to combine
    // all the famitracker sequences into one
    // all sequences MUST be the same length and have the same loop point

    size_t lengths[] = {
      volumeSeq.getLength(),
      arpeggioSeq.getLength(),
      pitchSeq.getLength(),
      hiPitchSeq.getLength(),
      dutyCycleSeq.getLength()
    };

    size_t length = 0;
    for (auto length_ : lengths) {
      if(length) {
	if(length_ && length_ != length) {
	  std::stringstream errMsg;
	  errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": mismatched sequence lengths; expected " << length << ", got " << length_;
	  throw errMsg.str();
	}
      } else {
	length = length_;
      }
    }

    int loopPoints[] = {
      volumeSeq.getLoopPoint(),
      arpeggioSeq.getLoopPoint(),
      pitchSeq.getLoopPoint(),
      hiPitchSeq.getLoopPoint(),
      dutyCycleSeq.getLoopPoint()
    };

    int loopPoint = -1;
    for (auto loopPoint_ : loopPoints) {
      if(loopPoint != -1) {
	if(loopPoint_ != -1 && loopPoint_ != loopPoint) {
	  std::stringstream errMsg;
	  errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": mismatched loop point; expected " << loopPoint << ", got " << loopPoint_;
	  throw errMsg.str();
	}
      } else {
	loopPoint = loopPoint_;
      }
    }

    if(loopPoint < -1) {
      throw "invalid loop point";
    }
    unsigned loopPoint_ = loopPoint == -1 ? 0 : (unsigned)loopPoint;

    InstrumentCommand command;

    int currentVolume = -1;
    int currentPitch = 0;
    int currentHiPitch = 0;
    int currentDuty = -1;
    for(size_t i = 0; i < length; i++) {
      if(i == loopPoint_) {
	command.type = INSTR_MARK;
	instrument.addCommand(command);
      }

      if(volumeSeq) {
	uint8_t volume = volumeSeq.at(i);
	if(volume != currentVolume) {
	  currentVolume = volume;
	  command.type = INSTR_VOL;
	  command.newVolume = volume << 4;
	  instrument.addCommand(command);
	}
      }

      if(pitchSeq) {
	uint8_t pitch = pitchSeq.at(i);
	if(pitch != currentPitch) {
	  currentPitch = pitch;
	  command.type = INSTR_HPITCH;
	  command.newPitch = pitch;
	  instrument.addCommand(command);
	}
      }

      if(hiPitchSeq) {
	uint8_t hiPitch = hiPitchSeq.at(i);
	if(hiPitch != currentHiPitch) {
	  currentHiPitch = hiPitch;
	  command.type = INSTR_HPITCH;
	  command.newHiPitch = hiPitch;
	  instrument.addCommand(command);
	}
      }

      if(dutyCycleSeq) {
	uint8_t duty = dutyCycleSeq.at(i);
	if(duty != currentDuty) {
	  currentDuty = duty;
	  switch(duty) {
	  case 0: command.type = INSTR_DUTY_LO; break;
	  case 1: command.type = INSTR_DUTY_25; break;
	  case 2: command.type = INSTR_DUTY_50; break;
	  case 3: command.type = INSTR_DUTY_75; break;
	  default: throw "Invalid duty";
	  }
	  instrument.addCommand(command);
	}
      }

      command.type = INSTR_END_FRAME;
      instrument.addCommand(command);
    }
    if(loopPoint_ == length) {
      command.type = INSTR_END;
      instrument.addCommand(command);
    } else {
      command.type = INSTR_LOOP;
      instrument.addCommand(command);
    }

    return instrument;
  }

  // TODO: this makes me sad
#define SEQ_WAVE 4
  
  // TODO: combine this with the previous function, this is ridiculous
  GbInstrument buildN163GbInstrument(int volumeNum, int arpeggioNum, int pitchNum, int hiPitchNum, int waveNum) {
    GbInstrument instrument;

    auto volumeSeq   = getInstrSequence(InstrSequenceIndex(SNDCHIP_N163, SEQ_VOLUME,   volumeNum));
    auto arpeggioSeq = getInstrSequence(InstrSequenceIndex(SNDCHIP_N163, SEQ_ARPEGGIO, arpeggioNum));
    auto pitchSeq    = getInstrSequence(InstrSequenceIndex(SNDCHIP_N163, SEQ_PITCH,    pitchNum));
    auto hiPitchSeq  = getInstrSequence(InstrSequenceIndex(SNDCHIP_N163, SEQ_HIPITCH,  hiPitchNum));
    auto waveSeq     = getInstrSequence(InstrSequenceIndex(SNDCHIP_N163, 4,     waveNum));

    // the engine only has one "sequence" for each instrument, so we need to combine
    // all the famitracker sequences into one
    // all sequences MUST be the same length and have the same loop point

    size_t lengths[] = {
      volumeSeq.getLength(),
      arpeggioSeq.getLength(),
      pitchSeq.getLength(),
      hiPitchSeq.getLength(),
      waveSeq.getLength()
    };

    size_t length = 0;
    for (auto length_ : lengths) {
      if(length) {
	if(length_ && length_ != length) {
	  std::stringstream errMsg;
	  errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": mismatched sequence lengths; expected " << length << ", got " << length_;
	  throw errMsg.str();
	}
      } else {
	length = length_;
      }
    }

    int loopPoints[] = {
      volumeSeq.getLoopPoint(),
      arpeggioSeq.getLoopPoint(),
      pitchSeq.getLoopPoint(),
      hiPitchSeq.getLoopPoint(),
      waveSeq.getLoopPoint()
    };

    int loopPoint = -1;
    for (auto loopPoint_ : loopPoints) {
      if(loopPoint != -1) {
	if(loopPoint_ != -1 && loopPoint_ != loopPoint) {
	  std::stringstream errMsg;
	  errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": mismatched loop point; expected " << loopPoint << ", got " << loopPoint_;
	  throw errMsg.str();
	}
      } else {
	loopPoint = loopPoint_;
      }
    }

    if(loopPoint < -1) {
      throw "invalid loop point";
    }
    unsigned loopPoint_ = loopPoint == -1 ? 0 : (unsigned)loopPoint;

    InstrumentCommand command;

    int currentVolume = -1;
    int currentPitch = 0;
    int currentHiPitch = 0;
    int currentWave = -1;
    for(size_t i = 0; i < length; i++) {
      if(i == loopPoint_) {
	command.type = INSTR_MARK;
	instrument.addCommand(command);
      }

      if(volumeSeq) {
	uint8_t volume = volumeSeq.at(i);
	if(volume != currentVolume) {
	  currentVolume = volume;
	  command.type = INSTR_VOL;
	  // note that this is different than for the standard GB channels
	  command.newVolume = (volume >= 4 ? 4 - (volume >> 2) : 0) << 5;
	  instrument.addCommand(command);
	}
      }

      if(pitchSeq) {
	uint8_t pitch = pitchSeq.at(i);
	if(pitch != currentPitch) {
	  currentPitch = pitch;
	  command.type = INSTR_HPITCH;
	  command.newPitch = pitch;
	  instrument.addCommand(command);
	}
      }

      if(hiPitchSeq) {
	uint8_t hiPitch = hiPitchSeq.at(i);
	if(hiPitch != currentHiPitch) {
	  currentHiPitch = hiPitch;
	  command.type = INSTR_HPITCH;
	  command.newHiPitch = hiPitch;
	  instrument.addCommand(command);
	}
      }

      if(waveSeq) {
	uint8_t wave = waveSeq.at(i);
	if(wave != currentWave) {
	  currentWave = wave;
	  command.type = INSTR_SETWAVE;
	  command.newWave = wave;
	  instrument.addCommand(command);
	}
      }

      command.type = INSTR_END_FRAME;
      instrument.addCommand(command);
    }
    if(loopPoint_ == length) {
      command.type = INSTR_END;
      instrument.addCommand(command);
    } else {
      command.type = INSTR_LOOP;
      instrument.addCommand(command);
    }

    return instrument;
  }
  
  int getChannelCount(void) const {
    if(hasN163) {
      return 13;
    } else {
      return 5;
    }
  }

  void skipFrameNumber(void) {
    t.readHex(0, MAX_FRAMES - 1);
    checkColon();
  }

  void addInstrSequence(InstrSequenceIndex index, const InstrSequence& sequence) {
    instrSequenceTable[index] = sequence;
  }
  
  const InstrSequence& getInstrSequence(InstrSequenceIndex i) const {
    if(i) {
      return instrSequenceTable.at(i);
    } else {
      return InstrSequence::emptySequence;
    }
  }
};

Importer::Importer(const std::string& text) : impl(std::make_unique<ImporterImpl>(text)) {}
Importer::Importer(Importer&& importer) : impl(std::move(importer.impl)) {}
Importer::~Importer() {}

Importer Importer::fromFile(const char *filename) {
  std::ifstream f(filename, std::ifstream::in);
  std::stringstream buffer;
  buffer << f.rdbuf();
  f.close();

  return std::move(Importer(buffer.str()));
}

Song Importer::runImport(void) {
  return impl->runImport();
}
