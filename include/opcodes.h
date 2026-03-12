#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

// ============================================================================
// Instruction encoding:
//   [opcode:8][flags:8][operand1:16][operand2:16][operand3:16]
//   Total: 8 bytes per instruction
//
// Operand modes (encoded in flags):
//   00 = register
//   01 = immediate value (index into constant pool)
//   10 = memory address (register indirect)
//   11 = memory address (register indirect + offset)
// ============================================================================

enum class OpCode : uint8_t {
  // --- System ---
  NOP = 0x00,
  HALT = 0x01,
  SYSCALL = 0x02,
  DEBUG = 0x03,
  BREAKPOINT = 0x04,

  // --- Data Movement ---
  MOV = 0x10,         // MOV  dst, src
  LOAD = 0x11,        // LOAD dst, [addr]       (load from memory)
  STORE = 0x12,       // STORE [addr], src       (store to memory)
  LEA = 0x13,         // LEA  dst, [addr+offset]
  PUSH = 0x14,        // PUSH src
  POP = 0x15,         // POP  dst
  XCHG = 0x16,        // XCHG r1, r2
  MOVI = 0x17,        // MOV  dst, immediate (constant pool index)
  LOAD_CONST = 0x18,  // Load a constant from pool into register

  // --- Integer Arithmetic ---
  ADD = 0x20,  // ADD dst, src1, src2
  SUB = 0x21,  // SUB dst, src1, src2
  MUL = 0x22,  // MUL dst, src1, src2
  DIV = 0x23,  // DIV dst, src1, src2
  MOD = 0x24,  // MOD dst, src1, src2
  INC = 0x25,  // INC dst
  DEC = 0x26,  // DEC dst
  NEG = 0x27,  // NEG dst (two's complement)
  ABS = 0x28,  // ABS dst, src

  // --- Floating Point Arithmetic ---
  FADD = 0x30,  // FADD dst, src1, src2
  FSUB = 0x31,
  FMUL = 0x32,
  FDIV = 0x33,
  FMOD = 0x34,
  FNEG = 0x35,
  FABS = 0x36,
  FSQRT = 0x37,
  FSIN = 0x38,
  FCOS = 0x39,
  FPOW = 0x3A,

  // --- Type Conversion ---
  ITOF = 0x40,  // Int to Float
  FTOI = 0x41,  // Float to Int
  ITOS = 0x42,  // Int to String
  FTOS = 0x43,  // Float to String

  // --- Bitwise ---
  AND = 0x50,
  OR = 0x51,
  XOR = 0x52,
  NOT = 0x53,
  SHL = 0x54,
  SHR = 0x55,
  SAR = 0x56,  // Arithmetic shift right
  ROL = 0x57,  // Rotate left
  ROR = 0x58,  // Rotate right

  // --- Comparison ---
  CMP = 0x60,   // CMP src1, src2 (sets flags)
  FCMP = 0x61,  // FCMP src1, src2 (float compare)
  TEST = 0x62,  // TEST src1, src2 (AND without storing)

  // --- Control Flow ---
  JMP = 0x70,   // Unconditional jump
  JZ = 0x71,    // Jump if zero
  JNZ = 0x72,   // Jump if not zero
  JG = 0x73,    // Jump if greater
  JL = 0x74,    // Jump if less
  JGE = 0x75,   // Jump if greater or equal
  JLE = 0x76,   // Jump if less or equal
  JO = 0x77,    // Jump if overflow
  JNO = 0x78,   // Jump if not overflow
  LOOP = 0x79,  // Decrement counter register, jump if != 0

  // --- Function Calls ---
  CALL = 0x80,   // CALL address
  RET = 0x81,    // Return from function
  ENTER = 0x82,  // Set up stack frame (push FP, mov FP SP, sub SP locals)
  LEAVE = 0x83,  // Tear down stack frame

  // --- Stack Operations ---
  DUP = 0x90,    // Duplicate top of stack
  SWAP = 0x91,   // Swap top two stack elements
  ROT = 0x92,    // Rotate top 3 stack elements
  DEPTH = 0x93,  // Push stack depth
  PICK = 0x94,   // Copy nth stack element to top

  // --- Heap Memory ---
  ALLOC = 0xA0,    // ALLOC dst, size (allocate heap memory, return ptr in dst)
  FREE = 0xA1,     // FREE ptr
  REALLOC = 0xA2,  // REALLOC dst, ptr, new_size
  MEMCPY = 0xA3,   // MEMCPY dst_ptr, src_ptr, size
  MEMSET = 0xA4,   // MEMSET ptr, value, size
  LOADH = 0xA5,    // Load from heap: LOADH dst, base_reg, offset
  STOREH = 0xA6,   // Store to heap: STOREH base_reg, offset, src

