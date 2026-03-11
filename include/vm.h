#pragma once
#include "opcodes.h"
#include "value.h"
#include <vector>
#include <stack>
#include <unordered_map>
#include <functional>
#include <memory>
#include <iostream>
#include <iomanip>

class VirtualMachine {
public:
    static constexpr size_t DEFAULT_STACK_SIZE   = 65536;  // 64K values
    static constexpr size_t DEFAULT_HEAP_SIZE    = 1048576; // 1M values
    static constexpr size_t MAX_CALL_DEPTH       = 1024;
    static constexpr uint64_t HEAP_BLOCK_HEADER  = 2;      // size + used flag

    // VM execution result
    enum class ExecResult {
        OK,
        HALTED,
        ERROR,
        BREAKPOINT,
    };

    struct VMStats {
        uint64_t instructionsExecuted = 0;
        uint64_t functionCalls = 0;
        uint64_t memoryAllocations = 0;
        uint64_t memoryFrees = 0;
        uint64_t exceptionsThrown = 0;
        uint64_t peakStackDepth = 0;
        uint64_t gcCollections = 0;
        uint64_t gcObjectsFreed = 0;
        uint64_t gcArraysFreed = 0;
    };

    VirtualMachine(size_t stackSize = DEFAULT_STACK_SIZE,
                   size_t heapSize = DEFAULT_HEAP_SIZE);
    ~VirtualMachine() = default;

    // Load a program (instructions + constant pool)
    void loadProgram(const std::vector<Instruction>& program, const ConstantPool& pool);

    // Execute until HALT or error
    ExecResult run();

    // Execute a single instruction (for debugging)
    ExecResult step();

    // Reset the VM state
    void reset();

    // Register a syscall handler: syscall number -> function
    using SyscallHandler = std::function<void(VirtualMachine&)>;
    void registerSyscall(uint16_t number, SyscallHandler handler);

    // Access registers
    Value& reg(uint16_t r);
    const Value& reg(uint16_t r) const;

    // Access the stack
    void push(const Value& val);
    Value pop();
    const Value& peek(size_t offset = 0) const;
    size_t stackDepth() const;

    // Access constant pool
    const ConstantPool& constants() const { return constantPool_; }

    // Get stats
    const VMStats& stats() const { return stats_; }

    // Set/get trace mode
    void setTrace(bool on) { tracing_ = on; }
    bool tracing() const { return tracing_; }

    // Program counter
    uint64_t pc() const { return pc_; }

    // Heap access (for syscalls)
    Value& heapAt(uint64_t addr);

    // Array access (for syscalls)
    ManagedArray& getArray(uint64_t id);

private:
    // Registers
    Value registers_[Register::NUM_REGISTERS];

    // Program
    std::vector<Instruction> program_;
    ConstantPool constantPool_;

    // Stack
    std::vector<Value> stack_;
    size_t sp_;  // stack pointer (index into stack_)

    // Heap (simple first-fit allocator)
    std::vector<Value> heap_;
    size_t heapSize_;

    // Heap free list: address -> size
    struct HeapBlock {
        uint64_t address;
        uint64_t size;
        bool     used;
        bool     marked;
    };
    std::vector<HeapBlock> heapBlocks_;

    // Managed arrays
    std::unordered_map<uint64_t, ManagedArray> arrays_;
    uint64_t nextArrayId_;

    // Call stack
    std::vector<CallFrame> callStack_;

    // Exception handlers
    std::vector<ExceptionHandler> exceptionHandlers_;

    // Syscall table
    std::unordered_map<uint16_t, SyscallHandler> syscalls_;

    // State
    uint64_t pc_;
    uint8_t  flags_;
    bool     running_;
    bool     tracing_;
    VMStats  stats_;

    // GC configuration
    bool     gcEnabled_;
    uint64_t gcThreshold_;
    uint64_t gcAllocsSinceLastCollect_;

    // Instruction dispatch
    ExecResult dispatch(const Instruction& instr);

    // Helpers
    void setFlags(int64_t result);
    void setFlagsFloat(double result);
    bool checkFlag(uint8_t flag) const;

    // Heap management
    uint64_t heapAlloc(uint64_t size);
    void     heapFree(uint64_t addr);
    uint64_t heapRealloc(uint64_t addr, uint64_t newSize);

    // Tracing
    void traceInstruction(const Instruction& instr) const;

    // Exception handling
    void throwException(int64_t errorCode);

    // Garbage collection
    void collectGarbage();
    void gcMark();
    void gcMarkValue(const Value& val, std::vector<uint64_t>& heapWorklist,
                     std::unordered_map<uint64_t, bool>& markedArrays);
    void gcMarkHeapBlock(uint64_t addr, std::vector<uint64_t>& heapWorklist,
                         std::unordered_map<uint64_t, bool>& markedArrays);
    void gcSweep(std::unordered_map<uint64_t, bool>& markedArrays);
};
