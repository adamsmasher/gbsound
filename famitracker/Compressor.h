#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct Match {
  uint8_t length;
  uint8_t delta;
};

/* we can think of the compressed output as being a sequence of "blocks", with each block
   containing eight "commands". A command can be either a literal byte or compressed; each command
   has a corresponding bit in the flag_byte that tells us which (it's 1 if literal or 0 if not) */
struct BlockImpl;
class Block {
 public:
  Block();
  Block(Block&&);
  Block(const BlockImpl&);
  ~Block();

  uint8_t getFlagByte(void) const;
  uint16_t sizeIncludingFlagByte(void) const;
  uint16_t sizeNoFlagByte(void) const;
  const char *begin(void) const;
 private:
  std::unique_ptr<BlockImpl> impl;
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
