#include "free_block.h"

Block* FreeBlock::FromRawPtr(void* ptr) {
  return reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(ptr) -
                                  offsetof(Block, body_));
}