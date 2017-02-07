#include "Tokenizer.h"
#include <sstream>

Tokenizer::Tokenizer(const std::string& text_)
  : text(text_), pos(0), line(1), linestart(0)
{}

Tokenizer::~Tokenizer() {}

void Tokenizer::reset(void) {
  pos = 0;
  line = 1;
  linestart = 0;
}

void Tokenizer::consumeSpace(void) {
  while (pos < text.size()) {
    char c = text.at(pos);
    if (c != ' ' && c != '\t') {
      return;
    }
    ++pos;
  }
}

void Tokenizer::finishLine(void) {
  while (pos < text.size() && text.at(pos) != '\n') {
    ++pos;
  }

  if (pos < text.size()) ++pos; // skip newline
  ++line;
  linestart = pos;
}

int Tokenizer::getLine(void) const {
  return line;
}


int Tokenizer::getColumn(void) const {
  return 1 + pos - linestart;
}

bool Tokenizer::finished(void) const {
  return pos >= text.size();
}

std::string Tokenizer::readToken() {
  consumeSpace();
  std::string t = "";

  bool inQuote = false;
  bool lastQuote = false; // for finding double-quotes
  do {
    if (pos >= text.size()) {
      break;
    }

    char c = text.at(pos);
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

  return t;
}

int Tokenizer::readInt(int rangeMin, int rangeMax) {
  std::ostringstream errMsg;

  std::string t = readToken();
  int c = getColumn();

  if (t.size() < 1) {
    errMsg << "Line " << line << " column " << c << ": expected integer, no token found.";
    throw errMsg.str();
  }

  int i;
  int result = ::sscanf(t.c_str(), "%d", &i);
  if(result == EOF || result == 0) {
    errMsg << "Line " << line << " column " << c << ": expected integer, '" << t << "' found.";
    throw errMsg.str();
  }

  if (i < rangeMin || i > rangeMax) {
    errMsg << "Line " << line << " column " << c << ": expected integer in range [" << rangeMin << "," << rangeMax << "], " << i << " found.";
    throw errMsg.str();
  }

  return i;
}

int Tokenizer::readHex(int rangeMin, int rangeMax) {
  std::ostringstream errMsg;

  std::string t = readToken();
  int c = getColumn();

  if (t.size() < 1) {
    errMsg << "Line " << line << " column " << c << ": expected hexadecimal, no token found.";
    throw errMsg.str();
  }

  int i;
  int result = ::sscanf(t.c_str(), "%x", &i);
  if(result == EOF || result == 0) {
    errMsg << "Line " << line << " column " << c << ": expected hexadecimal, '" << t << "' found.";
    throw errMsg.str();
  }

  if (i < rangeMin || i > rangeMax) {
    errMsg << "Line " << line << " column " << c << ": expected hexadecimal in range [" << rangeMin << "," << rangeMax << "], " << i << " found.";
    throw errMsg.str();
  }
   
  return i;
}

// note: finishes line if found
void Tokenizer::readEOL(void) {
  std::ostringstream errMsg;

  int c = getColumn();
  consumeSpace();

  std::string s = readToken();
  if (s.size() > 0) {
    errMsg << "Line " << line << " column " << c << ": expected end of line, '" << s << "' found.";
    throw errMsg.str();
  }

  if (finished()) {
    return;
  }

  char eol = text.at(pos);
  if (eol != '\r' && eol != '\n') {
    errMsg << "Line " << line << " column " << c << ": expected end of line, '" << eol << "' found.";
    throw errMsg.str();
  }

  finishLine();
}

// note: finishes line if found
bool Tokenizer::isEOL(void) {
  consumeSpace();
  if (finished()) {
    return true;
  }

  char eol = text.at(pos);
  if (eol == '\r' || eol == '\n') {
    finishLine();
    return true;
  }

  return false;
}
