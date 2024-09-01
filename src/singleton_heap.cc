#include "src/singleton_heap.h"

#include <cstdlib>

#include "src/heap_factory.h"
#include "src/heap_interface.h"

namespace bench {

/* static */
Heap* SingletonHeap::GlobalInstance() {
  HeapFactory* factory = HeapFactory::GlobalInstance();
  if (factory->Instance(0) == nullptr) {
    auto result = factory->NewInstance(kHeapSize);
    if (!result.ok()) {
      std::cerr << "Failed to create heap instance: " << result.status()
                << std::endl;
      exit(-1);
    }
  }
  return factory->Instance(0);
}

}  // namespace bench
