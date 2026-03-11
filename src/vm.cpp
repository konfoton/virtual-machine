#include "vm.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <unordered_set>

// ============================================================================
// Construction / Setup
// ============================================================================

VirtualMachine::VirtualMachine(size_t stackSize, size_t heapSize)
    : sp_(0), heapSize_(heapSize), nextArrayId_(1),
      pc_(0), flags_(0), running_(false), tracing_(false),
      gcEnabled_(true), gcThreshold_(256), gcAllocsSinceLastCollect_(0)
{
    stack_.resize(stackSize);
    heap_.resize(heapSize);

    // Initialize heap: one big free block
    heapBlocks_.push_back({0, heapSize, false, false});

    reset();
}

void VirtualMachine::loadProgram(const std::vector<Instruction>& program,
                                  const ConstantPool& pool)
{
    program_ = program;
    constantPool_ = pool;
    reset();
}

void VirtualMachine::reset() {
    for (auto& r : registers_) r = Value();
    sp_ = 0;
    pc_ = 0;
    flags_ = 0;
    running_ = false;
    stats_ = {};
    callStack_.clear();
    exceptionHandlers_.clear();
    arrays_.clear();
    nextArrayId_ = 1;

    // Reset heap
    heapBlocks_.clear();
    heapBlocks_.push_back({0, heapSize_, false, false});
    for (size_t i = 0; i < heapSize_; ++i) heap_[i] = Value();

    // Reset GC state
    gcAllocsSinceLastCollect_ = 0;

    // Set stack pointer register
    registers_[RSP] = Value(static_cast<int64_t>(0));
    registers_[RFP] = Value(static_cast<int64_t>(0));
}

void VirtualMachine::registerSyscall(uint16_t number, SyscallHandler handler) {
    syscalls_[number] = std::move(handler);
}

// ============================================================================
// Register / Stack Access
// ============================================================================

Value& VirtualMachine::reg(uint16_t r) {
    if (r >= Register::NUM_REGISTERS)
        throw std::out_of_range("Invalid register: " + std::to_string(r));
    return registers_[r];
}

const Value& VirtualMachine::reg(uint16_t r) const {
    if (r >= Register::NUM_REGISTERS)
        throw std::out_of_range("Invalid register: " + std::to_string(r));
    return registers_[r];
}

void VirtualMachine::push(const Value& val) {
    if (sp_ >= stack_.size())
        throw std::overflow_error("Stack overflow");
    stack_[sp_++] = val;
    if (sp_ > stats_.peakStackDepth) stats_.peakStackDepth = sp_;
    registers_[RSP] = Value(static_cast<int64_t>(sp_));
}

Value VirtualMachine::pop() {
    if (sp_ == 0)
        throw std::underflow_error("Stack underflow");
    Value val = stack_[--sp_];
    registers_[RSP] = Value(static_cast<int64_t>(sp_));
    return val;
}

const Value& VirtualMachine::peek(size_t offset) const {
    if (offset >= sp_)
        throw std::out_of_range("Stack peek out of range");
    return stack_[sp_ - 1 - offset];
}

size_t VirtualMachine::stackDepth() const { return sp_; }

Value& VirtualMachine::heapAt(uint64_t addr) {
    if (addr >= heap_.size())
        throw std::out_of_range("Heap access out of range: " + std::to_string(addr));
    return heap_[addr];
}

ManagedArray& VirtualMachine::getArray(uint64_t id) {
    auto it = arrays_.find(id);
    if (it == arrays_.end())
        throw std::out_of_range("Invalid array reference: " + std::to_string(id));
    return it->second;
}

// ============================================================================
// Heap Management (first-fit allocator)
// ============================================================================

