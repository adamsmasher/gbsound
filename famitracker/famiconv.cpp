#include <sstream>
#include <fstream>

#include "Importer.h"

int main(int argc, char **argv) {
  if(argc != 3) {
    std::ostringstream errMsg;
    errMsg << "Missing command line arguments; usage: " << argv[0] << " IN.ftm OUT.bin";
    throw errMsg.str();
  }

  Importer importer = Importer::fromFile(argv[1]);
  Song song = importer.runImport();

  std::ofstream out;
  out.open(argv[2], std::ios::binary);
  out << song;
  out.close();
}
