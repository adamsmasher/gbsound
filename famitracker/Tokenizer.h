#pragma once

#include <string>

class Tokenizer {
 public:
  Tokenizer(const std::string& text_);
  virtual ~Tokenizer();

  void reset(void);
  
  int getColumn(void) const;
  bool finished(void) const;
  int getLine(void) const;
  
  std::string readToken(void);
  int readInt(int rangeMin, int rangeMax);
  int readHex(int rangeMin, int rangeMax);
  void readEOL(void);
  bool isEOL(void);
  void finishLine(void);
 private:
  void consumeSpace(void);

  const std::string text;
  size_t pos;
  int line;
  int linestart;
};
