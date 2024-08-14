
#include <cassert>

class Block {
 public:
  // Block* take_free_block(size_t size) {
  Block* take_free_block();

  uint64_t GetBlockSize() const;

  uint64_t GetUserSize() const;

  void SetBlockSize(uint64_t size);

  bool IsFree() const;

  void SetFree(bool free);

  void SetMagic();

  uint8_t* GetBody();

  static Block* FromRawPtr(void* ptr);

  void CheckValid() const;

  Block* GetNextBlock();

 private:
  uint64_t header_;
  uint64_t magic_value_ = 123456;
  uint64_t garbage_ = -0;
  // flexible size, returns pointer to beginning of body
  uint8_t body_[];
};