  // --- I/O ---
  PRINT = 0xB0,    // Print register value
  PRINTLN = 0xB1,  // Print register value + newline
  PRINTS = 0xB2,   // Print string from constant pool
  INPUT = 0xB3,    // Read integer input into register
  INPUTF = 0xB4,   // Read float input
  INPUTS = 0xB5,   // Read string input

  // --- Exception Handling ---
  TRY = 0xC0,     // TRY handler_addr (push exception handler)
  ENDTRY = 0xC1,  // Pop exception handler
  THROW = 0xC2,   // Throw exception (error code in register)

  // --- Array Operations ---
  ANEW = 0xD0,  // ANEW dst, size (allocate array)
  AGET = 0xD1,  // AGET dst, array_reg, index_reg
  ASET = 0xD2,  // ASET array_reg, index_reg, value_reg
  ALEN = 0xD3,  // ALEN dst, array_reg

  // --- Garbage Collection ---
  GC_COLLECT = 0xE0,  // Force garbage collection
  GC_ON = 0xE1,       // Enable automatic GC
  GC_OFF = 0xE2,      // Disable automatic GC
  GC_STATS = 0xE3,    // Print GC statistics

  // --- Extended ---
  DUMP_REGS = 0xF0,   // Debug: dump all registers
  DUMP_STACK = 0xF1,  // Debug: dump stack
  DUMP_HEAP = 0xF2,   // Debug: dump heap info
  TRACE_ON = 0xF3,    // Enable instruction tracing
  TRACE_OFF = 0xF4,   // Disable instruction tracing
};

// Instruction flags for operand modes
enum class OpFlag : uint8_t {
  NONE = 0x00,
  IMM_OP2 = 0x01,   // Operand 2 is immediate/constant pool index
  IMM_OP3 = 0x02,   // Operand 3 is immediate
  INDIRECT = 0x04,  // Memory indirect addressing
  OFFSET = 0x08,    // Include offset in addressing
};

