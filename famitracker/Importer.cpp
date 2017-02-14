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

Command Importer::getCommandEnum(const std::string& command) const {
  std::ostringstream errMsg;
  
  for (int c = 0; c <= CT_LAST; ++c) {
    if (0 == strcasecmp(command.c_str(), CT[c])) {
        return (Command)c;
    }
  }
  
  errMsg << "Unrecognized command at line " << t.getLine() << ": '" << command << "'.";
  throw errMsg.str();
}

const char* HEX_TEXT[16] = {
  "0", "1", "2", "3", "4", "5", "6", "7",
  "8", "9", "A", "B", "C", "D", "E", "F"
};

int Importer::importHex(const std::string& sToken) const {
  std::ostringstream errMsg;

  int i = 0;

  for (size_t d = 0; d < sToken.size(); ++d) {
    i <<= 4;
    std::string token = sToken.substr(d, 1);

    int h = 0;
    for (h = 0; h < 16; ++h) {
      if (0 == strcasecmp(token.c_str(), HEX_TEXT[h])) {
	break;
      }
    }

    if (h >= 16) {
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": hexadecimal number expected, '" << sToken << "' found.";
      throw errMsg.str();
    }

    i += h;
  }

  return i;
}

const char* VOL_TEXT[17] = {
  "0", "1", "2", "3", "4", "5", "6", "7",
  "8", "9", "A", "B", "C", "D", "E", "F",
  "."
};

int Importer::getVolId(const std::string& sVol) const {
  std::stringstream errMsg;
  
  int v;  
  for (v = 0; v <= 17; ++v) {
    if (0 == strcasecmp(sVol.c_str(), VOL_TEXT[v])) {
      break;
    }
  }
  
  if (v > 17) {
    errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": unrecognized volume token '" << sVol << "'.";
    throw errMsg.str();
  }

  return v;
}