uint64_t VirtualMachine::heapAlloc(uint64_t size) {
    // Try GC if threshold reached
    if (gcEnabled_) {
        gcAllocsSinceLastCollect_++;
        if (gcAllocsSinceLastCollect_ >= gcThreshold_) {
            collectGarbage();
        }
    }

    auto tryAlloc = [&]() -> int64_t {
        for (size_t idx = 0; idx < heapBlocks_.size(); ++idx) {
            if (!heapBlocks_[idx].used && heapBlocks_[idx].size >= size) {
                uint64_t addr = heapBlocks_[idx].address;
                // Split block if significantly larger
                if (heapBlocks_[idx].size > size + 4) {
                    HeapBlock newBlock{addr + size, heapBlocks_[idx].size - size, false, false};
                    heapBlocks_[idx].size = size;
                    heapBlocks_[idx].used = true;
                    heapBlocks_.insert(heapBlocks_.begin() + static_cast<long>(idx + 1), newBlock);
                } else {
                    heapBlocks_[idx].used = true;
                }
                stats_.memoryAllocations++;
                return static_cast<int64_t>(addr);
            }
        }
        return -1;
    };

    int64_t result = tryAlloc();
    if (result >= 0) return static_cast<uint64_t>(result);

    // No free block found — try GC then retry
    if (gcEnabled_) {
        collectGarbage();
        result = tryAlloc();
        if (result >= 0) return static_cast<uint64_t>(result);
    }

    throw std::runtime_error("Heap allocation failed: out of memory (requested " +
                             std::to_string(size) + ")");
}

void VirtualMachine::heapFree(uint64_t addr) {
    for (auto& block : heapBlocks_) {
        if (block.address == addr && block.used) {
            block.used = false;
            // Clear the memory
            for (uint64_t i = 0; i < block.size; ++i)
                heap_[block.address + i] = Value();

            // Coalesce adjacent free blocks
            for (size_t i = 0; i + 1 < heapBlocks_.size(); ) {
                if (!heapBlocks_[i].used && !heapBlocks_[i + 1].used) {
                    heapBlocks_[i].size += heapBlocks_[i + 1].size;
                    heapBlocks_.erase(heapBlocks_.begin() + static_cast<long>(i + 1));
                } else {
                    ++i;
                }
            }
            stats_.memoryFrees++;
            return;
        }
    }
    throw std::runtime_error("Heap free: invalid address " + std::to_string(addr));
}

uint64_t VirtualMachine::heapRealloc(uint64_t addr, uint64_t newSize) {
    // Find old block
    uint64_t oldSize = 0;
    for (auto& block : heapBlocks_) {
        if (block.address == addr && block.used) {
            oldSize = block.size;
            break;
        }
    }
    if (oldSize == 0)
        throw std::runtime_error("Heap realloc: invalid address");

    uint64_t newAddr = heapAlloc(newSize);
    uint64_t copySize = std::min(oldSize, newSize);
    for (uint64_t i = 0; i < copySize; ++i)
        heap_[newAddr + i] = heap_[addr + i];
    heapFree(addr);
    return newAddr;
}

// ============================================================================
// Flag Management
// ============================================================================

void VirtualMachine::setFlags(int64_t result) {
    flags_ = 0;
    if (result == 0) flags_ |= FLAG_ZERO;
    if (result < 0)  flags_ |= FLAG_NEGATIVE;
}

void VirtualMachine::setFlagsFloat(double result) {
    flags_ = 0;
    if (result == 0.0) flags_ |= FLAG_ZERO;
    if (result < 0.0)  flags_ |= FLAG_NEGATIVE;
}

bool VirtualMachine::checkFlag(uint8_t flag) const {
    return (flags_ & flag) != 0;
}

// ============================================================================
// Exception Handling
// ============================================================================

void VirtualMachine::throwException(int64_t errorCode) {
    stats_.exceptionsThrown++;
    if (exceptionHandlers_.empty()) {
        throw std::runtime_error("Unhandled VM exception: error code " +
                                 std::to_string(errorCode));
    }

    auto handler = exceptionHandlers_.back();
    exceptionHandlers_.pop_back();

    // Unwind stack
    sp_ = handler.stackDepth;
    registers_[RSP] = Value(static_cast<int64_t>(sp_));
    registers_[RFP] = Value(static_cast<int64_t>(handler.framePointer));

    // Put error code in R0
    registers_[R0] = Value(errorCode);

    // Jump to handler
    pc_ = handler.handlerAddress;
}

// ============================================================================
// Tracing
// ============================================================================

void VirtualMachine::traceInstruction(const Instruction& instr) const {
    std::cerr << "[TRACE] PC=" << std::setw(4) << std::setfill('0') << pc_
              << " | " << std::setw(10) << std::left << opcodeName(instr.opcode)
              << " " << instr.op1 << ", " << instr.op2 << ", " << instr.op3
              << "  | SP=" << sp_ << " FLAGS=" << std::hex
              << static_cast<int>(flags_) << std::dec;
    if (sp_ > 0) std::cerr << " TOS=" << stack_[sp_ - 1];
    std::cerr << std::endl;
}

