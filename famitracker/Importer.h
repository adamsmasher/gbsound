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

#include "Tokenizer.h"

class Importer {
public:
  Importer(const std::string& text);
  virtual ~Importer();

  static Importer fromFile(const char *filename);

  void runImport();
 private:
  Tokenizer t;
  int getVolId(const std::string& sVol) const;
  int getInstrumentId(const std::string& sInst) const;
  std::pair<int, int> getNoteAndOctave(const std::string& sNote) const;
  void importCellText(void);
  int importHex(const std::string& sToken) const;

  unsigned int track;
  unsigned int pattern;
  unsigned int channel;
  unsigned int row;
};
