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
#include <strings.h>
#include <sstream>

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
  std::ostringstream errMsg;

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

// =============================================================================

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

// =============================================================================

#define CHECK_SYMBOL(x) \
  { \
    std::string symbol_ = t.readToken(); \
    if (symbol_ != (x)) \
    { \
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": expected '" << (x) << "', '" << symbol_ << "' found."; \
      throw errMsg.str();						\
    } \
  }

#define CHECK_COLON() CHECK_SYMBOL(":")

void Importer::importCommand(Command c) {
  std::ostringstream errMsg;

  int i; // generic integer for reading
  unsigned int dpcm_index = 0;
  unsigned int dpcm_pos = 0;
  
  switch (c) {
  case CT_COMMENTLINE:
    t.finishLine();
    break;
  case CT_TITLE:
    //pDoc->SetSongName(Charify(t.readToken()));
    t.readEOL();
    break;
  case CT_AUTHOR:
    //pDoc->SetSongArtist(Charify(t.readToken()));
    t.readEOL();
    break;
  case CT_COPYRIGHT:
    //pDoc->SetSongCopyright(Charify(t.readToken()));
    t.readEOL();
    break;
  case CT_COMMENT:
    /*CString sComment = pDoc->GetComment();
      if (sComment.GetLength() > 0)
      sComment = sComment + _T("\r\n");
      sComment += t.readToken();
      pDoc->SetComment(sComment, pDoc->ShowCommentOnOpen()); */
    t.readEOL();
    break;
  case CT_MACHINE:
    i = t.readInt(0, PAL);
    //pDoc->SetMachine(i);
    t.readEOL();
    break;
  case CT_FRAMERATE:
    i = t.readInt(0, 800);
    //pDoc->SetEngineSpeed(i);
    t.readEOL();
    break;
  case CT_EXPANSION:
    i = t.readInt(0, 255);
    //pDoc->SelectExpansionChip(i);
    t.readEOL();
    break;
  case CT_VIBRATO:
    i = t.readInt(0, VIBRATO_NEW);
    //pDoc->SetVibratoStyle((vibrato_t)i);
    t.readEOL();
    break;
  case CT_SPLIT:
    i = t.readInt(0, 255);
    //pDoc->SetSpeedSplitPoint(i);
    t.readEOL();
    break;
  case CT_N163CHANNELS:
    i = t.readInt(1, 8);
    //pDoc->SetNamcoChannels(i);
    //pDoc->SelectExpansionChip(pDoc->GetExpansionChip());
    t.readEOL();
    break;
  case CT_MACRO:
  case CT_MACROVRC6:
  case CT_MACRON163:
  case CT_MACROS5B: {
    const int CHIP_MACRO[4] = { SNDCHIP_NONE, SNDCHIP_VRC6, SNDCHIP_N163, SNDCHIP_S5B };
    int chip = c - CT_MACRO;

    int mt = t.readInt(0, SEQ_COUNT - 1);
    i = t.readInt(0, MAX_SEQUENCES - 1);
    //CSequence* pSeq = pDoc->GetSequence(CHIP_MACRO[chip], i, mt);

    i = t.readInt(-1, MAX_SEQUENCE_ITEMS);
    //pSeq->SetLoopPoint(i);
    i = t.readInt(-1, MAX_SEQUENCE_ITEMS);
    //pSeq->SetReleasePoint(i);
    i = t.readInt(0, 255);
    //pSeq->SetSetting(i);

    CHECK_COLON();

    int count = 0;
    while (!t.isEOL()) {
      i = t.readInt(-128, 127);
      if (count >= MAX_SEQUENCE_ITEMS) {
	errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": macro overflow, max size: " << MAX_SEQUENCE_ITEMS << ".";
	throw errMsg.str();
      }
      //pSeq->SetItem(count, i);
      ++count;
    }
    //pSeq->SetItemCount(count);
  }
    break;
  case CT_DPCMDEF: {
    i = t.readInt(0, MAX_DSAMPLES - 1);
    dpcm_index = i;
    dpcm_pos = 0;

    //CHECK(t.readInt(i,0,CDSample::MAX_SIZE,&err));
    //CDSample* pSample = pDoc->GetSample(dpcm_index);
    //pSample->Allocate(i, NULL);
    //::memset(pSample->GetData(), 0, i);
    //pSample->SetName(Charify(t.readToken()));

    t.readEOL();
  }
    break;
  case CT_DPCM: {
    //CDSample* pSample = pDoc->GetSample(dpcm_index);
    CHECK_COLON();
    while (!t.isEOL()) {
      i = t.readHex(0x00, 0xFF);
      //            if (dpcm_pos >= pSample->GetSize())
      //            {
      //              sResult.Format(_T("Line %d column %d: DPCM sample %d overflow, increase size used in %s."), t.getLine(), t.GetColumn(), dpcm_index, CT[CT_DPCMDEF]);
      //              return sResult;
      //            }
      //            *(pSample->GetData() + dpcm_pos) = (char)(i);
      ++dpcm_pos;
    }
  }
    break;
  case CT_INST2A03: {
    i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //CInstrument2A03* pInst = (CInstrument2A03*)pDoc->CreateInstrument(INST_2A03);
    //pDoc->AddInstrument(pInst, i);
    for (int s = 0; s < SEQ_COUNT; ++s) {
      i = t.readInt(-1, MAX_SEQUENCES - 1);
      //pInst->SetSeqEnable(s, (i == -1) ? 0 : 1);
      //pInst->SetSeqIndex(s, (i == -1) ? 0 : i);
    }
    //pInst->SetName(Charify(t.readToken()));
    t.readEOL();
  }
    break;
  case CT_INSTVRC6: {
    i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //CInstrumentVRC6* pInst = (CInstrumentVRC6*)pDoc->CreateInstrument(INST_VRC6);
    //pDoc->AddInstrument(pInst, i);
    for (int s = 0; s < SEQ_COUNT; ++s) {
      i = t.readInt(-1, MAX_SEQUENCES - 1);
      //            pInst->SetSeqEnable(s, (i == -1) ? 0 : 1);
      //            pInst->SetSeqIndex(s, (i == -1) ? 0 : i);
    }
    //          pInst->SetName(Charify(t.readToken()));
    t.readEOL();
  }
    break;
  case CT_INSTVRC7: {
    i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //CInstrumentVRC7* pInst = (CInstrumentVRC7*)pDoc->CreateInstrument(INST_VRC7);
    //pDoc->AddInstrument(pInst, i);
    i = t.readInt(0, 15);
    //pInst->SetPatch(i);
    for (int r = 0; r < 8; ++r) {
      i = t.readHex(0x00, 0xFF);
      //pInst->SetCustomReg(r, i);
    }
    //pInst->SetName(Charify(t.readToken()));
    t.readEOL();
  }
    break;
  case CT_INSTFDS: {
    i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //CInstrumentFDS* pInst = (CInstrumentFDS*)pDoc->CreateInstrument(INST_FDS);
    //pDoc->AddInstrument(pInst, i);
    i = t.readInt(0, 1);
    //pInst->SetModulationEnable(i==1);
    i = t.readInt(0, 4095);
    //pInst->SetModulationSpeed(i);
    i = t.readInt(0, 63);
    //pInst->SetModulationDepth(i);
    i = t.readInt(0, 255);
    //pInst->SetModulationDelay(i);
    //pInst->SetName(Charify(t.readToken()));
    t.readEOL();
  }
    break;
  case CT_INSTN163: {
    i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //CInstrumentN163* pInst = (CInstrumentN163*)pDoc->CreateInstrument(INST_N163);
    //pDoc->AddInstrument(pInst, i);
    for (int s=0; s < SEQ_COUNT; ++s) {
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
    break;
  case CT_INSTS5B: {
    i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //CInstrumentS5B* pInst = (CInstrumentS5B*)pDoc->CreateInstrument(INST_S5B);
    //pDoc->AddInstrument(pInst, i);
    for (int s=0; s < SEQ_COUNT; ++s) {
      i = t.readInt(-1, MAX_SEQUENCES - 1);
      //pInst->SetSeqEnable(s, (i == -1) ? 0 : 1);
      //pInst->SetSeqIndex(s, (i == -1) ? 0 : i);
    }
    //pInst->SetName(Charify(t.readToken()));
    t.readEOL();
  }
    break;
  case CT_KEYDPCM: {
    i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //          if (pDoc->GetInstrumentType(i) != INST_2A03) {
    //            sResult.Format(_T("Line %d column %d: instrument %d is not defined as a 2A03 instrument."), t.getLine(), t.GetColumn(), i);
    //            return sResult;
    //          }
    //          CInstrument2A03* pInst = (CInstrument2A03*)pDoc->GetInstrument(i);

    int io = t.readInt(0, OCTAVE_RANGE);
    int in = t.readInt(0, 12);

    i = t.readInt(0, MAX_DSAMPLES - 1);
    //pInst->SetSample(io, in, i + 1);
    i = t.readInt(0, 15);
    //pInst->SetSamplePitch(io, in, i);
    i = t.readInt(0, 1);
    //pInst->SetSampleLoop(io, in, i==1);
    i = t.readInt(0, 255);
    //pInst->SetSampleLoopOffset(io, in, i);
    i = t.readInt(-1, 127);
    //pInst->SetSampleDeltaValue(io, in, i);

    t.readEOL();
  }
    break;
  case CT_FDSWAVE: {
    i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //          if (pDoc->GetInstrumentType(i) != INST_FDS)
    //          {
    //            sResult.Format(_T("Line %d column %d: instrument %d is not defined as an FDS instrument."), t.getLine(), t.GetColumn(), i);
    //            return sResult;
    //          }
    //          CInstrumentFDS* pInst = (CInstrumentFDS*)pDoc->GetInstrument(i);
    CHECK_COLON();
    //          for (int s = 0; s < CInstrumentFDS::WAVE_SIZE; ++s)
    //          {
    //            CHECK(t.readInt(i,0,63,&sResult));
    //            pInst->SetSample(s, i);
    //          }
    t.readEOL();
  }
    break;
  case CT_FDSMOD: {
    i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //          if (pDoc->GetInstrumentType(i) != INST_FDS)
    //          {
    //            sResult.Format(_T("Line %d column %d: instrument %d is not defined as an FDS instrument."), t.getLine(), t.GetColumn(), i);
    //            return sResult;
    //          }
    //          CInstrumentFDS* pInst = (CInstrumentFDS*)pDoc->GetInstrument(i);
    CHECK_COLON();
    //          for (int s = 0; s < CInstrumentFDS::MOD_SIZE; ++s)
    //          {
    //            CHECK(t.readInt(i,0,7,&sResult));
    //            pInst->SetModulation(s, i);
    //          }
    t.readEOL();
  }
    break;
  case CT_FDSMACRO: {
    i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //          if (pDoc->GetInstrumentType(i) != INST_FDS)
    //          {
    //            sResult.Format(_T("Line %d column %d: instrument %d is not defined as an FDS instrument."), t.getLine(), t.GetColumn(), i);
    //            return sResult;
    //          }
    //          CInstrumentFDS* pInst = (CInstrumentFDS*)pDoc->GetInstrument(i);

    i = t.readInt(0, 2);
    //CSequence * pSeq = NULL;
    switch(i) {
    case 0:
      //pSeq = pInst->GetVolumeSeq();
      break;
    case 1:
      //pSeq = pInst->GetArpSeq();
      break;
    case 2:
      //pSeq = pInst->GetPitchSeq();
      break;
    default:
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": unexpected error.";
      throw errMsg.str();
    }
    i = t.readInt(-1, MAX_SEQUENCE_ITEMS);
    //pSeq->SetLoopPoint(i);
    i = t.readInt(-1, MAX_SEQUENCE_ITEMS);
    //pSeq->SetReleasePoint(i);
    i = t.readInt(0, 255);
    //pSeq->SetSetting(i);

    CHECK_COLON();

    int count = 0;
    while (!t.isEOL()) {
      i = t.readInt(-128, 127);
      if (count >= MAX_SEQUENCE_ITEMS) {
	errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": macro overflow, max size: " << MAX_SEQUENCE_ITEMS << ".";
	throw errMsg.str();
      }
      //pSeq->SetItem(count, i);
      ++count;
    }
    //pSeq->SetItemCount(count);
  }
    break;
  case CT_N163WAVE: {
    i = t.readInt(0, MAX_INSTRUMENTS - 1);
    //          if (pDoc->GetInstrumentType(i) != INST_N163)
    //          {
    //            sResult.Format(_T("Line %d column %d: instrument %d is not defined as an N163 instrument."), t.getLine(), t.GetColumn(), i);
    //            return sResult;
    //          }
    //          CInstrumentN163* pInst = (CInstrumentN163*)pDoc->GetInstrument(i);

    int iw;
    //CHECK(t.readInt(iw, 0, CInstrumentN163::MAX_WAVE_COUNT - 1);
    CHECK_COLON();
    //          for (int s=0; s < pInst->GetWaveSize(); ++s)
    //          {
    //            CHECK(t.readInt(i,0,15,&sResult));
    //            pInst->SetSample(iw, s, i);
    //          }
    t.readEOL();
  }
    break;
  case CT_TRACK: {
    if (track != 0) {
      //            if(pDoc->AddTrack() == -1) {
      //              sResult.Format(_T("Line %d column %d: unable to add new track."), t.getLine(), t.GetColumn());
      //              return sResult;
      //            }
    }

    i = t.readInt(0, MAX_PATTERN_LENGTH);
    //pDoc->SetPatternLength(track, i);
    i = t.readInt(0, MAX_TEMPO);
    //pDoc->SetSongSpeed(track, i);
    i = t.readInt(0, MAX_TEMPO);
    //pDoc->SetSongTempo(track, i);
    //pDoc->SetTrackTitle(track, t.readToken());

    t.readEOL();
    ++track;
  }
    break;
  case CT_COLUMNS: {
    CHECK_COLON();
    //          for (int c = 0; c < pDoc->GetChannelCount(); ++c)
    //          {
    //            CHECK(t.readInt(i,1,MAX_EFFECT_COLUMNS,&sResult));
    //            pDoc->SetEffColumns(track-1,c,i-1);
    //          }
    t.readEOL();
  }
    break;
  case CT_ORDER: {
    int ifr = t.readHex(0, MAX_FRAMES - 1);
    //          if (ifr >= (int)pDoc->GetFrameCount(track-1)) // expand to accept frames
    //          {
    //            pDoc->SetFrameCount(track-1,ifr+1);
    //          }
    CHECK_COLON();
    //          for (int c = 0; c < pDoc->GetChannelCount(); ++c)
    //          {
    //            i = t.readHex(0, MAX_PATTERN - 1, &sResult));
    //            pDoc->SetPatternAtFrame(track - 1, ifr, c, i);
    //          }
    t.readEOL();
  }
    break;
  case CT_PATTERN:
    i = t.readHex(0, MAX_PATTERN - 1);
    pattern = i;
    t.readEOL();
    break;
  case CT_ROW: {
    if (track == 0) {
      errMsg << "Line " << t.getLine() << " column " << t.getColumn() << ": no TRACK defined, cannot add ROW data.";
      throw errMsg.str();
    }

    i = t.readHex(0, MAX_PATTERN_LENGTH - 1);
    //          for (int c=0; c < pDoc->GetChannelCount(); ++c) {
    //            CHECK_COLON();
    //            if (!ImportCellText(pDoc, t, track-1, pattern, c, i, sResult)) {
    //              return sResult;
    //            }
    //          }
    t.readEOL();
  }
    break;
  }
}

void Importer::runImport(void) {
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
}
