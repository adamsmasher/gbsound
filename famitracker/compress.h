#pragma once

// TODO: rename me

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct Match {
  uint8_t length;
  uint8_t delta;
};

// TODO: use PIMPL to hide the implementation
/* we can think of the compressed output as being a sequence of "blocks", with each block
   containing eight "commands". A command can be either a literal byte or compressed; each command
   has a corresponding bit in the flag_byte that tells us which (it's 1 if literal or 0 if not) */
struct Block {
  Block();

  uint8_t flagByte;
  std::vector<char> data;

  void addMatch(const Match&);
  void addLiteral(char);
  void addEOF(void);

  bool isFull(void) const;

  uint8_t commandCount;
private:
  const uint8_t COMMANDS_PER_BLOCK = 8;
};

struct CompressorImpl;
class Compressor {
 public:
  explicit Compressor(const std::vector<char>&);
  Compressor(Compressor&&);
  ~Compressor();
  Block getNextBlock(void);
  bool isDone(void) const;
 private:
  std::unique_ptr<CompressorImpl> impl;
};
