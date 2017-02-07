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

#include "TextExporter.h"

#include <cstdio>
#include <fstream>
#include <strings.h>
#include <sstream>

#define DEBUG_OUT(...) { CString s__; s__.Format(__VA_ARGS__); OutputDebugString(s__); }

// command tokens
enum {
  CT_COMMENTLINE,    // anything may follow
  // song info
  CT_TITLE,          // string
  CT_AUTHOR,         // string
  CT_COPYRIGHT,      // string
  CT_COMMENT,        // string (concatenates line)
  // global settings
  CT_MACHINE,        // uint (0=NTSC, 1=PAL)
  CT_FRAMERATE,      // uint (0=default)
  CT_EXPANSION,      // uint (0=none, 1=VRC6, 2=VRC7, 4=FDS, 8=MMC5, 16=N163, 32=S5B)
  CT_VIBRATO,        // uint (0=old, 1=new)
  CT_SPLIT,          // uint (32=default)
  // namco global settings
  CT_N163CHANNELS,   // uint
  // macros
  CT_MACRO,          // uint (type) uint (index) int (loop) int (release) int (setting) : int_list
  CT_MACROVRC6,      // uint (type) uint (index) int (loop) int (release) int (setting) : int_list
  CT_MACRON163,      // uint (type) uint (index) int (loop) int (release) int (setting) : int_list
  CT_MACROS5B,       // uint (type) uint (index) int (loop) int (release) int (setting) : int_list
  // dpcm samples
  CT_DPCMDEF,        // uint (index) uint (size) string (name)
  CT_DPCM,           // : hex_list
  // instruments
  CT_INST2A03,       // uint (index) int int int int int string (name)
  CT_INSTVRC6,       // uint (index) int int int int int string (name)
  CT_INSTVRC7,       // uint (index) int (patch) hex hex hex hex hex hex hex hex string (name)
  CT_INSTFDS,        // uint (index) int (mod enable) int (m speed) int (m depth) int (m delay) string (name)
  CT_INSTN163,       // uint (index) int int int int int uint (w size) uint (w pos) uint (w count) string (name)
  CT_INSTS5B,        // uint (index) int int int int int  string (name)
  // instrument data
  CT_KEYDPCM,        // uint (inst) uint (oct) uint (note) uint (sample) uint (pitch) uint (loop) uint (loop_point)
  CT_FDSWAVE,        // uint (inst) : uint_list x 64
  CT_FDSMOD,         // uint (inst) : uint_list x 32
  CT_FDSMACRO,       // uint (inst) uint (type) int (loop) int (release) int (setting) : int_list
  CT_N163WAVE,       // uint (inst) uint (wave) : uint_list
  // track info
  CT_TRACK,          // uint (pat length) uint (speed) uint (tempo) string (name)
  CT_COLUMNS,        // : uint_list (effect columns)
  CT_ORDER,          // hex (frame) : hex_list
  // pattern data
  CT_PATTERN,        // hex (pattern)
  CT_ROW,            // row data
  // end of command list
  CT_COUNT
};

