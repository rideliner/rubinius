#include "oop.hpp"
#include "vm.hpp"
#include "state.hpp"
#include "memory.hpp"
#include "memory/inflated_headers.hpp"

#include <iostream>

namespace rubinius {
namespace memory {
  void InflatedHeaders::Diagnostics::log() {
    if(!modified_p()) return;

    diagnostics::Diagnostics::log();

    logger::write("inflated headers: diagnostics: " \
        "objects: %ld, bytes: %ld, collections: %ld\n",
        objects_, bytes_, collections_);
  }

  InflatedHeader* InflatedHeaders::allocate(STATE, ObjectHeader* obj, uint32_t* index) {
    bool needs_gc = false;
    uintptr_t header_index = allocator_->allocate_index(&needs_gc);
    if(header_index > UINT32_MAX) {
      rubinius::bug("Rubinius can't handle more than 4G inflated headers active at the same time");
    }
    *index = (uint32_t)header_index;
    InflatedHeader* header = allocator_->from_index(header_index);
    if(needs_gc) {
      diagnostics_.collections_++;
      state->memory()->schedule_full_collection(
          "Inflated headers",
          state->vm()->metrics().gc.headers_set);
    }
    atomic::memory_barrier();
    return header;
  }

  void InflatedHeaders::deallocate_headers(unsigned int mark) {
    std::vector<bool> chunk_marks(allocator_->chunks_.size(), false);

    diagnostics_.objects_ = 0;

    for(std::vector<int>::size_type i = 0; i < allocator_->chunks_.size(); ++i) {
      InflatedHeader* chunk = allocator_->chunks_[i];

      for(size_t j = 0; j < allocator_->cChunkSize; j++) {
        InflatedHeader* header = &chunk[j];

        if(header->marked_p(mark)) {
          chunk_marks[i] = true;
          diagnostics_.objects_++;
        } else {
          header->clear();
        }
      }
    }

    allocator_->rebuild_freelist(&chunk_marks);

    diagnostics_.bytes_ = allocator_->in_use_ * sizeof(InflatedHeader);
    diagnostics_.modify();
  }
}
}
