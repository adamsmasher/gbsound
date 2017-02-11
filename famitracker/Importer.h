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

#pragma once

#include <string>

#include "Command.h"
#include "Song.h"
#include "Tokenizer.h"

// TODO: add a flag that invalidates this after use
class Importer {
public:
  Importer(const std::string& text);
  virtual ~Importer();

  static Importer fromFile(const char *filename);

  Song runImport();
 private:
  Tokenizer t;
  int getVolId(const std::string& sVol) const;
  int getInstrumentId(const std::string& sInst) const;
  std::pair<int, int> getNoteAndOctave(const std::string& sNote) const;
  void importCellText(void);
  int importHex(const std::string& sToken) const;
  Command getCommandEnum(const std::string& command) const;
  void importCommand(Command);
  void importMacro(int chip);
  void importStandardMacro(void);
  void importStandardInstrument(void);
  void importTrack(void);
  void importColumns(void);
  void importOrder(void);
  void importRow(void);
  void importMachine(void);
  void importVibrato(void);
  void importSplit(void);
  void importFramerate(void);
  void importPattern(void);
  void importExpansion(void);
  void checkColon(void);
  void checkSymbol(const std::string&);
  void importN163Wave(void);
  void importN163Channels(void);
  void importN163Instrument(void);
  void importN163Macro(void);
  void skipInstrumentName(void);
  void skipInstrumentNumber(void);
  int readSequenceNumber(void);

  unsigned int track;
  unsigned int pattern;
  unsigned int channel;
  unsigned int row;

  Song song;
};
