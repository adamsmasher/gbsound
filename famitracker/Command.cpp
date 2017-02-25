#include "Command.h"

std::string commandName(Command c) {
  switch(c) {
  case CT_COMMENTLINE: return "Commentline";
  case CT_TITLE: return "Title";
  case CT_AUTHOR: return "Author";
  case CT_COPYRIGHT: return "Copyright";
  case CT_COMMENT: return "Comment";
  case CT_MACHINE: return "Machine";
  case CT_FRAMERATE: return "Framerate";
  case CT_EXPANSION: return "Expansion";
  case CT_VIBRATO: return "Vibrato";
  case CT_SPLIT: return "Split";
  case CT_N163CHANNELS: return "N163Channels";
  case CT_MACRO: return "Macro";
  case CT_MACROVRC6: return "MacroVRC6";
  case CT_MACRON163: return "MacroN163";
  case CT_MACROS5B: return "MacroS5B";
  case CT_DPCMDEF: return "DPCMDef";
  case CT_DPCM: return "DPCM";
  case CT_INST2A03: return "Inst2A03";
  case CT_INSTVRC6: return "InstVRC6";
  case CT_INSTVRC7: return "InstVRC7";
  case CT_INSTFDS: return "InstFDS";
  case CT_INSTN163: return "InstN163";
  case CT_INSTS5B: return "InstS6B";
  case CT_KEYDPCM: return "KeyDPCM";
  case CT_FDSWAVE: return "FDSWave";
  case CT_FDSMOD: return "FDSMod";
  case CT_FDSMACRO: return "FDSMacro";
  case CT_N163WAVE: return "N163Wave";
  case CT_TRACK: return "Track";
  case CT_COLUMNS: return "Columns";
  case CT_ORDER: return "Order";
  case CT_PATTERN: return "Pattern";
  case CT_ROW: return "Row";
  default: throw "Unknown command";
  }
}