static const char* CT[CT_COUNT] = {
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

// =============================================================================

class Tokenizer
{
public:
  Tokenizer(const std::string *text_)
    : text(text_), pos(0), line(1), linestart(0)
  {}

  ~Tokenizer() {}

  void reset() {
    pos = 0;
    line = 1;
    linestart = 0;
  }

  void consumeSpace() {
    if (!text) return;

    while (pos < text->size()) {
      char c = text->at(pos);
      if (c != ' ' && c != '\t') {
        return;
      }
      ++pos;
    }
  }

  void finishLine() {
    if (!text) return;

    while (pos < text->size() && text->at(pos) != '\n') {
      ++pos;
    }

    if (pos < text->size()) ++pos; // skip newline
    ++line;
    linestart = pos;
  }

  int getColumn() const {
    return 1 + pos - linestart;
  }

  bool finished() const {
    if (!text) return true;
    else return pos >= text->size();
  }

  std::string readToken() {
    if (!text) return "";

    consumeSpace();
    std::string t = "";

    bool inQuote = false;
    bool lastQuote = false; // for finding double-quotes
    do {
      if (pos >= text->size()) break;

      char c = text->at(pos);
      if ((c == ' ' && !inQuote) ||
        c == '\t' ||
        c == '\r' ||
        c == '\n')
      {
        break;
      }

      // quotes suppress space ending the token
      if (c == '\"') {
        if (!inQuote && t.size() == 0) { // first quote begins a quoted string
          inQuote = true;
        }
        else {
          if (lastQuote) { // convert "" to "
            t += c;
            lastQuote = false;
          }
          else {
            lastQuote = true;
          }
        }
      }
      else {
        lastQuote = false;
        t += c;
      }

      ++pos;
    }
    while (true);

    //DEBUG_OUT("readToken(%d,%d): '%s'\n", line, GetColumn(), t);
    return t;
  }

  // TODO: make me use std::optional
  std::string readInt(int& i, int range_min, int range_max, bool *err) {
    std::ostringstream errMsg;
    int result;

    std::string t = readToken();
    int c = getColumn();

    if (t.size() < 1) {
      errMsg << "Line " << line << " column " << c << ": expected integer, no token found.";
      goto error;
    }

    result = ::sscanf(t.c_str(), "%d", &i);
    if(result == EOF || result == 0) {
      errMsg << "Line " << line << " column " << c << ": expected integer, '" << t << "' found.";
      goto error;
    }

    if (i < range_min || i > range_max) {
      errMsg << "Line " << line << " column " << c << ": expected integer in range [" << range_min << "," << range_max << "], " << i << " found.";
      goto error;
    }

    *err = false;
    return {};
  error:
    *err = true;
    return errMsg.str();
  }

  // TODO: make me use std::optional
  std::string readHex(int& i, int range_min, int range_max, bool *err) {
    std::ostringstream errMsg;
    int result;

    std::string t = readToken();
    int c = getColumn();

    if (t.size() < 1) {
      errMsg << "Line " << line << " column " << c << ": expected hexadecimal, no token found.";
      goto error;
    }

    result = ::sscanf(t.c_str(), "%x", &i);
    if(result == EOF || result == 0) {
      errMsg << "Line " << line << " column " << c << ": expected hexadecimal, '" << t << "' found.";
      goto error;
    }

    if (i < range_min || i > range_max) {
      errMsg << "Line " << line << " column " << c << ": expected hexadecimal in range [" << range_min << "," << range_max << "], " << i << " found.";
      goto error;
    }
   
    *err = false;
    return {};
  error:
    *err = true;
    return errMsg.str();
  }

  // TODO: make me use std::optional
  // note: finishes line if found
  std::string readEOL(bool *err) {
    std::ostringstream errMsg;
    char eol;

    int c = getColumn();
    consumeSpace();

    std::string s = readToken();
    if (s.size() > 0) {
      errMsg << "Line " << line << " column " << c << ": expected end of line, '" << s << "' found.";
      goto error;
    }

    if (finished()) return {};

    eol = text->at(pos);
    if (eol != '\r' && eol != '\n') {
      errMsg << "Line " << line << " column " << c << ": expected end of line, '" << eol << "' found.";
      goto error;
    }

    finishLine();

    *err = false;
    return {};
  error:
    *err = true;
    return errMsg.str();
  }

  // note: finishes line if found
  bool isEOL() {
    consumeSpace();
    if (finished()) return true;

    char eol = text->at(pos);
    if (eol == '\r' || eol == '\n') {
      finishLine();
      return true;
    }

    return false;
  }

  const std::string *text;
  size_t pos;
  int line;
  int linestart;
};

// =============================================================================

// TODO: make me use std optional
static std::string importHex(const std::string& sToken, int *i, int line, int column, bool *err) {
  std::ostringstream errMsg;

  *i = 0;

  for (size_t d = 0; d < sToken.size(); ++d) {
    const char* HEX_TEXT[16] = {
      "0", "1", "2", "3", "4", "5", "6", "7",
      "8", "9", "A", "B", "C", "D", "E", "F"
    };

    *i <<= 4;
    std::string t = sToken.substr(d, 1);

    int h = 0;
    for (h = 0; h < 16; ++h) {
      if (0 == strcasecmp(t.c_str(), HEX_TEXT[h])) break;
    }

    if (h >= 16) {
      errMsg << "Line " << line << " column " << column << ": hexadecimal number expected, '" << sToken << "' found.";
      goto error;
    }

    *i += h;
  }

  *err = false;
  return {};
error:
  *err = true;
  return errMsg.str();
}

// =============================================================================

const char* VOL_TEXT[17] = {
  "0", "1", "2", "3", "4", "5", "6", "7",
  "8", "9", "A", "B", "C", "D", "E", "F",
  "."
};
 
static std::string ImportCellText(
  Tokenizer &t,
  unsigned int track,
  unsigned int pattern,
  unsigned int channel,
  unsigned int row,
  bool *err)
{
  std::ostringstream errMsg;
  int v;
  std::string sVol;
  std::string sInst;
  // stChanNote Cell;

  // empty Cell
  // ::memset(&Cell, 0, sizeof(Cell));
  // Cell.Instrument = MAX_INSTRUMENTS;
  // Cell.Vol = 0x10;

  std::string sNote = t.readToken();
  if      (sNote == "...") { /*Cell.Note = 0;*/ }
  else if (sNote == "---") { /* Cell.Note = HALT; */ }
  else if (sNote == "===") { /* Cell.Note = RELEASE; */ }
  else {
    if (sNote.size() != 3) {
      errMsg << "Line " << t.line << " column " << t.getColumn() << ": note column should be 3 characters wide, '" << sNote << "' found.";
      goto error;
    }

    if (channel == 3) { // noise
      int h;
      bool err;
      std::string importHexErr = importHex(sNote.substr(0, 1), &h, t.line, t.getColumn(), &err);
      if(err) {
        errMsg << importHexErr;
        goto error;
      }
      //Cell.Note = (h % 12) + 1;
      //Cell.Octave = h / 12;

      // importer is very tolerant about the second and third characters
      // in a noise note, they can be anything
    }
    else {
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
          errMsg << "Line " << t.line << " column " << t.getColumn() << ": unrecognized note '" << sNote << "'.";
          goto error;
      }
      switch (sNote.at(1)) {
        case '-': case '.': break;
        case '#': case '+': n += 1; break;
        case 'b': case 'f': n -= 1; break;
        default:
          errMsg << "Line " << t.line << " column " << t.getColumn() << ": unrecognized note '" << sNote << "'.";
          goto error;
      }
      while (n <   0) n += 12;
      while (n >= 12) n -= 12;
      //Cell.Note = n + 1;

      int o = sNote.at(2) - '0';
      if (o < 0 || o >= OCTAVE_RANGE) {
        errMsg << "Line " << t.line << " column " << t.getColumn() << ": unrecognized octave '" << sNote << "'.";
        goto error;
      }
      //Cell.Octave = o;
    }
  }

  sInst = t.readToken();
  if (sInst == "..") { /*Cell.Instrument = MAX_INSTRUMENTS;*/ }
  else {
    if (sInst.size() != 2) {
      errMsg << "Line " << t.line << " column " << t.getColumn() << ": instrument column should be 2 characters wide, '" << sInst << "' found.";
      goto error;
    }

    int h;
    bool err;
    std::string importHexErr = importHex(sInst, &h, t.line, t.getColumn(), &err);
    if (err) {
      errMsg << importHexErr;
      goto error;
    }

    if (h >= MAX_INSTRUMENTS) {
      errMsg << "Line " << t.line << " column " << ": instrument '" << sInst << "' is out of bounds.";
      goto error;
    }
    //Cell.Instrument = h;
  }

  sVol  = t.readToken();
  v = 0;
  for (; v <= 17; ++v)
    if (0 == strcasecmp(sVol.c_str(), VOL_TEXT[v])) break;
  if (v > 17) {
    errMsg << "Line " << t.line << " column " << t.getColumn() << ": unrecognized volume token '" << sVol << "'.";
    goto error;
  }
  //Cell.Vol = v;

//  for (unsigned int e = 0; e <= pDoc->GetEffColumns(track, channel); ++e) {
//    std::string sEff = t.readToken();
//    if (sEff != "...") {
//      if (sEff.size() != 3) {
//        errMsg << "Line " << t.line << " column " << t.getColumn() << ": effect column should be 3 characters wide, '" << sEff << "' found.";
//        goto error;
//      }
//
//      int p = 0;
//      char pC = sEff.at(0);
//      if (pC >= 'a' && pC <= 'z') pC += 'A' - 'a';
//      for (; p < EF_COUNT; ++p)
//        if (EFF_CHAR[p] == pC) break;
//      if (p >= EF_COUNT) {
//        errMsg << "Line " << t.line << " column " << t.getColumn() << ": unrecognized effect '" << sEff << "'.";
//        goto error;
//      }
//      // Cell.EffNumber[e] = p+1;
//
//      int h;
//      bool err;
//      std::string importHexErr = importHex(sEff.substr(sEff.size() - 2), &h, t.line, t.getColumn(), &err);
//      if (err) {
//        errMsg << importHexErr;
//        goto error;
//      }
//      //Cell.EffParam[e] = h;
//    }
//  }

  //pDoc->SetDataAtPattern(track,pattern,channel,row,&Cell);
  *err = false;
  return {};
error:
  *err = true;
  return errMsg.str();
}