int Importer::getInstrumentId(const std::string& sInst) const {
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

std::pair<int, int> Importer::getNoteAndOctave(const std::string& sNote) const {
  std::ostringstream errMsg;
  
  if (sNote.size() != 3) {
    errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": note column should be 3 characters wide, '" << sNote << "' found.";
    throw errMsg.str();
  } else if (sNote == "...") {
    return {0, 0};
  } else if (sNote == "---") {
    return {HALT, 0};
  } else if (sNote == "===") {
    return {RELEASE, 0};
  } else if (channel == 3) { // noise
    int h = importHex(sNote.substr(0, 1));

    // importer is very tolerant about the second and third characters
    // in a noise note, they can be anything

    return {h % 12 + 1, h / 12};
  } else {
    int n = 0;
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

    return { n + 1, o };
  }

  throw "unreachable";
}

void Importer::importCellText(void) {
  // stChanNote Cell;
  // empty Cell
  // ::memset(&Cell, 0, sizeof(Cell));

  std::string sNote = t.readToken();
  // Cell.Note, Cell.Octave = getNoteAndOctave(sNote);

  std::string sInst = t.readToken();
  // Cell.Instrument = getInstrumentId(sInst);

  std::string sVol = t.readToken();
  //Cell.Vol = getVolId(sVol);

//  for (unsigned int e = 0; e <= pDoc->GetEffColumns(track, channel); ++e) {
//    std::string sEff = t.readToken();
//    if (sEff != "...") {
//      if (sEff.size() != 3) {
//        errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": effect column should be 3 characters wide, '" << sEff << "' found.";
//        goto error;
//      }
//
//      int p = 0;
//      char pC = sEff.at(0);
//      if (pC >= 'a' && pC <= 'z') pC += 'A' - 'a';
//      for (; p < EF_COUNT; ++p)
//        if (EFF_CHAR[p] == pC) break;
//      if (p >= EF_COUNT) {
//        errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": unrecognized effect '" << sEff << "'.";
//        goto error;
//      }
//      // Cell.EffNumber[e] = p+1;
//
//      int h;
//      bool err;
//      std::string importHexErr = importHex(sEff.substr(sEff.size() - 2), &h, t.getLine(), t.getColumn(), &err);
//      if (err) {
//        errMsg << importHexErr;
//        goto error;
//      }
//      //Cell.EffParam[e] = h;
//    }
//  }

  //pDoc->SetDataAtPattern(track,pattern,channel,row,&Cell);
}

Importer::Importer(const std::string& text)
  : t(text)
{}

Importer Importer::fromFile(const char *filename) {
  // read file into "text"
  std::ifstream f(filename, std::ifstream::in);
  std::stringstream buffer;
  buffer << f.rdbuf();
  f.close();

  return Importer(buffer.str());
}

Importer::~Importer()
{}

void Importer::checkSymbol(const std::string& x) {
  std::ostringstream errMsg;
  
  std::string symbol = t.readToken();
  if (symbol != x) {
    errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": expected '" << x << "', '" << symbol << "' found.";
    throw errMsg.str();
  }
}

void Importer::checkColon(void) {
  checkSymbol(":");
}

void Importer::importMacro(int chip) {
  std::ostringstream errMsg;

  Sequence sequence;
  
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

  addSequence(std::make_tuple(chip, sequenceType, sequenceNum), sequence);
}

void Importer::importStandardMacro(void) {
  importMacro(SNDCHIP_NONE);
}

void Importer::skipInstrumentName(void) {
  t.readToken();
}

void Importer::skipInstrumentNumber(void) {
  t.readInt(0, MAX_INSTRUMENTS - 1);
}

int Importer::readSequenceNumber(void) {
  return t.readInt(-1, MAX_SEQUENCES - 1);
}

void Importer::importStandardInstrument(void) {
  skipInstrumentNumber();
  
  int volume = readSequenceNumber();
  int arpeggio = readSequenceNumber();
  int pitch = readSequenceNumber();
  int hiPitch = readSequenceNumber();
  int dutyCycle = readSequenceNumber();
  song.addInstrument(buildInstrument(volume, arpeggio, pitch, hiPitch, dutyCycle));
  
  skipInstrumentName();
  t.readEOL();
}

void Importer::skipTrackTitle(void) {
  t.readToken();
}

void Importer::importTrack(void) {
  if (track != 0) {
    throw "Multiple tracks not supported";
  }

  int patternLength = t.readInt(0, MAX_PATTERN_LENGTH);
  setPatternLength(patternLength);
    
  int speed = t.readInt(0, MAX_TEMPO);
  int tempo = t.readInt(0, MAX_TEMPO);
  song.setTempo(computeTempo(speed, tempo));

  skipTrackTitle();
  t.readEOL();
  ++track;
}

void Importer::importColumns(void) {
  checkColon();
  for (int c = 0; c < getChannelCount(); ++c) {
    int i = t.readInt(1, MAX_EFFECT_COLUMNS);
    // TODO: what does this do?
    //pDoc->SetEffColumns(track-1,c,i-1);
  }
  t.readEOL();
}

int Importer::readPatternNumber(void) {
  return t.readHex(0, MAX_PATTERN - 1);
}

void Importer::skipFrameNumber(void) {
  t.readHex(0, MAX_FRAMES - 1);
  checkColon();
}

void Importer::importOrder(void) {
  skipFrameNumber();

  int pattern = readPatternNumber();

  // unlike in Famitracker, in our driver there's only one
  // pattern per frame
  for (int c = 1; c < getChannelCount(); ++c) {
    int pattern_ = readPatternNumber();
    if(pattern != pattern_) {
      // TODO: better error
      throw "Mismatched pattern number";
    }
    // TODO: implement
    //pDoc->SetPatternAtFrame(track - 1, ifr, c, i);
  }
  t.readEOL();
}

void Importer::importRow(void) {
  std::ostringstream errMsg;
  
  if (track == 0) {
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

void Importer::importMachine(void) {
  int i = t.readInt(0, PAL);
  if(i == PAL) {
    throw "The Game Boy always runs at 60Hz. PAL songs are not supported.";
  }
  t.readEOL();
}

void Importer::importVibrato(void) {
  t.readInt(0, VIBRATO_NEW);
  std::cout << "Warning: ignoring vibrato command." << std::endl;
  t.readEOL();
}

void Importer::importSplit(void) {
  int i = t.readInt(0, 255);
  //pDoc->SetSpeedSplitPoint(i);
  t.readEOL();
}

void Importer::importFramerate(void) {
  int i = t.readInt(0, 800);
  if(i != 0 || i != 60) {
    throw "Engine speed must be 60Hz.";
  }
  t.readEOL();
}

void Importer::importPattern(void) {
  int i = t.readHex(0, MAX_PATTERN - 1);
  pattern = i;
  t.readEOL();
}

void Importer::importExpansion(void) {
  int i = t.readInt(0, 255);
  if(i != SNDCHIP_NONE && i != SNDCHIP_N163) {
    throw "Unsupported expansion.";
  }
  t.readEOL();
}

void Importer::importN163Macro(void) {
  importMacro(SNDCHIP_N163);
}

void Importer::importN163Instrument(void) {
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

void Importer::importN163Wave(void) {
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

void Importer::importN163Channels(void) {
  int i = t.readInt(1, 8);
  if(i > 1) {
    throw "Only 1 N163 wave channel is supported.";
  }
  t.readEOL();
}

void Importer::importCommand(Command c) {
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
    importStandardMacro();
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
    importN163Macro();
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

Song Importer::runImport(void) {
  /*// begin a new document
  if (!pDoc->OnNewDocument())
  {
    sResult = _T("Unable to create new Famitracker document.");
    return sResult;
  }*/

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

uint8_t Importer::computeTempo(int speed, int tempo) {
  double bpm = (tempo * 6.0)/speed;
  double rowsPerMinute = bpm * 4;
  double rowsPerSecond = rowsPerMinute/60;
  double ticksPerRow = 60/rowsPerSecond;
  return (uint8_t)ceil(256/ticksPerRow);
}

void Sequence::setLoopPoint(int loopPoint) {
  if(loopPoint >= 0 && (unsigned)loopPoint > sequence.size()) {
    throw "Loop point out of range";
  }
  this->loopPoint = loopPoint;
}

void Sequence::setReleasePoint(int releasePoint) {
  if(releasePoint >= 0 && (unsigned)releasePoint > sequence.size()) {
    throw "Release point out of range";
  }
  this->releasePoint = releasePoint;
}

void Sequence::pushBack(int i) {
  sequence.push_back(i);
}
