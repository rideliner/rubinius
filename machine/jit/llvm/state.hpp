#ifndef VM_LLVM_STATE_HPP
#define VM_LLVM_STATE_HPP

#include <stdint.h>
#include <unistd.h>

#include "config.h"

#if RBX_LLVM_API_VER >= 303
#include <llvm/IR/Module.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#else
#include <llvm/Module.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Function.h>
#include <llvm/Instructions.h>
#endif
#if RBX_LLVM_API_VER >= 303
#include <llvm/IR/IRBuilder.h>
#elif RBX_LLVM_API_VER >= 302
#include <llvm/IRBuilder.h>
#else
#include <llvm/Support/IRBuilder.h>
#endif
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/CodeGen/MachineCodeInfo.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/Support/raw_ostream.h>
#if RBX_LLVM_API_VER >= 303
#include <llvm/IR/LLVMContext.h>
#else
#include <llvm/LLVMContext.h>
#endif
#include <llvm/ExecutionEngine/JITEventListener.h>

#include "jit/llvm/local_info.hpp"

#include "memory/managed.hpp"
#include "internal_threads.hpp"
#include "configuration.hpp"
#include "metrics.hpp"
#include "util/thread.hpp"

namespace rubinius {
  typedef std::map<int, LocalInfo> LocalMap;
  class State;
  class SymbolTable;
  class CompiledCode;

  namespace memory {
    class GarbageCollector;
  }

  namespace jit {
    class Builder;
    class Compiler;
    class RubiniusJITMemoryManager;
  }

  class JITCompileRequest;
  class BlockEnvironment;
  class Context;
  class List;
  class Symbol;
  class Thread;

  enum JitDebug {
    cSimple = 1,
    cOptimized = 2,
    cMachineCode = 4
  };

  class LLVMState : public InternalThread {
    jit::RubiniusJITMemoryManager* memory_;
    llvm::JITEventListener* jit_event_listener_;

    State* state_;
    Configuration& config_;

    memory::TypedRoot<List*> compile_list_;
    SymbolTable& symbols_;

    SharedState& shared_;

    bool include_profiling_;

    std::ostream* log_;

    uint32_t fixnum_class_id_;
    uint32_t integer_class_id_;
    uint32_t numeric_class_id_;
    uint32_t bignum_class_id_;
    uint32_t float_class_id_;
    uint32_t symbol_class_id_;
    uint32_t string_class_id_;
    uint32_t regexp_class_id_;
    uint32_t encoding_class_id_;
    uint32_t module_class_id_;
    uint32_t class_class_id_;
    uint32_t nil_class_id_;
    uint32_t true_class_id_;
    uint32_t false_class_id_;
    uint32_t array_class_id_;
    uint32_t tuple_class_id_;

    bool type_optz_;

    bool enabled_;

    jit::Compiler* current_compiler_;

    utilities::thread::SpinLock method_update_lock_;
    utilities::thread::Mutex wait_mutex;
    utilities::thread::Condition wait_cond;
    utilities::thread::Mutex request_lock_;
    utilities::thread::Mutex compile_lock_;
    utilities::thread::Condition compile_cond_;

    std::string cpu_;

  public:

    LLVMState(STATE);
    virtual ~LLVMState();

    State* state() {
      return state_;
    }

    void add_internal_functions();
    void enable(STATE);

    bool enabled() {
      return enabled_;
    }

    int jit_dump_code() {
      return config_.jit_dump_code;
    }

    bool debug_p();

    Configuration& config() {
      return config_;
    }

    bool include_profiling() {
      return include_profiling_;
    }

    jit::RubiniusJITMemoryManager* memory() { return memory_; }
    llvm::JITEventListener* jit_event_listener() { return jit_event_listener_; }

    SharedState& shared() { return shared_; }

    std::ostream& log() {
      return *log_;
    }

    uint32_t fixnum_class_id() {
      return fixnum_class_id_;
    }

    uint32_t integer_class_id() {
      return integer_class_id_;
    }

    uint32_t numeric_class_id() {
      return numeric_class_id_;
    }

    uint32_t bignum_class_id() {
      return bignum_class_id_;
    }

    uint32_t float_class_id() {
      return float_class_id_;
    }

    uint32_t symbol_class_id() {
      return symbol_class_id_;
    }

    uint32_t string_class_id() {
      return string_class_id_;
    }

    uint32_t regexp_class_id() {
      return regexp_class_id_;
    }

    uint32_t encoding_class_id() {
      return encoding_class_id_;
    }

    uint32_t module_class_id() {
      return module_class_id_;
    }

    uint32_t class_class_id() {
      return class_class_id_;
    }

    uint32_t nil_class_id() {
      return nil_class_id_;
    }

    uint32_t true_class_id() {
      return true_class_id_;
    }

    uint32_t false_class_id() {
      return false_class_id_;
    }

    uint32_t array_class_id() {
      return array_class_id_;
    }

    uint32_t tuple_class_id() {
      return tuple_class_id_;
    }

    bool type_optz() {
      return type_optz_;
    }

    std::string cpu() {
      return cpu_;
    }

    void start_method_update() {
      method_update_lock_.lock();
    }

    void end_method_update() {
      method_update_lock_.unlock();
    }

    void compile(STATE, CompiledCode* code,
        Class* receiver_class, BlockEnvironment* block_env = NULL, bool is_block=false);

    void compile_soon(STATE, CompiledCode* code,
        Class* receiver_class, BlockEnvironment* block_env = NULL, bool is_block=false);

    void add(STATE, JITCompileRequest* req);
    void remove(void* func);

    CallFrame* find_candidate(STATE, CallFrame* call_frame, CompiledCode* start);
    void compile_callframe(STATE, CompiledCode* start, int primitive = -1);

    Symbol* symbol(const std::string& sym);
    std::string symbol_debug_str(const Symbol* sym);

    std::string enclosure_name(CompiledCode* code);


    void initialize(STATE);
    void run(STATE);
    void wakeup(STATE);
    void stop(STATE);

    void after_fork_child(STATE);

    void gc_scan(memory::GarbageCollector* gc);

    static void show_machine_code(void* impl, size_t bytes);

    class CompileError {
      const char* error_;

    public:
      CompileError(const char* error)
        : error_(error)
      { }

      const char* error() {
        return error_;
      }
    };
  };
}

#endif