// ============================================================================
// Execution
// ============================================================================

VirtualMachine::ExecResult VirtualMachine::run() {
    running_ = true;
    while (running_) {
        auto result = step();
        if (result != ExecResult::OK)
            return result;
    }
    return ExecResult::HALTED;
}

VirtualMachine::ExecResult VirtualMachine::step() {
    if (pc_ >= program_.size()) {
        running_ = false;
        return ExecResult::HALTED;
    }

    const Instruction& instr = program_[pc_];
    if (tracing_) traceInstruction(instr);

    stats_.instructionsExecuted++;
    pc_++;

    return dispatch(instr);
}

// ============================================================================
// Instruction Dispatch
// ============================================================================

VirtualMachine::ExecResult VirtualMachine::dispatch(const Instruction& instr) {
    try {
        switch (instr.opcode) {

        // ---- System ----
        case OpCode::NOP:
            break;

        case OpCode::HALT:
            running_ = false;
            return ExecResult::HALTED;

        case OpCode::SYSCALL: {
            auto it = syscalls_.find(instr.op1);
            if (it == syscalls_.end())
                throw std::runtime_error("Unknown syscall: " + std::to_string(instr.op1));
            it->second(*this);
            break;
        }

        case OpCode::DEBUG:
            std::cerr << "[DEBUG] R" << instr.op1 << " = "
                      << registers_[instr.op1] << std::endl;
            break;

        case OpCode::BREAKPOINT:
            return ExecResult::BREAKPOINT;

        // ---- Data Movement ----
        case OpCode::MOV:
            registers_[instr.op1] = registers_[instr.op2];
            break;

        case OpCode::MOVI:
            registers_[instr.op1] = constantPool_.get(instr.op2);
            break;

        case OpCode::LOAD_CONST:
            registers_[instr.op1] = constantPool_.get(instr.op2);
            break;

        case OpCode::LOAD: {
            uint64_t addr = registers_[instr.op2].asInt();
            registers_[instr.op1] = heap_[addr];
            break;
        }

        case OpCode::STORE: {
            uint64_t addr = registers_[instr.op1].asInt();
            heap_[addr] = registers_[instr.op2];
            break;
        }

        case OpCode::LEA: {
            int64_t addr = registers_[instr.op2].asInt() + static_cast<int16_t>(instr.op3);
            registers_[instr.op1] = Value(addr);
            break;
        }

        case OpCode::PUSH:
            push(registers_[instr.op1]);
            break;

        case OpCode::POP:
            registers_[instr.op1] = pop();
            break;

        case OpCode::XCHG: {
            Value tmp = registers_[instr.op1];
            registers_[instr.op1] = registers_[instr.op2];
            registers_[instr.op2] = tmp;
            break;
        }

        // ---- Integer Arithmetic ----
        case OpCode::ADD: {
            int64_t a = registers_[instr.op2].asInt();
            int64_t b = registers_[instr.op3].asInt();
            int64_t result = a + b;
            registers_[instr.op1] = Value(result);
            setFlags(result);
            // Overflow detection
            if ((a > 0 && b > 0 && result < 0) || (a < 0 && b < 0 && result > 0))
                flags_ |= FLAG_OVERFLOW;
            break;
        }

        case OpCode::SUB: {
            int64_t a = registers_[instr.op2].asInt();
            int64_t b = registers_[instr.op3].asInt();
            int64_t result = a - b;
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::MUL: {
            int64_t a = registers_[instr.op2].asInt();
            int64_t b = registers_[instr.op3].asInt();
            int64_t result = a * b;
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::DIV: {
            int64_t b = registers_[instr.op3].asInt();
            if (b == 0) {
                if (!exceptionHandlers_.empty()) {
                    throwException(1); // error code 1 = division by zero
                    break;
                }
                throw std::runtime_error("Division by zero");
            }
            int64_t a = registers_[instr.op2].asInt();
            int64_t result = a / b;
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::MOD: {
            int64_t b = registers_[instr.op3].asInt();
            if (b == 0) {
                if (!exceptionHandlers_.empty()) {
                    throwException(2); // error code 2 = modulo by zero
                    break;
                }
                throw std::runtime_error("Modulo by zero");
            }
            int64_t a = registers_[instr.op2].asInt();
            int64_t result = a % b;
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::INC: {
            int64_t val = registers_[instr.op1].asInt() + 1;
            registers_[instr.op1] = Value(val);
            setFlags(val);
            break;
        }

        case OpCode::DEC: {
            int64_t val = registers_[instr.op1].asInt() - 1;
            registers_[instr.op1] = Value(val);
            setFlags(val);
            break;
        }

        case OpCode::NEG: {
            int64_t val = -registers_[instr.op1].asInt();
            registers_[instr.op1] = Value(val);
            setFlags(val);
            break;
        }

        case OpCode::ABS: {
            int64_t val = std::abs(registers_[instr.op2].asInt());
            registers_[instr.op1] = Value(val);
            setFlags(val);
            break;
        }

        // ---- Floating Point Arithmetic ----
        case OpCode::FADD: {
            double result = registers_[instr.op2].asFloat() + registers_[instr.op3].asFloat();
            registers_[instr.op1] = Value(result);
            setFlagsFloat(result);
            break;
        }

        case OpCode::FSUB: {
            double result = registers_[instr.op2].asFloat() - registers_[instr.op3].asFloat();
            registers_[instr.op1] = Value(result);
            setFlagsFloat(result);
            break;
        }

        case OpCode::FMUL: {
            double result = registers_[instr.op2].asFloat() * registers_[instr.op3].asFloat();
            registers_[instr.op1] = Value(result);
            setFlagsFloat(result);
            break;
        }

        case OpCode::FDIV: {
            double b = registers_[instr.op3].asFloat();
            if (b == 0.0) throw std::runtime_error("Float division by zero");
            double result = registers_[instr.op2].asFloat() / b;
            registers_[instr.op1] = Value(result);
            setFlagsFloat(result);
            break;
        }

        case OpCode::FMOD: {
            double b = registers_[instr.op3].asFloat();
            if (b == 0.0) throw std::runtime_error("Float modulo by zero");
            double result = std::fmod(registers_[instr.op2].asFloat(), b);
            registers_[instr.op1] = Value(result);
            setFlagsFloat(result);
            break;
        }

        case OpCode::FNEG: {
            double val = -registers_[instr.op1].asFloat();
            registers_[instr.op1] = Value(val);
            setFlagsFloat(val);
            break;
        }

        case OpCode::FABS: {
            double val = std::fabs(registers_[instr.op2].asFloat());
            registers_[instr.op1] = Value(val);
            setFlagsFloat(val);
            break;
        }

        case OpCode::FSQRT: {
            double val = std::sqrt(registers_[instr.op2].asFloat());
            registers_[instr.op1] = Value(val);
            setFlagsFloat(val);
            break;
        }

        case OpCode::FSIN: {
            double val = std::sin(registers_[instr.op2].asFloat());
            registers_[instr.op1] = Value(val);
            setFlagsFloat(val);
            break;
        }

        case OpCode::FCOS: {
            double val = std::cos(registers_[instr.op2].asFloat());
            registers_[instr.op1] = Value(val);
            setFlagsFloat(val);
            break;
        }

        case OpCode::FPOW: {
            double result = std::pow(registers_[instr.op2].asFloat(),
                                     registers_[instr.op3].asFloat());
            registers_[instr.op1] = Value(result);
            setFlagsFloat(result);
            break;
        }

        // ---- Type Conversion ----
        case OpCode::ITOF: {
            double val = static_cast<double>(registers_[instr.op2].asInt());
            registers_[instr.op1] = Value(val);
            break;
        }

        case OpCode::FTOI: {
            int64_t val = static_cast<int64_t>(registers_[instr.op2].asFloat());
            registers_[instr.op1] = Value(val);
            break;
        }

        case OpCode::ITOS: {
            std::string s = std::to_string(registers_[instr.op2].asInt());
            registers_[instr.op1] = Value(s);
            break;
        }

        case OpCode::FTOS: {
            std::string s = std::to_string(registers_[instr.op2].asFloat());
            registers_[instr.op1] = Value(s);
            break;
        }

        // ---- Bitwise ----
        case OpCode::AND: {
            int64_t result = registers_[instr.op2].asInt() & registers_[instr.op3].asInt();
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::OR: {
            int64_t result = registers_[instr.op2].asInt() | registers_[instr.op3].asInt();
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::XOR: {
            int64_t result = registers_[instr.op2].asInt() ^ registers_[instr.op3].asInt();
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::NOT: {
            int64_t result = ~registers_[instr.op2].asInt();
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::SHL: {
            int64_t result = registers_[instr.op2].asInt() << registers_[instr.op3].asInt();
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::SHR: {
            uint64_t val = static_cast<uint64_t>(registers_[instr.op2].asInt());
            uint64_t shift = static_cast<uint64_t>(registers_[instr.op3].asInt());
            int64_t result = static_cast<int64_t>(val >> shift);
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::SAR: {
            int64_t result = registers_[instr.op2].asInt() >> registers_[instr.op3].asInt();
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::ROL: {
            uint64_t val = static_cast<uint64_t>(registers_[instr.op2].asInt());
            uint64_t shift = static_cast<uint64_t>(registers_[instr.op3].asInt()) % 64;
            int64_t result = static_cast<int64_t>((val << shift) | (val >> (64 - shift)));
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        case OpCode::ROR: {
            uint64_t val = static_cast<uint64_t>(registers_[instr.op2].asInt());
            uint64_t shift = static_cast<uint64_t>(registers_[instr.op3].asInt()) % 64;
            int64_t result = static_cast<int64_t>((val >> shift) | (val << (64 - shift)));
            registers_[instr.op1] = Value(result);
            setFlags(result);
            break;
        }

        // ---- Comparison ----
        case OpCode::CMP: {
            int64_t a = registers_[instr.op1].asInt();
            int64_t b = registers_[instr.op2].asInt();
            setFlags(a - b);
            break;
        }

        case OpCode::FCMP: {
            double a = registers_[instr.op1].asFloat();
            double b = registers_[instr.op2].asFloat();
            setFlagsFloat(a - b);
            break;
        }

        case OpCode::TEST: {
            int64_t result = registers_[instr.op1].asInt() & registers_[instr.op2].asInt();
            setFlags(result);
            break;
        }

        // ---- Control Flow ----
        case OpCode::JMP:
            pc_ = instr.op1;
            break;

        case OpCode::JZ:
            if (checkFlag(FLAG_ZERO)) pc_ = instr.op1;
            break;

        case OpCode::JNZ:
            if (!checkFlag(FLAG_ZERO)) pc_ = instr.op1;
            break;

        case OpCode::JG:
            if (!checkFlag(FLAG_ZERO) && !checkFlag(FLAG_NEGATIVE)) pc_ = instr.op1;
            break;

        case OpCode::JL:
            if (checkFlag(FLAG_NEGATIVE)) pc_ = instr.op1;
            break;

        case OpCode::JGE:
            if (!checkFlag(FLAG_NEGATIVE)) pc_ = instr.op1;
            break;

        case OpCode::JLE:
            if (checkFlag(FLAG_ZERO) || checkFlag(FLAG_NEGATIVE)) pc_ = instr.op1;
            break;

        case OpCode::JO:
            if (checkFlag(FLAG_OVERFLOW)) pc_ = instr.op1;
            break;

        case OpCode::JNO:
            if (!checkFlag(FLAG_OVERFLOW)) pc_ = instr.op1;
            break;

        case OpCode::LOOP: {
            int64_t counter = registers_[instr.op2].asInt();
            counter--;
            registers_[instr.op2] = Value(counter);
            if (counter != 0) pc_ = instr.op1;
            break;
        }

        // ---- Function Calls ----
        case OpCode::CALL: {
            if (callStack_.size() >= MAX_CALL_DEPTH)
                throw std::runtime_error("Call stack overflow (max depth " +
                                         std::to_string(MAX_CALL_DEPTH) + ")");
            CallFrame frame;
            frame.returnAddress = pc_;
            frame.savedFramePointer = static_cast<uint64_t>(registers_[RFP].asInt());
            frame.savedRegisters = 0;
            callStack_.push_back(frame);
            stats_.functionCalls++;
            pc_ = instr.op1;
            break;
        }

        case OpCode::RET: {
            if (callStack_.empty())
                throw std::runtime_error("Return without matching call");
            CallFrame frame = callStack_.back();
            callStack_.pop_back();
            pc_ = frame.returnAddress;
            registers_[RFP] = Value(static_cast<int64_t>(frame.savedFramePointer));
            break;
        }

        case OpCode::ENTER: {
            // Set up stack frame: push old FP, set FP = SP, reserve locals
            push(registers_[RFP]);
            registers_[RFP] = Value(static_cast<int64_t>(sp_));
            // Reserve space for local variables
            uint16_t numLocals = instr.op1;
            for (uint16_t i = 0; i < numLocals; ++i)
                push(Value(static_cast<int64_t>(0)));
            break;
        }

        case OpCode::LEAVE: {
            // Tear down stack frame: SP = FP, pop FP
            sp_ = static_cast<size_t>(registers_[RFP].asInt());
            registers_[RSP] = Value(static_cast<int64_t>(sp_));
            registers_[RFP] = pop();
            break;
        }

        // ---- Stack Operations ----
        case OpCode::DUP:
            push(peek(0));
            break;

        case OpCode::SWAP: {
            if (sp_ < 2) throw std::runtime_error("SWAP: insufficient stack depth");
            std::swap(stack_[sp_ - 1], stack_[sp_ - 2]);
            break;
        }

        case OpCode::ROT: {
            if (sp_ < 3) throw std::runtime_error("ROT: insufficient stack depth");
            Value tmp = stack_[sp_ - 3];
            stack_[sp_ - 3] = stack_[sp_ - 2];
            stack_[sp_ - 2] = stack_[sp_ - 1];
            stack_[sp_ - 1] = tmp;
            break;
        }

        case OpCode::DEPTH:
            push(Value(static_cast<int64_t>(sp_)));
            break;

        case OpCode::PICK: {
            uint16_t n = instr.op1;
            push(peek(n));
            break;
        }

        // ---- Heap Memory ----
        case OpCode::ALLOC: {
            uint64_t size = static_cast<uint64_t>(registers_[instr.op2].asInt());
            uint64_t addr = heapAlloc(size);
            registers_[instr.op1] = Value::makePtr(addr);
            break;
        }

        case OpCode::FREE: {
            uint64_t addr = registers_[instr.op1].ptr;
            heapFree(addr);
            registers_[instr.op1] = Value();
            break;
        }

        case OpCode::REALLOC: {
            uint64_t oldAddr = registers_[instr.op2].ptr;
            uint64_t newSize = static_cast<uint64_t>(registers_[instr.op3].asInt());
            uint64_t newAddr = heapRealloc(oldAddr, newSize);
            registers_[instr.op1] = Value::makePtr(newAddr);
            break;
        }

        case OpCode::MEMCPY: {
            uint64_t dst = registers_[instr.op1].ptr;
            uint64_t src = registers_[instr.op2].ptr;
            uint64_t size = static_cast<uint64_t>(registers_[instr.op3].asInt());
            for (uint64_t i = 0; i < size; ++i)
                heap_[dst + i] = heap_[src + i];
            break;
        }

        case OpCode::MEMSET: {
            uint64_t addr = registers_[instr.op1].ptr;
            Value val = registers_[instr.op2];
            uint64_t size = static_cast<uint64_t>(registers_[instr.op3].asInt());
            for (uint64_t i = 0; i < size; ++i)
                heap_[addr + i] = val;
            break;
        }

        case OpCode::LOADH: {
            uint64_t base = registers_[instr.op2].ptr;
            uint64_t offset = static_cast<uint64_t>(instr.op3);
            registers_[instr.op1] = heap_[base + offset];
            break;
        }

        case OpCode::STOREH: {
            uint64_t base = registers_[instr.op1].ptr;
            uint64_t offset = static_cast<uint64_t>(instr.op2);
            heap_[base + offset] = registers_[instr.op3];
            break;
        }

        // ---- I/O ----
        case OpCode::PRINT:
            std::cout << registers_[instr.op1];
            break;

        case OpCode::PRINTLN:
            std::cout << registers_[instr.op1] << std::endl;
            break;

        case OpCode::PRINTS:
            std::cout << constantPool_.get(instr.op1).asString();
            break;

        case OpCode::INPUT: {
            int64_t val;
            std::cin >> val;
            registers_[instr.op1] = Value(val);
            break;
        }

        case OpCode::INPUTF: {
            double val;
            std::cin >> val;
            registers_[instr.op1] = Value(val);
            break;
        }

        case OpCode::INPUTS: {
            std::string val;
            std::getline(std::cin, val);
            registers_[instr.op1] = Value(val);
            break;
        }

        // ---- Exception Handling ----
        case OpCode::TRY: {
            ExceptionHandler handler;
            handler.handlerAddress = instr.op1;
            handler.stackDepth = sp_;
            handler.framePointer = static_cast<uint64_t>(registers_[RFP].asInt());
            exceptionHandlers_.push_back(handler);
            break;
        }

        case OpCode::ENDTRY:
            if (!exceptionHandlers_.empty())
                exceptionHandlers_.pop_back();
            break;

        case OpCode::THROW: {
            int64_t errorCode = registers_[instr.op1].asInt();
            throwException(errorCode);
            break;
        }

        // ---- Array Operations ----
        case OpCode::ANEW: {
            uint64_t size = static_cast<uint64_t>(registers_[instr.op2].asInt());
            uint64_t id = nextArrayId_++;
            arrays_.emplace(id, ManagedArray(size));
            registers_[instr.op1] = Value::makeArray(id);
            break;
        }

        case OpCode::AGET: {
            uint64_t id = registers_[instr.op2].ptr;
            int64_t idx = registers_[instr.op3].asInt();
            auto& arr = getArray(id);
            if (idx < 0 || static_cast<size_t>(idx) >= arr.elements.size())
                throw std::runtime_error("Array index out of bounds: " + std::to_string(idx));
            registers_[instr.op1] = arr.elements[static_cast<size_t>(idx)];
            break;
        }

        case OpCode::ASET: {
            uint64_t id = registers_[instr.op1].ptr;
            int64_t idx = registers_[instr.op2].asInt();
            auto& arr = getArray(id);
            if (idx < 0 || static_cast<size_t>(idx) >= arr.elements.size())
                throw std::runtime_error("Array index out of bounds: " + std::to_string(idx));
            arr.elements[static_cast<size_t>(idx)] = registers_[instr.op3];
            break;
        }

        case OpCode::ALEN: {
            uint64_t id = registers_[instr.op2].ptr;
            auto& arr = getArray(id);
            registers_[instr.op1] = Value(static_cast<int64_t>(arr.elements.size()));
            break;
        }

        // ---- Debug / Extended ----
        case OpCode::DUMP_REGS: {
            std::cerr << "=== Register Dump ===" << std::endl;
            const char* names[] = {
                "R0","R1","R2","R3","R4","R5","R6","R7",
                "R8","R9","R10","R11","R12","R13","R14","R15",
                "RSP","RFP","RPC","RFLAGS","RRV"
            };
            for (int i = 0; i < Register::NUM_REGISTERS; ++i) {
                std::cerr << "  " << std::setw(6) << std::left << names[i]
                          << " = " << registers_[i] << std::endl;
            }
            std::cerr << "  FLAGS  = 0x" << std::hex << static_cast<int>(flags_)
                      << std::dec << " [";
            if (flags_ & FLAG_ZERO)     std::cerr << "Z";
            if (flags_ & FLAG_NEGATIVE) std::cerr << "N";
            if (flags_ & FLAG_OVERFLOW) std::cerr << "O";
            if (flags_ & FLAG_CARRY)    std::cerr << "C";
            std::cerr << "]" << std::endl;
            break;
        }

        case OpCode::DUMP_STACK: {
            std::cerr << "=== Stack Dump (depth=" << sp_ << ") ===" << std::endl;
            for (size_t i = sp_; i > 0; --i) {
                std::cerr << "  [" << std::setw(4) << (i - 1) << "] "
                          << stack_[i - 1] << std::endl;
            }
            break;
        }

        case OpCode::DUMP_HEAP: {
            std::cerr << "=== Heap Info ===" << std::endl;
            for (const auto& block : heapBlocks_) {
                std::cerr << "  addr=" << block.address
                          << " size=" << block.size
                          << " " << (block.used ? "USED" : "FREE") << std::endl;
            }
            break;
        }

        case OpCode::TRACE_ON:
            tracing_ = true;
            break;

        case OpCode::TRACE_OFF:
            tracing_ = false;
            break;

        // ---- Garbage Collection ----
        case OpCode::GC_COLLECT:
            collectGarbage();
            break;

        case OpCode::GC_ON:
            gcEnabled_ = true;
            break;

        case OpCode::GC_OFF:
            gcEnabled_ = false;
            break;

        case OpCode::GC_STATS:
            std::cout << "[GC Stats] collections=" << stats_.gcCollections
                      << " objects_freed=" << stats_.gcObjectsFreed
                      << " arrays_freed=" << stats_.gcArraysFreed
                      << " allocs_since_last=" << gcAllocsSinceLastCollect_
                      << " heap_blocks=" << heapBlocks_.size()
                      << " arrays=" << arrays_.size()
                      << std::endl;
            break;

        default:
            throw std::runtime_error("Unknown opcode: 0x" +
                [](uint8_t v) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%02X", v);
                    return std::string(buf);
                }(static_cast<uint8_t>(instr.opcode)));
        }
    } catch (const std::exception& e) {
        std::cerr << "[VM ERROR] PC=" << (pc_ - 1)
                  << " Opcode=" << opcodeName(instr.opcode)
                  << " : " << e.what() << std::endl;
        running_ = false;
        return ExecResult::ERROR;
    }

    return ExecResult::OK;
}

// ============================================================================
// Garbage Collection (Mark-and-Sweep)
// ============================================================================

void VirtualMachine::gcMarkValue(const Value& val,
                                  std::vector<uint64_t>& heapWorklist,
                                  std::unordered_map<uint64_t, bool>& markedArrays) {
    if (val.type == ValueType::HeapPtr) {
        heapWorklist.push_back(val.ptr);
    } else if (val.type == ValueType::ArrayRef) {
        uint64_t id = val.ptr;
        if (markedArrays.find(id) == markedArrays.end()) {
            markedArrays[id] = true;
            // Scan array elements for more references
            auto it = arrays_.find(id);
            if (it != arrays_.end()) {
                for (const auto& elem : it->second.elements) {
                    gcMarkValue(elem, heapWorklist, markedArrays);
                }
            }
        }
    }
}

void VirtualMachine::gcMarkHeapBlock(uint64_t addr,
                                      std::vector<uint64_t>& heapWorklist,
                                      std::unordered_map<uint64_t, bool>& markedArrays) {
    for (auto& block : heapBlocks_) {
        if (block.address == addr && block.used && !block.marked) {
            block.marked = true;
            // Scan all values in this block for embedded pointers
            for (uint64_t i = 0; i < block.size; ++i) {
                gcMarkValue(heap_[block.address + i], heapWorklist, markedArrays);
            }
            return;
        }
    }
}

void VirtualMachine::gcMark() {
    std::vector<uint64_t> heapWorklist;
    std::unordered_map<uint64_t, bool> markedArrays;

    // Root set: scan all registers
    for (int i = 0; i < Register::NUM_REGISTERS; ++i) {
        gcMarkValue(registers_[i], heapWorklist, markedArrays);
    }

    // Root set: scan the stack
    for (size_t i = 0; i < sp_; ++i) {
        gcMarkValue(stack_[i], heapWorklist, markedArrays);
    }

    // Process worklist (iterative, not recursive)
    while (!heapWorklist.empty()) {
        uint64_t addr = heapWorklist.back();
        heapWorklist.pop_back();
        gcMarkHeapBlock(addr, heapWorklist, markedArrays);
    }

    // Pass markedArrays to sweep
    gcSweep(markedArrays);
}

void VirtualMachine::gcSweep(std::unordered_map<uint64_t, bool>& markedArrays) {
    // Sweep heap blocks: free unmarked used blocks
    for (auto& block : heapBlocks_) {
        if (block.used && !block.marked) {
            // Free this block
            for (uint64_t i = 0; i < block.size; ++i)
                heap_[block.address + i] = Value();
            block.used = false;
            stats_.gcObjectsFreed++;
            stats_.memoryFrees++;
        }
        // Clear mark for next cycle
        block.marked = false;
    }

    // Sweep arrays: erase unmarked arrays
    for (auto it = arrays_.begin(); it != arrays_.end(); ) {
        if (markedArrays.find(it->first) == markedArrays.end()) {
            it = arrays_.erase(it);
            stats_.gcArraysFreed++;
        } else {
            ++it;
        }
    }

    // Coalesce adjacent free blocks
    for (size_t i = 0; i + 1 < heapBlocks_.size(); ) {
        if (!heapBlocks_[i].used && !heapBlocks_[i + 1].used) {
            heapBlocks_[i].size += heapBlocks_[i + 1].size;
            heapBlocks_.erase(heapBlocks_.begin() + static_cast<long>(i + 1));
        } else {
            ++i;
        }
    }
}

void VirtualMachine::collectGarbage() {
    stats_.gcCollections++;
    gcAllocsSinceLastCollect_ = 0;
    gcMark();
    // gcSweep is called from gcMark after marking is complete
}
