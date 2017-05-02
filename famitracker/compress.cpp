#include "compress.h"

#include <experimental/optional>

using namespace std::experimental;

// TODO: change this to be a class that you repeatedly query for the next block

struct Match {
  uint8_t length;
  uint8_t delta;
};

optional<Match> find_longest_match(const char* buffer, size_t bufferLength, size_t i);

std::vector<char> compress(const char *inputBuffer, size_t inputLength) {
  /* we can think of the compressed output as being a sequence of "blocks", with each block
     containing eight "commands". A command can be either a literal byte or compressed; each command
     has a corresponding bit in the flag_byte that tells us which (it's 1 if literal or 0 if not) */
  uint8_t flag_byte = 0;

  // how many commands we've written this block
  int command_cnt;

  // where in the output stream the last flag byte was
  size_t last_flag_pos;

  std::vector<char> output;

  command_cnt = 0;
  for (size_t i = 0; i < inputLength;) {
    // check if we need to reserve a flag byte
    if (command_cnt % 8 == 0) {
      last_flag_pos = output.size();
      /* just write a garbage byte to reserve a slot for the flags */
      output.push_back(0xAA);
      flag_byte = 0;
      command_cnt = 0;
    }

    /* write out the next command */
    optional<Match> longest_match = find_longest_match(inputBuffer, inputLength, i);
    if (longest_match) {
      output.push_back(longest_match->length);
      output.push_back(-longest_match->delta);
      i += longest_match->length;
    } else {
      output.push_back(inputBuffer[i]);
      i++;
      /* raise the literal flag */
      flag_byte = (uint8_t)(flag_byte | (1 << command_cnt));
    }

    /* check if we need to write a flag byte */
    command_cnt++;
    if (command_cnt == 8) {
      output[last_flag_pos] = flag_byte;
    }
  }

  /* if we just wrote a flag byte, we need to write another one */
  if (command_cnt % 8 == 0) {
    output.push_back(0);
  } else {
    /* otherwise we need to write this one properly */
    output[last_flag_pos] = flag_byte;
  }

  /* write an EOF marker */
  output.push_back(0);
  return output;
}

optional<Match> find_longest_match(const char *inputBuffer, size_t bufferLength, size_t i) {
  optional<Match> longest_match;
  uint8_t match_len;
  uint8_t j;

  /* walk backwards from the read head */
  for (j = 1; i - j >= 0; j++) {

    /* find the biggest match from this starting point */
    for(match_len = 0;
        i + match_len < bufferLength &&
        inputBuffer[i + match_len] == inputBuffer[i + match_len - j];
        match_len++) {
      if (match_len == 255) {
        break;
      }
    }

    /* update it if this is the best */
    if ((longest_match && match_len > longest_match->length) ||
        (!longest_match && match_len > 2)) {
      longest_match.emplace();
      longest_match->delta = j; 
      longest_match->length = match_len;
    }

    if (j == 255) {
      break;
    }
  }

  return longest_match;
}
