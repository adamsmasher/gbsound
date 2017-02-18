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

#include <cstdio>
#include <fstream>
#include <iostream>
#include <math.h>
#include <strings.h>
#include <sstream>
#include <tuple>

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
  return (150 * speed)/tempo;
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
private:
  std::tuple<Chip, SeqType, int> index;
};

namespace std {
  template <> struct hash<InstrSequenceIndex> {
    size_t operator()(const InstrSequenceIndex&) const;
  };
}

size_t hash_combine(size_t lhs, size_t rhs) {
  lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
  return lhs;
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
private:
  int pitchClass;
  int octave;
};

class ImporterImpl {
public:
  ImporterImpl(const std::string& text) :
    isExpired(false), t(text), track(0), pattern(0), channel(0), row(0), hasN163(false)
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

    return song;
  }

private:
  bool isExpired;
  Tokenizer t;
  unsigned int track;
  unsigned int pattern;
  unsigned int channel;
  unsigned int row;
  bool hasN163;
  std::unordered_map<InstrSequenceIndex, uint8_t> instrSequenceTable;
  Song song;
  
  int getVolId(const std::string& sVol) const {
    const char* VOL_TEXT[17] = {
      "0", "1", "2", "3", "4", "5", "6", "7",
      "8", "9", "A", "B", "C", "D", "E", "F",
      "."
    };

    for (int v = 0; v <= 17; ++v) {
      if (0 == strcasecmp(sVol.c_str(), VOL_TEXT[v])) {
	return v;
      }
    }

    std::stringstream errMsg;
    errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": unrecognized volume token '" << sVol << "'.";
    throw errMsg.str();
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

  int getEffect(const std::string& effectColumn) const {
    char c = toupper(effectColumn.at(0));
    for (int i = 0; i < EF_COUNT; i++) {
      if (EFF_CHAR[i] == c) {
	return i;
      }
    }

    std::ostringstream errMsg;
    errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": unrecognized effect '" << effectColumn << "'.";
    throw errMsg.str();
  }

  void addCell(Note, int instrumentId, int volId, int effect, int effectParam);

  void importCellText(void) {
    Note note = readNote();
    int instrumentId = getInstrumentId(t.readToken());
    int volId = getVolId(t.readToken());

    // only one effect column per channel is allowed
    std::string effectColumn = readEffectColumn();
    int effect = 0;
    int effectParam = 0;
    if (effectColumn != "...") {
      effect = getEffect(effectColumn) + 1;
      effectParam = importHex(effectColumn.substr(effectColumn.size() - 2));
    }

    addCell(note, instrumentId, volId, effect, effectParam);
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

  void importInstrSequence(Chip chip) {
    InstrSequence sequence;
  
    int sequenceType = t.readInt(0, SEQ_COUNT - 1);
    int sequenceNum = t.readInt(0, MAX_SEQUENCES - 1);

    int loopPoint    = t.readInt(-1, MAX_SEQUENCE_ITEMS);
    int releasePoint = t.readInt(-1, MAX_SEQUENCE_ITEMS);

    // TODO: maybe validate the sequence type here - this is only meaningful for arpeggio sequences
    int arpeggioType = t.readInt(0, 255);
    sequence.setArpeggioType(arpeggioType);

    checkColon();

    while (!t.isEOL()) {
      int i = t.readInt(-128, 127);
      sequence.pushBack(i);
    }

    sequence.setLoopPoint(loopPoint);
    sequence.setReleasePoint(releasePoint);

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
    addInstrument(SNDCHIP_NONE, volume, arpeggio, pitch, hiPitch, dutyCycle);
  
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
    
  void importOrder(void) {
    skipFrameNumber();

    int pattern = readPatternNumber();

    // unlike in Famitracker, in our driver there's only one
    // pattern per frame
    for (int c = 1; c < getChannelCount(); ++c) {
      int pattern_ = readPatternNumber();
      if(pattern != pattern_) {
	std::ostringstream errMsg;
	errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": Mismatched pattern number, expected " << pattern << " got " << pattern_;
	throw errMsg.str();
      }
      // TODO: implement
      //pDoc->SetPatternAtFrame(track - 1, ifr, c, i);
    }
    t.readEOL();
  }
    
  void importRow(void) {
    if (track == 0) {
      std::ostringstream errMsg;
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": no TRACK defined, cannot add ROW data.";
      throw errMsg.str();
    }

    int i = t.readHex(0, MAX_PATTERN_LENGTH - 1);
    for (int c = 0; c < getChannelCount(); ++c) {
      checkColon();
      importCellText();
    }
    t.readEOL();
  }

  void importMachine(void) {
    int i = t.readInt(0, PAL);
    if(i == PAL) {
      throw "The Game Boy always runs at 60Hz. PAL songs are not supported.";
    }
    t.readEOL();
  }
    
  // TODO: wait what
  void importVibrato(void) {
    t.readInt(0, VIBRATO_NEW);
    std::cout << "Warning: ignoring vibrato command." << std::endl;
    t.readEOL();
  }
    
  void importSplit(void) {
    int i = t.readInt(0, 255);
    // TODO: what does this do
    //pDoc->SetSpeedSplitPoint(i);
    t.readEOL();
  }
    
  void importFramerate(void) {
    int i = t.readInt(0, 800);
    if(i != 0 || i != 60) {
      throw "Engine speed must be 60Hz.";
    }
    t.readEOL();
  }
    
  void importPattern(void) {
    int i = t.readHex(0, MAX_PATTERN - 1);
    pattern = i;
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
    int i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //          if (pDoc->GetInstrumentType(i) != INST_N163)
    //          {
    //            sResult.Format(_T("Line %d column %d: instrument %d is not defined as an N163 instrument."), t.getLine(), t.GetColumn(), i);
    //            return sResult;
    //          }
    //          CInstrumentN163* pInst = (CInstrumentN163*)pDoc->GetInstrument(i);

    int iw;
    //CHECK(t.readInt(iw, 0, CInstrumentN163::MAX_WAVE_COUNT - 1);
    checkColon();
    //          for (int s=0; s < pInst->GetWaveSize(); ++s)
    //          {
    //            CHECK(t.readInt(i,0,15,&sResult));
    //            pInst->SetSample(iw, s, i);
    //          }
    t.readEOL();
  }

  void importN163Channels(void) {
    int i = t.readInt(1, 8);
    if(i > 1) {
      throw "Only 1 N163 wave channel is supported.";
    }
    t.readEOL();
  }

  void importN163Instrument(void) {
    int i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //CInstrumentN163* pInst = (CInstrumentN163*)pDoc->CreateInstrument(INST_N163);
    //pDoc->AddInstrument(pInst, i);
    for (int s = 0; s < SEQ_COUNT; ++s) {
      i = t.readInt(-1, MAX_SEQUENCES - 1);
      //pInst->SetSeqEnable(s, (i == -1) ? 0 : 1);
      //pInst->SetSeqIndex(s, (i == -1) ? 0 : i);
    }
    //CHECK(t.readInt(i,0,CInstrumentN163::MAX_WAVE_SIZE,&sResult));
    //pInst->SetWaveSize(i);
    i = t.readInt(0, 127);
    //pInst->SetWavePos(i);
    //i = t.readInt(0, CInstrumentN163::MAX_WAVE_COUNT,&sResult));
    //pInst->SetWaveCount(i);
    //pInst->SetName(Charify(t.readToken()));
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
  
  int readPatternNumber(void) {
    return t.readHex(0, MAX_PATTERN - 1);
  }

  void addInstrument(Chip chip, int volumeFt, int arpeggioFt, int pitchFt, int hiPitchFt, int dutyCycleFt) {
    uint8_t volumeGb    = getInstrSequence(InstrSequenceIndex(chip, SEQ_VOLUME,    volumeFt));
    uint8_t arpeggioGb  = getInstrSequence(InstrSequenceIndex(chip, SEQ_ARPEGGIO,  arpeggioFt));
    uint8_t pitchGb     = getInstrSequence(InstrSequenceIndex(chip, SEQ_PITCH,     pitchFt));
    uint8_t hiPitchGb   = getInstrSequence(InstrSequenceIndex(chip, SEQ_HIPITCH,   hiPitchFt));
    uint8_t dutyCycleGb = getInstrSequence(InstrSequenceIndex(chip, SEQ_DUTYCYCLE, dutyCycleFt));
    song.addInstrument(volumeGb, arpeggioGb, pitchGb, hiPitchGb, dutyCycleGb);
  }

  int getChannelCount(void) const {
    if(hasN163) {
      return 5;
    } else {
      return 4;
    }
  }

  void skipFrameNumber(void) {
    t.readHex(0, MAX_FRAMES - 1);
    checkColon();
  }

  void addInstrSequence(InstrSequenceIndex index, const InstrSequence& sequence) {
    uint8_t i = song.addInstrSequence(sequence);
    instrSequenceTable[index] = i;
  }
  
  uint8_t getInstrSequence(InstrSequenceIndex i) const {
    // TODO: handle non existent
    return instrSequenceTable.at(i);
  }
};

void InstrSequence::setLoopPoint(int loopPoint) {
  if(loopPoint >= 0 && (unsigned)loopPoint > sequence.size()) {
    throw "Loop point out of range";
  }
  this->loopPoint = loopPoint;
}

void InstrSequence::setReleasePoint(int releasePoint) {
  if(releasePoint >= 0 && (unsigned)releasePoint > sequence.size()) {
    throw "Release point out of range";
  }
  this->releasePoint = releasePoint;
}

void InstrSequence::pushBack(int i) {
  sequence.push_back(i);
}

Importer::Importer(const std::string& text) : impl(new ImporterImpl(text)) {}

Importer Importer::fromFile(const char *filename) {
  std::ifstream f(filename, std::ifstream::in);
  std::stringstream buffer;
  buffer << f.rdbuf();
  f.close();

  return Importer(buffer.str());
}

Importer::~Importer() {
  delete impl;
}

Song Importer::runImport(void) {
  return impl->runImport();
}