// =============================================================================

CTextExport::CTextExport()
{
}

CTextExport::~CTextExport()
{
}

// =============================================================================

#define CHECK(x) { bool err; std::string errStr = (x); if (!(err)) { errMsg << errStr; goto error; } }

#define CHECK_SYMBOL(x) \
  { \
    std::string symbol_ = t.readToken(); \
    if (symbol_ != (x)) \
    { \
      errMsg << "Line " << t.line << " column " << t.getColumn() << ": expected '" << (x) << "', '" << symbol_ << "' found."; \
      goto error; \
    } \
  }

#define CHECK_COLON() CHECK_SYMBOL(":")

std::string CTextExport::importFile(const char *fileName) {
  std::ostringstream errMsg;

  // read file into "text"
  std::ifstream f(fileName, std::ifstream::in);
  std::stringstream buffer;
  buffer << f.rdbuf();
  f.close();
  std::string text = buffer.str();

  /*// begin a new document
  if (!pDoc->OnNewDocument())
  {
    sResult = _T("Unable to create new Famitracker document.");
    return sResult;
  }*/

  // parse the file
  Tokenizer t(&text);
  int i; // generic integer for reading
  unsigned int dpcm_index = 0;
  unsigned int dpcm_pos = 0;
  unsigned int track = 0;
  unsigned int pattern = 0;
  while (!t.finished()) {
    // read first token on line
    if (t.isEOL()) continue; // blank line
    std::string command = t.readToken();

    int c = 0;
    for (; c < CT_COUNT; ++c)
      if (0 == strcasecmp(command.c_str(), CT[c])) break;

    //DEBUG_OUT("Command read: %s\n", command);
    switch (c) {
      case CT_COMMENTLINE:
        t.finishLine();
        break;
      case CT_TITLE:
        //pDoc->SetSongName(Charify(t.readToken()));
        CHECK(t.readEOL(&err));
        break;
      case CT_AUTHOR:
        //pDoc->SetSongArtist(Charify(t.readToken()));
        CHECK(t.readEOL(&err));
        break;
      case CT_COPYRIGHT:
        //pDoc->SetSongCopyright(Charify(t.readToken()));
        CHECK(t.readEOL(&err));
        break;
      case CT_COMMENT: {
          /*CString sComment = pDoc->GetComment();
          if (sComment.GetLength() > 0)
            sComment = sComment + _T("\r\n");
          sComment += t.readToken();
          pDoc->SetComment(sComment, pDoc->ShowCommentOnOpen()); */
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_MACHINE:
        CHECK(t.readInt(i, 0, PAL, &err));
        //pDoc->SetMachine(i);
        CHECK(t.readEOL(&err));
        break;
      case CT_FRAMERATE:
        CHECK(t.readInt(i, 0, 800, &err));
        //pDoc->SetEngineSpeed(i);
        CHECK(t.readEOL(&err));
        break;
      case CT_EXPANSION:
        CHECK(t.readInt(i, 0, 255, &err));
        //pDoc->SelectExpansionChip(i);
        CHECK(t.readEOL(&err));
        break;
      case CT_VIBRATO:
        CHECK(t.readInt(i, 0, VIBRATO_NEW, &err));
        //pDoc->SetVibratoStyle((vibrato_t)i);
        CHECK(t.readEOL(&err));
        break;
      case CT_SPLIT:
        CHECK(t.readInt(i, 0, 255, &err));
        //pDoc->SetSpeedSplitPoint(i);
        CHECK(t.readEOL(&err));
        break;
      case CT_N163CHANNELS:
        CHECK(t.readInt(i, 1, 8, &err));
        //pDoc->SetNamcoChannels(i);
        //pDoc->SelectExpansionChip(pDoc->GetExpansionChip());
        CHECK(t.readEOL(&err));
        break;
      case CT_MACRO:
      case CT_MACROVRC6:
      case CT_MACRON163:
      case CT_MACROS5B: {
          const int CHIP_MACRO[4] = { SNDCHIP_NONE, SNDCHIP_VRC6, SNDCHIP_N163, SNDCHIP_S5B };
          int chip = c - CT_MACRO;

          int mt;
          CHECK(t.readInt(mt, 0, SEQ_COUNT - 1, &err));
          CHECK(t.readInt(i, 0, MAX_SEQUENCES - 1, &err));
          //CSequence* pSeq = pDoc->GetSequence(CHIP_MACRO[chip], i, mt);

          CHECK(t.readInt(i, -1, MAX_SEQUENCE_ITEMS, &err));
          //pSeq->SetLoopPoint(i);
          CHECK(t.readInt(i, -1, MAX_SEQUENCE_ITEMS, &err));
          //pSeq->SetReleasePoint(i);
          CHECK(t.readInt(i, 0, 255, &err));
          //pSeq->SetSetting(i);

          CHECK_COLON();

          int count = 0;
          while (!t.isEOL()) {
            CHECK(t.readInt(i, -128, 127, &err));
            if (count >= MAX_SEQUENCE_ITEMS) {
              errMsg << "Line " << t.line << " column " << t.getColumn() << ": macro overflow, max size: " << MAX_SEQUENCE_ITEMS << ".";
              goto error;
            }
            //pSeq->SetItem(count, i);
            ++count;
          }
          //pSeq->SetItemCount(count);
        }
        break;
      case CT_DPCMDEF: {
          CHECK(t.readInt(i, 0, MAX_DSAMPLES - 1, &err));
          dpcm_index = i;
          dpcm_pos = 0;

          //CHECK(t.readInt(i,0,CDSample::MAX_SIZE,&err));
          //CDSample* pSample = pDoc->GetSample(dpcm_index);
          //pSample->Allocate(i, NULL);
          //::memset(pSample->GetData(), 0, i);
          //pSample->SetName(Charify(t.readToken()));

          CHECK(t.readEOL(&err));
        }
        break;
      case CT_DPCM: {
          //CDSample* pSample = pDoc->GetSample(dpcm_index);
          CHECK_COLON();
          while (!t.isEOL()) {
            CHECK(t.readHex(i, 0x00, 0xFF, &err));
//            if (dpcm_pos >= pSample->GetSize())
//            {
//              sResult.Format(_T("Line %d column %d: DPCM sample %d overflow, increase size used in %s."), t.line, t.GetColumn(), dpcm_index, CT[CT_DPCMDEF]);
//              return sResult;
//            }
//            *(pSample->GetData() + dpcm_pos) = (char)(i);
            ++dpcm_pos;
          }
        }
        break;
      case CT_INST2A03: {
          CHECK(t.readInt(i, 0, MAX_INSTRUMENTS - 1, &err));
          //CInstrument2A03* pInst = (CInstrument2A03*)pDoc->CreateInstrument(INST_2A03);
          //pDoc->AddInstrument(pInst, i);
          for (int s = 0; s < SEQ_COUNT; ++s) {
            CHECK(t.readInt(i, -1, MAX_SEQUENCES - 1, &err));
            //pInst->SetSeqEnable(s, (i == -1) ? 0 : 1);
            //pInst->SetSeqIndex(s, (i == -1) ? 0 : i);
          }
          //pInst->SetName(Charify(t.readToken()));
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_INSTVRC6: {
          CHECK(t.readInt(i, 0, MAX_INSTRUMENTS - 1, &err));
          //CInstrumentVRC6* pInst = (CInstrumentVRC6*)pDoc->CreateInstrument(INST_VRC6);
          //pDoc->AddInstrument(pInst, i);
          for (int s = 0; s < SEQ_COUNT; ++s) {
            CHECK(t.readInt(i, -1, MAX_SEQUENCES - 1, &err));
//            pInst->SetSeqEnable(s, (i == -1) ? 0 : 1);
//            pInst->SetSeqIndex(s, (i == -1) ? 0 : i);
          }
//          pInst->SetName(Charify(t.readToken()));
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_INSTVRC7: {
          CHECK(t.readInt(i, 0, MAX_INSTRUMENTS - 1, &err));
          //CInstrumentVRC7* pInst = (CInstrumentVRC7*)pDoc->CreateInstrument(INST_VRC7);
          //pDoc->AddInstrument(pInst, i);
          CHECK(t.readInt(i, 0, 15, &err));
          //pInst->SetPatch(i);
          for (int r = 0; r < 8; ++r) {
            CHECK(t.readHex(i, 0x00, 0xFF, &err));
            //pInst->SetCustomReg(r, i);
          }
          //pInst->SetName(Charify(t.readToken()));
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_INSTFDS: {
          CHECK(t.readInt(i, 0, MAX_INSTRUMENTS - 1, &err));
          //CInstrumentFDS* pInst = (CInstrumentFDS*)pDoc->CreateInstrument(INST_FDS);
          //pDoc->AddInstrument(pInst, i);
          CHECK(t.readInt(i, 0, 1, &err));
          //pInst->SetModulationEnable(i==1);
          CHECK(t.readInt(i, 0, 4095, &err));
          //pInst->SetModulationSpeed(i);
          CHECK(t.readInt(i, 0, 63, &err));
          //pInst->SetModulationDepth(i);
          CHECK(t.readInt(i, 0, 255, &err));
          //pInst->SetModulationDelay(i);
          //pInst->SetName(Charify(t.readToken()));
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_INSTN163: {
          CHECK(t.readInt(i, 0, MAX_INSTRUMENTS - 1, &err));
          //CInstrumentN163* pInst = (CInstrumentN163*)pDoc->CreateInstrument(INST_N163);
          //pDoc->AddInstrument(pInst, i);
          for (int s=0; s < SEQ_COUNT; ++s) {
            CHECK(t.readInt(i, -1, MAX_SEQUENCES - 1, &err));
            //pInst->SetSeqEnable(s, (i == -1) ? 0 : 1);
            //pInst->SetSeqIndex(s, (i == -1) ? 0 : i);
          }
          //CHECK(t.readInt(i,0,CInstrumentN163::MAX_WAVE_SIZE,&sResult));
          //pInst->SetWaveSize(i);
          CHECK(t.readInt(i, 0, 127, &err));
          //pInst->SetWavePos(i);
          //CHECK(t.readInt(i, 0, CInstrumentN163::MAX_WAVE_COUNT,&sResult));
          //pInst->SetWaveCount(i);
          //pInst->SetName(Charify(t.readToken()));
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_INSTS5B: {
          CHECK(t.readInt(i, 0, MAX_INSTRUMENTS - 1, &err));
          //CInstrumentS5B* pInst = (CInstrumentS5B*)pDoc->CreateInstrument(INST_S5B);
          //pDoc->AddInstrument(pInst, i);
          for (int s=0; s < SEQ_COUNT; ++s) {
            CHECK(t.readInt(i, -1, MAX_SEQUENCES - 1, &err));
            //pInst->SetSeqEnable(s, (i == -1) ? 0 : 1);
            //pInst->SetSeqIndex(s, (i == -1) ? 0 : i);
          }
          //pInst->SetName(Charify(t.readToken()));
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_KEYDPCM: {
          CHECK(t.readInt(i, 0, MAX_INSTRUMENTS - 1, &err));
//          if (pDoc->GetInstrumentType(i) != INST_2A03) {
//            sResult.Format(_T("Line %d column %d: instrument %d is not defined as a 2A03 instrument."), t.line, t.GetColumn(), i);
//            return sResult;
//          }
//          CInstrument2A03* pInst = (CInstrument2A03*)pDoc->GetInstrument(i);

          int io, in;
          CHECK(t.readInt(io, 0, OCTAVE_RANGE, &err));
          CHECK(t.readInt(in, 0, 12, &err));

          CHECK(t.readInt(i, 0, MAX_DSAMPLES - 1, &err));
          //pInst->SetSample(io, in, i + 1);
          CHECK(t.readInt(i, 0, 15, &err));
          //pInst->SetSamplePitch(io, in, i);
          CHECK(t.readInt(i, 0, 1, &err));
          //pInst->SetSampleLoop(io, in, i==1);
          CHECK(t.readInt(i, 0, 255, &err));
          //pInst->SetSampleLoopOffset(io, in, i);
          CHECK(t.readInt(i, -1, 127, &err));
          //pInst->SetSampleDeltaValue(io, in, i);

          CHECK(t.readEOL(&err));
        }
        break;
      case CT_FDSWAVE: {
          CHECK(t.readInt(i, 0, MAX_INSTRUMENTS - 1, &err));
//          if (pDoc->GetInstrumentType(i) != INST_FDS)
//          {
//            sResult.Format(_T("Line %d column %d: instrument %d is not defined as an FDS instrument."), t.line, t.GetColumn(), i);
//            return sResult;
//          }
//          CInstrumentFDS* pInst = (CInstrumentFDS*)pDoc->GetInstrument(i);
          CHECK_COLON();
//          for (int s = 0; s < CInstrumentFDS::WAVE_SIZE; ++s)
//          {
//            CHECK(t.readInt(i,0,63,&sResult));
//            pInst->SetSample(s, i);
//          }
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_FDSMOD: {
          CHECK(t.readInt(i, 0, MAX_INSTRUMENTS - 1, &err));
//          if (pDoc->GetInstrumentType(i) != INST_FDS)
//          {
//            sResult.Format(_T("Line %d column %d: instrument %d is not defined as an FDS instrument."), t.line, t.GetColumn(), i);
//            return sResult;
//          }
//          CInstrumentFDS* pInst = (CInstrumentFDS*)pDoc->GetInstrument(i);
          CHECK_COLON();
//          for (int s = 0; s < CInstrumentFDS::MOD_SIZE; ++s)
//          {
//            CHECK(t.readInt(i,0,7,&sResult));
//            pInst->SetModulation(s, i);
//          }
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_FDSMACRO: {
          CHECK(t.readInt(i, 0, MAX_INSTRUMENTS - 1, &err));
//          if (pDoc->GetInstrumentType(i) != INST_FDS)
//          {
//            sResult.Format(_T("Line %d column %d: instrument %d is not defined as an FDS instrument."), t.line, t.GetColumn(), i);
//            return sResult;
//          }
//          CInstrumentFDS* pInst = (CInstrumentFDS*)pDoc->GetInstrument(i);

          CHECK(t.readInt(i, 0, 2, &err));
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
              errMsg << "Line " << t.line << " column " << t.getColumn() << ": unexpected error.";
              goto error;
          }
          CHECK(t.readInt(i, -1, MAX_SEQUENCE_ITEMS, &err));
          //pSeq->SetLoopPoint(i);
          CHECK(t.readInt(i, -1, MAX_SEQUENCE_ITEMS, &err));
          //pSeq->SetReleasePoint(i);
          CHECK(t.readInt(i, 0, 255,&err));
          //pSeq->SetSetting(i);

          CHECK_COLON();

          int count = 0;
          while (!t.isEOL()) {
            CHECK(t.readInt(i, -128, 127, &err));
            if (count >= MAX_SEQUENCE_ITEMS) {
              errMsg << "Line " << t.line << " column " << t.getColumn() << ": macro overflow, max size: " << MAX_SEQUENCE_ITEMS << ".";
              goto error;
            }
            //pSeq->SetItem(count, i);
            ++count;
          }
          //pSeq->SetItemCount(count);
        }
        break;
      case CT_N163WAVE: {
          CHECK(t.readInt(i, 0, MAX_INSTRUMENTS - 1, &err));
//          if (pDoc->GetInstrumentType(i) != INST_N163)
//          {
//            sResult.Format(_T("Line %d column %d: instrument %d is not defined as an N163 instrument."), t.line, t.GetColumn(), i);
//            return sResult;
//          }
//          CInstrumentN163* pInst = (CInstrumentN163*)pDoc->GetInstrument(i);

          int iw;
          //CHECK(t.readInt(iw, 0, CInstrumentN163::MAX_WAVE_COUNT - 1, &err));
          CHECK_COLON();
//          for (int s=0; s < pInst->GetWaveSize(); ++s)
//          {
//            CHECK(t.readInt(i,0,15,&sResult));
//            pInst->SetSample(iw, s, i);
//          }
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_TRACK: {
          if (track != 0) {
//            if(pDoc->AddTrack() == -1) {
//              sResult.Format(_T("Line %d column %d: unable to add new track."), t.line, t.GetColumn());
//              return sResult;
//            }
          }

          CHECK(t.readInt(i, 0, MAX_PATTERN_LENGTH, &err));
          //pDoc->SetPatternLength(track, i);
          CHECK(t.readInt(i, 0, MAX_TEMPO, &err));
          //pDoc->SetSongSpeed(track, i);
          CHECK(t.readInt(i, 0, MAX_TEMPO, &err));
          //pDoc->SetSongTempo(track, i);
          //pDoc->SetTrackTitle(track, t.readToken());

          CHECK(t.readEOL(&err));
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
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_ORDER: {
          int ifr;
          CHECK(t.readHex(ifr, 0, MAX_FRAMES - 1, &err));
//          if (ifr >= (int)pDoc->GetFrameCount(track-1)) // expand to accept frames
//          {
//            pDoc->SetFrameCount(track-1,ifr+1);
//          }
          CHECK_COLON();
//          for (int c = 0; c < pDoc->GetChannelCount(); ++c)
//          {
//            CHECK(t.readHex(i, 0, MAX_PATTERN - 1, &sResult));
//            pDoc->SetPatternAtFrame(track - 1, ifr, c, i);
//          }
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_PATTERN:
        CHECK(t.readHex(i, 0, MAX_PATTERN - 1, &err));
        pattern = i;
        CHECK(t.readEOL(&err));
        break;
      case CT_ROW: {
          if (track == 0) {
            errMsg << "Line " << t.line << " column " << t.getColumn() << ": no TRACK defined, cannot add ROW data.";
            goto error;
          }

          CHECK(t.readHex(i, 0, MAX_PATTERN_LENGTH - 1, &err));
//          for (int c=0; c < pDoc->GetChannelCount(); ++c) {
//            CHECK_COLON();
//            if (!ImportCellText(pDoc, t, track-1, pattern, c, i, sResult)) {
//              return sResult;
//            }
//          }
          CHECK(t.readEOL(&err));
        }
        break;
      case CT_COUNT:
      default:
        errMsg << "Unrecognized command at line " << t.line << ": '" << command << "'.";
        goto error;
    }
  }

  return {};
error:
  return errMsg.str();
}
