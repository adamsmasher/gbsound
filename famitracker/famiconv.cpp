#include <sstream>
#include <fstream>

#include "Importer.h"

int main(int argc, char **argv) {
  if(argc != 3) {
    std::ostringstream errMsg;
    errMsg << "Missing command line arguments; usage: " << argv[0] << " IN.txt OUT.bin";
    std::cerr << errMsg.str();
    return -1;
  }

  Importer importer = Importer::fromFile(argv[1]);

  std::ofstream out;
  try {
    Song song = importer.runImport();
    out.open(argv[2], std::ios::binary);
    song.writeGb(out);
  } catch (const std::stringstream& error) {
    std::cerr << "Error: " << error.str();
    return -2;
  } catch (const std::string& error) {
    std::cerr << "Error: " << error;
    return -2;
  } catch (const std::runtime_error& error) {
    std::cerr << "Error: " << error.what();
    return -2;
  }
  out.close();
}
