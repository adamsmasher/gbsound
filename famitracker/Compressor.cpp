#include "Compressor.h"

#include <experimental/optional>

using namespace std::experimental;

Block::Block() : flagByte(0), commandCount(0) {}

void Block::addMatch(const Match& match) {
  this->data.push_back(match.length);
  this->data.push_back(-match.delta);
  this->commandCount++;
}

void Block::addLiteral(char c) {
  this->data.push_back(c);
  /* raise the literal flag */
  this->flagByte |= (1 << this->commandCount);
  this->commandCount++;
}

void Block::addEOF(void) {
  this->data.push_back(0);
}

bool Block::isFull(void) const {
  return this->commandCount == COMMANDS_PER_BLOCK;
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
    Block nextBlock;

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

    return nextBlock;
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
