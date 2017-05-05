#include "Compressor.h"

#include <experimental/optional>

using namespace std::experimental;

class BlockImpl {
public:
  BlockImpl() : commandCount(0), flagByte(0) {}
  
  void addMatch(const Match& match) {
    this->data.push_back(match.length);
    this->data.push_back(-match.delta);
    this->commandCount++;
  }

  void addLiteral(char c) {
    this->data.push_back(c);
    /* raise the literal flag */
    this->flagByte |= (1 << this->commandCount);
    this->commandCount++;
  }

  void addEOF(void) {
    this->data.push_back(0);
  }

  bool isFull(void) const {
    return this->commandCount == COMMANDS_PER_BLOCK;
  }

  uint8_t getFlagByte(void) const {
    return this->flagByte;
  }

  uint16_t sizeIncludingFlagByte(void) const {
    return this->data.size() + 1;
  }

  uint16_t sizeNoFlagByte(void) const {
    return this->data.size();
  }

  const char* begin(void) const {
    return &this->data[0];
  }

private:
  std::vector<char> data;
  uint8_t commandCount;
  uint8_t flagByte;

  const int COMMANDS_PER_BLOCK = 8;
};

Block::Block() : impl(std::make_unique<BlockImpl>()) {}
Block::Block(const BlockImpl& impl) : impl(std::make_unique<BlockImpl>(impl)) {}
Block::Block(Block&& block) : impl(std::move(block.impl)) {}
Block::~Block() {}

uint8_t Block::getFlagByte(void) const {
  return impl->getFlagByte();
}

uint16_t Block::sizeIncludingFlagByte(void) const {
  return impl->sizeIncludingFlagByte();
}

uint16_t Block::sizeNoFlagByte(void) const {
  return impl->sizeNoFlagByte();
}

const char* Block::begin(void) const {
  return impl->begin();
}

class CompressorImpl {
public:
  CompressorImpl(const std::vector<char>& inputBuffer) : inputBuffer(inputBuffer) {
    this->i = this->inputBuffer.cbegin();
  }

  bool isDone(void) const {
    return (this->i == this->inputBuffer.cend());
  }

  Block getNextBlock(void) {
    BlockImpl nextBlock;

    while(!nextBlock.isFull() && !this->isDone()) {
      optional<Match> longestMatch = getNextLongestMatch();
      if (longestMatch) {
	nextBlock.addMatch(*longestMatch);
	i += longestMatch->length;
      } else {
	nextBlock.addLiteral(*this->i);
	i++;
      }
    }

    if(this->isDone()) {
      nextBlock.addEOF();
    }

    return Block(nextBlock);
  }

private:
  const int COMMANDS_PER_BLOCK = 8;
  const std::vector<char> inputBuffer;
  std::vector<char>::const_iterator i;

  optional<Match> getNextLongestMatch() const {
    optional<Match> longestMatch;

    /* walk backwards from the read head */
    for (uint8_t j = 1; this->i - j >= inputBuffer.cbegin(); j++) {
      uint8_t matchLen;

      /* find the biggest match from this starting point */
      for(matchLen = 0;
	  this->i + matchLen < inputBuffer.cend() && *(this->i + matchLen) == *(this->i + matchLen - j);
	  matchLen++)
	{
	  if (matchLen == 255) {
	    break;
	  }
	}

      /* update it if this is the best */
      if ((longestMatch && matchLen > longestMatch->length) ||
	  (!longestMatch && matchLen > 2))
	{
	  longestMatch.emplace();
	  longestMatch->delta = j; 
	  longestMatch->length = matchLen;
	}

      if (j == 255) {
	break;
      }
    }

    return longestMatch;
  }
};

Compressor::Compressor(const std::vector<char>& inputBuffer) : impl(std::make_unique<CompressorImpl>(inputBuffer)) {}
Compressor::Compressor(Compressor&& compressor) : impl(std::move(compressor.impl)) {}
Compressor::~Compressor() {}

bool Compressor::isDone(void) const { return impl->isDone(); }
Block Compressor::getNextBlock(void) { return impl->getNextBlock(); }