inline OpFlag operator|(OpFlag a, OpFlag b) {
  return static_cast<OpFlag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline bool operator&(OpFlag a, OpFlag b) {
  return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

// CPU Flags
enum CpuFlag : uint8_t {
  FLAG_ZERO = 0x01,
  FLAG_NEGATIVE = 0x02,
  FLAG_OVERFLOW = 0x04,
  FLAG_CARRY = 0x08,
};

// Named registers
enum Register : uint16_t {
  R0 = 0,
  R1,
  R2,
  R3,
  R4,
  R5,
  R6,
  R7,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,
  // Special-purpose
  RSP = 16,     // Stack pointer
  RFP = 17,     // Frame pointer
  RPC = 18,     // Program counter (read-only from instructions)
  RFLAGS = 19,  // Flags register
  RRV = 20,     // Return value register
  NUM_REGISTERS = 21,
};

struct Instruction {
  OpCode opcode;
  OpFlag flags;
  uint16_t op1;
  uint16_t op2;
  uint16_t op3;
};
static_assert(sizeof(Instruction) == 8, "Instruction must be 8 bytes");

// Map opcode names for the assembler
inline const std::unordered_map<std::string, OpCode>& getOpcodeMap() {
  static const std::unordered_map<std::string, OpCode> map = {
      {"nop", OpCode::NOP},
      {"halt", OpCode::HALT},
      {"syscall", OpCode::SYSCALL},
      {"debug", OpCode::DEBUG},
      {"breakpoint", OpCode::BREAKPOINT},
      {"mov", OpCode::MOV},
      {"load", OpCode::LOAD},
      {"store", OpCode::STORE},
      {"lea", OpCode::LEA},
      {"push", OpCode::PUSH},
      {"pop", OpCode::POP},
      {"xchg", OpCode::XCHG},
      {"movi", OpCode::MOVI},
      {"load_const", OpCode::LOAD_CONST},
      {"add", OpCode::ADD},
      {"sub", OpCode::SUB},
      {"mul", OpCode::MUL},
      {"div", OpCode::DIV},
      {"mod", OpCode::MOD},
      {"inc", OpCode::INC},
      {"dec", OpCode::DEC},
      {"neg", OpCode::NEG},
      {"abs", OpCode::ABS},
      {"fadd", OpCode::FADD},
      {"fsub", OpCode::FSUB},
      {"fmul", OpCode::FMUL},
      {"fdiv", OpCode::FDIV},
      {"fmod", OpCode::FMOD},
      {"fneg", OpCode::FNEG},
      {"fabs", OpCode::FABS},
      {"fsqrt", OpCode::FSQRT},
      {"fsin", OpCode::FSIN},
      {"fcos", OpCode::FCOS},
      {"fpow", OpCode::FPOW},
      {"itof", OpCode::ITOF},
      {"ftoi", OpCode::FTOI},
      {"itos", OpCode::ITOS},
      {"ftos", OpCode::FTOS},
      {"and", OpCode::AND},
      {"or", OpCode::OR},
      {"xor", OpCode::XOR},
      {"not", OpCode::NOT},
      {"shl", OpCode::SHL},
      {"shr", OpCode::SHR},
      {"sar", OpCode::SAR},
      {"rol", OpCode::ROL},
      {"ror", OpCode::ROR},
      {"cmp", OpCode::CMP},
      {"fcmp", OpCode::FCMP},
      {"test", OpCode::TEST},
      {"jmp", OpCode::JMP},
      {"jz", OpCode::JZ},
      {"jnz", OpCode::JNZ},
      {"jg", OpCode::JG},
      {"jl", OpCode::JL},
      {"jge", OpCode::JGE},
      {"jle", OpCode::JLE},
      {"jo", OpCode::JO},
      {"jno", OpCode::JNO},
      {"loop", OpCode::LOOP},
      {"call", OpCode::CALL},
      {"ret", OpCode::RET},
      {"enter", OpCode::ENTER},
      {"leave", OpCode::LEAVE},
      {"dup", OpCode::DUP},
      {"swap", OpCode::SWAP},
      {"rot", OpCode::ROT},
      {"depth", OpCode::DEPTH},
      {"pick", OpCode::PICK},
      {"alloc", OpCode::ALLOC},
      {"free", OpCode::FREE},
      {"realloc", OpCode::REALLOC},
      {"memcpy", OpCode::MEMCPY},
      {"memset", OpCode::MEMSET},
      {"loadh", OpCode::LOADH},
      {"storeh", OpCode::STOREH},
      {"print", OpCode::PRINT},
      {"println", OpCode::PRINTLN},
      {"prints", OpCode::PRINTS},
      {"input", OpCode::INPUT},
      {"inputf", OpCode::INPUTF},
      {"inputs", OpCode::INPUTS},
      {"try", OpCode::TRY},
      {"endtry", OpCode::ENDTRY},
      {"throw", OpCode::THROW},
      {"anew", OpCode::ANEW},
      {"aget", OpCode::AGET},
      {"aset", OpCode::ASET},
      {"alen", OpCode::ALEN},
      {"gc_collect", OpCode::GC_COLLECT},
      {"gc_on", OpCode::GC_ON},
      {"gc_off", OpCode::GC_OFF},
      {"gc_stats", OpCode::GC_STATS},
      {"dump_regs", OpCode::DUMP_REGS},
      {"dump_stack", OpCode::DUMP_STACK},
      {"dump_heap", OpCode::DUMP_HEAP},
      {"trace_on", OpCode::TRACE_ON},
      {"trace_off", OpCode::TRACE_OFF},
  };
  return map;
}

inline const char* opcodeName(OpCode op) {
  switch (op) {
    case OpCode::NOP:
      return "NOP";
    case OpCode::HALT:
      return "HALT";
    case OpCode::SYSCALL:
      return "SYSCALL";
    case OpCode::DEBUG:
      return "DEBUG";
    case OpCode::BREAKPOINT:
      return "BREAKPOINT";
    case OpCode::MOV:
      return "MOV";
    case OpCode::LOAD:
      return "LOAD";
    case OpCode::STORE:
      return "STORE";
    case OpCode::LEA:
      return "LEA";
    case OpCode::PUSH:
      return "PUSH";
    case OpCode::POP:
      return "POP";
    case OpCode::XCHG:
      return "XCHG";
    case OpCode::MOVI:
      return "MOVI";
    case OpCode::LOAD_CONST:
      return "LOAD_CONST";
    case OpCode::ADD:
      return "ADD";
    case OpCode::SUB:
      return "SUB";
    case OpCode::MUL:
      return "MUL";
    case OpCode::DIV:
      return "DIV";
    case OpCode::MOD:
      return "MOD";
    case OpCode::INC:
      return "INC";
    case OpCode::DEC:
      return "DEC";
    case OpCode::NEG:
      return "NEG";
    case OpCode::ABS:
      return "ABS";
    case OpCode::FADD:
      return "FADD";
    case OpCode::FSUB:
      return "FSUB";
    case OpCode::FMUL:
      return "FMUL";
    case OpCode::FDIV:
      return "FDIV";
    case OpCode::FMOD:
      return "FMOD";
    case OpCode::FNEG:
      return "FNEG";
    case OpCode::FABS:
      return "FABS";
    case OpCode::FSQRT:
      return "FSQRT";
    case OpCode::FSIN:
      return "FSIN";
    case OpCode::FCOS:
      return "FCOS";
    case OpCode::FPOW:
      return "FPOW";
    case OpCode::ITOF:
      return "ITOF";
    case OpCode::FTOI:
      return "FTOI";
    case OpCode::ITOS:
      return "ITOS";
    case OpCode::FTOS:
      return "FTOS";
    case OpCode::AND:
      return "AND";
    case OpCode::OR:
      return "OR";
    case OpCode::XOR:
      return "XOR";
    case OpCode::NOT:
      return "NOT";
    case OpCode::SHL:
      return "SHL";
    case OpCode::SHR:
      return "SHR";
    case OpCode::SAR:
      return "SAR";
    case OpCode::ROL:
      return "ROL";
    case OpCode::ROR:
      return "ROR";
    case OpCode::CMP:
      return "CMP";
    case OpCode::FCMP:
      return "FCMP";
    case OpCode::TEST:
      return "TEST";
    case OpCode::JMP:
      return "JMP";
    case OpCode::JZ:
      return "JZ";
    case OpCode::JNZ:
      return "JNZ";
    case OpCode::JG:
      return "JG";
    case OpCode::JL:
      return "JL";
    case OpCode::JGE:
      return "JGE";
    case OpCode::JLE:
      return "JLE";
    case OpCode::JO:
      return "JO";
    case OpCode::JNO:
      return "JNO";
    case OpCode::LOOP:
      return "LOOP";
    case OpCode::CALL:
      return "CALL";
    case OpCode::RET:
      return "RET";
    case OpCode::ENTER:
      return "ENTER";
    case OpCode::LEAVE:
      return "LEAVE";
    case OpCode::DUP:
      return "DUP";
    case OpCode::SWAP:
      return "SWAP";
    case OpCode::ROT:
      return "ROT";
    case OpCode::DEPTH:
      return "DEPTH";
    case OpCode::PICK:
      return "PICK";
    case OpCode::ALLOC:
      return "ALLOC";
    case OpCode::FREE:
      return "FREE";
    case OpCode::REALLOC:
      return "REALLOC";
    case OpCode::MEMCPY:
      return "MEMCPY";
    case OpCode::MEMSET:
      return "MEMSET";
    case OpCode::LOADH:
      return "LOADH";
    case OpCode::STOREH:
      return "STOREH";
    case OpCode::PRINT:
      return "PRINT";
    case OpCode::PRINTLN:
      return "PRINTLN";
    case OpCode::PRINTS:
      return "PRINTS";
    case OpCode::INPUT:
      return "INPUT";
    case OpCode::INPUTF:
      return "INPUTF";
    case OpCode::INPUTS:
      return "INPUTS";
    case OpCode::TRY:
      return "TRY";
    case OpCode::ENDTRY:
      return "ENDTRY";
    case OpCode::THROW:
      return "THROW";
    case OpCode::ANEW:
      return "ANEW";
    case OpCode::AGET:
      return "AGET";
    case OpCode::ASET:
      return "ASET";
    case OpCode::ALEN:
      return "ALEN";
    case OpCode::GC_COLLECT:
      return "GC_COLLECT";
    case OpCode::GC_ON:
      return "GC_ON";
    case OpCode::GC_OFF:
      return "GC_OFF";
    case OpCode::GC_STATS:
      return "GC_STATS";
    case OpCode::DUMP_REGS:
      return "DUMP_REGS";
    case OpCode::DUMP_STACK:
      return "DUMP_STACK";
    case OpCode::DUMP_HEAP:
      return "DUMP_HEAP";
    case OpCode::TRACE_ON:
      return "TRACE_ON";
    case OpCode::TRACE_OFF:
      return "TRACE_OFF";
    default:
      return "UNKNOWN";
  }
}
