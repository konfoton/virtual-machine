#include "assembler.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>

// ============================================================================
// Tokenizer
// ============================================================================

std::vector<Assembler::Token> Assembler::tokenize(const std::string& source) {
  std::vector<Token> tokens;
  int line = 1;
  size_t i = 0;

  while (i < source.size()) {
    char c = source[i];

    // Skip whitespace (but not newlines)
    if (c == ' ' || c == '\t' || c == '\r') {
      ++i;
      continue;
    }

    // Newline
    if (c == '\n') {
      tokens.push_back({Token::NEWLINE, "\\n", 0, 0.0, line});
      ++line;
      ++i;
      continue;
    }

    // Comment
    if (c == ';') {
      while (i < source.size() && source[i] != '\n') ++i;
      continue;
    }

    // Comma
    if (c == ',') {
      tokens.push_back({Token::COMMA, ",", 0, 0.0, line});
      ++i;
      continue;
    }

    // Directive (.const, .string, .data)
    if (c == '.') {
      ++i;
      std::string name;
      while (i < source.size() && (std::isalnum(source[i]) || source[i] == '_'))
        name += source[i++];
      tokens.push_back({Token::DIRECTIVE, name, 0, 0.0, line});
      continue;
    }

    // Immediate value: #42, #3.14, #"hello"
    if (c == '#') {
      ++i;
      if (i < source.size() && source[i] == '"') {
        // String immediate
        ++i;
        std::string str;
        while (i < source.size() && source[i] != '"') {
          if (source[i] == '\\' && i + 1 < source.size()) {
            ++i;
            switch (source[i]) {
              case 'n':
                str += '\n';
                break;
              case 't':
                str += '\t';
                break;
              case '\\':
                str += '\\';
                break;
              case '"':
                str += '"';
                break;
              default:
                str += source[i];
                break;
            }
          } else {
            str += source[i];
          }
          ++i;
        }
        if (i < source.size()) ++i;  // skip closing "
        tokens.push_back({Token::IMMEDIATE_STRING, str, 0, 0.0, line});
      } else {
        // Numeric immediate
        std::string num;
        bool isFloat = false;
        if (i < source.size() && source[i] == '-') {
          num += '-';
          ++i;
        }
        while (i < source.size() &&
               (std::isdigit(source[i]) || source[i] == '.')) {
          if (source[i] == '.') isFloat = true;
          num += source[i++];
        }
        if (isFloat) {
          double val = std::stod(num);
          tokens.push_back({Token::IMMEDIATE_FLOAT, num, 0, val, line});
        } else {
          int64_t val = std::stoll(num);
          tokens.push_back({Token::IMMEDIATE_INT, num, val, 0.0, line});
        }
      }
      continue;
    }

    // Label reference: @label_name
    if (c == '@') {
      ++i;
      std::string name;
      while (i < source.size() && (std::isalnum(source[i]) || source[i] == '_'))
        name += source[i++];
      tokens.push_back({Token::LABEL_REF, name, 0, 0.0, line});
      continue;
    }

    // Identifier (opcode, register, or label definition)
    if (std::isalpha(c) || c == '_') {
      std::string name;
      while (i < source.size() && (std::isalnum(source[i]) || source[i] == '_'))
        name += source[i++];

      // Check for label definition (ends with ':')
      if (i < source.size() && source[i] == ':') {
        ++i;
        tokens.push_back({Token::LABEL, name, 0, 0.0, line});
      }
      // Check if it's a register
      else {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.size() >= 2 && lower[0] == 'r' && std::isdigit(lower[1])) {
          tokens.push_back({Token::REGISTER, lower, 0, 0.0, line});
        } else if (lower == "rsp" || lower == "rfp" || lower == "rpc" ||
                   lower == "rflags" || lower == "rrv") {
          tokens.push_back({Token::REGISTER, lower, 0, 0.0, line});
        }
        // Check if opcode
        else {
          auto& opcodeMap = getOpcodeMap();
          if (opcodeMap.count(lower)) {
            tokens.push_back({Token::OPCODE, lower, 0, 0.0, line});
          } else {
            // Treat as opcode anyway (will fail during assembly)
            tokens.push_back({Token::OPCODE, lower, 0, 0.0, line});
          }
        }
      }
      continue;
    }

    // Skip unknown characters
    ++i;
  }

  tokens.push_back({Token::END, "", 0, 0.0, line});
  return tokens;
}

// ============================================================================
// Register Parsing
// ============================================================================

uint16_t Assembler::parseRegister(const std::string& name, int line) {
  if (name == "rsp") return RSP;
  if (name == "rfp") return RFP;
  if (name == "rpc") return RPC;
  if (name == "rflags") return RFLAGS;
  if (name == "rrv") return RRV;

  if (name.size() >= 2 && name[0] == 'r') {
    try {
      int num = std::stoi(name.substr(1));
      if (num >= 0 && num <= 15) return static_cast<uint16_t>(num);
    } catch (...) {
    }
  }

  throw AssemblerError(line, "Invalid register: " + name);
}

// ============================================================================
// Assembly
// ============================================================================

Assembler::AssembleResult Assembler::assemble(const std::string& source) {
  AssembleResult result;
  auto tokens = tokenize(source);

  // First pass: collect labels and their addresses
  uint16_t instrAddr = 0;
  std::unordered_map<std::string, uint16_t> labels;
  std::unordered_map<std::string, uint16_t> namedConstants;

  for (size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i].type == Token::LABEL) {
      labels[tokens[i].text] = instrAddr;
    } else if (tokens[i].type == Token::DIRECTIVE) {
      // Directives don't generate instructions, skip their operands
      if (tokens[i].text == "const" || tokens[i].text == "string") {
        // Skip name and value tokens
        ++i;  // skip name
        ++i;  // skip value
      }
    } else if (tokens[i].type == Token::OPCODE) {
      instrAddr++;
    }
  }

  result.labelMap = labels;

  // Second pass: generate instructions
  size_t ti = 0;
  auto next = [&]() -> const Token& { return tokens[ti++]; };
  auto skipCommasAndNewlines = [&]() {
    while (ti < tokens.size() && (tokens[ti].type == Token::COMMA ||
                                  tokens[ti].type == Token::NEWLINE))
      ++ti;
  };
  auto expectComma = [&]() {
    if (ti < tokens.size() && tokens[ti].type == Token::COMMA) ++ti;
  };

  // Helper: parse an operand (register, immediate, or label ref)
  // Returns the uint16_t value and sets isImmediate if it was an immediate
  struct Operand {
    uint16_t value;
    bool isImmediate;
    bool isLabelRef;
    int64_t rawInt;  // raw integer value (for offsets etc.)
  };

  auto parseOperand = [&](int line) -> Operand {
    const Token& tok = next();
    switch (tok.type) {
      case Token::REGISTER:
        return {parseRegister(tok.text, line), false, false, 0};
      case Token::IMMEDIATE_INT: {
        uint16_t idx = result.constants.addInt(tok.intVal);
        return {idx, true, false, tok.intVal};
      }
      case Token::IMMEDIATE_FLOAT: {
        uint16_t idx = result.constants.addFloat(tok.floatVal);
        return {idx, true, false, 0};
      }
      case Token::IMMEDIATE_STRING: {
        uint16_t idx = result.constants.addString(tok.text);
        return {idx, true, false, 0};
      }
      case Token::LABEL_REF: {
        auto it = labels.find(tok.text);
        if (it == labels.end())
          throw AssemblerError(line, "Undefined label: " + tok.text);
        return {it->second, false, true, 0};
      }
      default:
        throw AssemblerError(line, "Expected operand, got: " + tok.text);
    }
  };

  ti = 0;
  while (ti < tokens.size()) {
    skipCommasAndNewlines();
    if (ti >= tokens.size() || tokens[ti].type == Token::END) break;

    const Token& tok = next();

    // Skip labels (already processed)
    if (tok.type == Token::LABEL) continue;

    // Handle directives
    if (tok.type == Token::DIRECTIVE) {
      if (tok.text == "const") {
        // .const name value
        const Token& nameTok = next();
        const Token& valTok = next();
        uint16_t idx;
        if (valTok.type == Token::IMMEDIATE_INT)
          idx = result.constants.addInt(valTok.intVal);
        else if (valTok.type == Token::IMMEDIATE_FLOAT)
          idx = result.constants.addFloat(valTok.floatVal);
        else
          throw AssemblerError(tok.line, "Expected numeric constant value");
        namedConstants[nameTok.text] = idx;
      } else if (tok.text == "string") {
        const Token& nameTok = next();
        const Token& valTok = next();
        if (valTok.type != Token::IMMEDIATE_STRING)
          throw AssemblerError(tok.line, "Expected string value");
        uint16_t idx = result.constants.addString(valTok.text);
        namedConstants[nameTok.text] = idx;
      }
      continue;
    }

    // Must be an opcode
    if (tok.type != Token::OPCODE)
      throw AssemblerError(tok.line, "Expected opcode, got: " + tok.text);

    auto& opcodeMap = getOpcodeMap();
    auto it = opcodeMap.find(tok.text);
    if (it == opcodeMap.end())
      throw AssemblerError(tok.line, "Unknown opcode: " + tok.text);

    Instruction instr = {};
    instr.opcode = it->second;
    instr.flags = OpFlag::NONE;

    // Parse operands based on opcode
    switch (instr.opcode) {
      // No operands
      case OpCode::NOP:
      case OpCode::HALT:
      case OpCode::RET:
      case OpCode::DUP:
      case OpCode::SWAP:
      case OpCode::ROT:
      case OpCode::DEPTH:
      case OpCode::LEAVE:
      case OpCode::DUMP_REGS:
      case OpCode::DUMP_STACK:
      case OpCode::DUMP_HEAP:
      case OpCode::TRACE_ON:
      case OpCode::TRACE_OFF:
      case OpCode::ENDTRY:
      case OpCode::GC_COLLECT:
      case OpCode::GC_ON:
      case OpCode::GC_OFF:
      case OpCode::GC_STATS:
        break;

      // Single register operand
      case OpCode::PUSH:
      case OpCode::POP:
      case OpCode::INC:
      case OpCode::DEC:
      case OpCode::NEG:
      case OpCode::FNEG:
      case OpCode::FREE:
      case OpCode::PRINT:
      case OpCode::PRINTLN:
      case OpCode::INPUT:
      case OpCode::INPUTF:
      case OpCode::INPUTS:
      case OpCode::THROW:
      case OpCode::DEBUG: {
        auto op = parseOperand(tok.line);
        instr.op1 = op.value;
        break;
      }

      // Single immediate/label operand
      case OpCode::JMP:
      case OpCode::JZ:
      case OpCode::JNZ:
      case OpCode::JG:
      case OpCode::JL:
      case OpCode::JGE:
      case OpCode::JLE:
      case OpCode::JO:
      case OpCode::JNO:
      case OpCode::CALL:
      case OpCode::TRY: {
        auto op = parseOperand(tok.line);
        instr.op1 = op.value;
        break;
      }

      // PRINTS: single constant pool index
      case OpCode::PRINTS: {
        auto op = parseOperand(tok.line);
        instr.op1 = op.value;
        break;
      }

      // PICK: single immediate
      case OpCode::PICK: {
        auto op = parseOperand(tok.line);
        instr.op1 = op.value;
        break;
      }

      // ENTER: single immediate (number of locals)
      case OpCode::ENTER: {
        auto op = parseOperand(tok.line);
        instr.op1 = op.value;
        break;
      }

      // SYSCALL: single immediate (syscall number)
      case OpCode::SYSCALL: {
        auto op = parseOperand(tok.line);
        instr.op1 = op.value;
        break;
      }

      case OpCode::BREAKPOINT:
        break;

      // Two operands: dst, src
      case OpCode::MOV:
      case OpCode::LOAD:
      case OpCode::STORE:
      case OpCode::XCHG:
      case OpCode::ABS:
      case OpCode::FABS:
      case OpCode::FSQRT:
      case OpCode::FSIN:
      case OpCode::FCOS:
      case OpCode::ITOF:
      case OpCode::FTOI:
      case OpCode::ITOS:
      case OpCode::FTOS:
      case OpCode::NOT:
      case OpCode::CMP:
      case OpCode::FCMP:
      case OpCode::TEST:
      case OpCode::ALLOC:
      case OpCode::ANEW:
      case OpCode::ALEN: {
        auto op1 = parseOperand(tok.line);
        instr.op1 = op1.value;
        expectComma();
        auto op2 = parseOperand(tok.line);
        instr.op2 = op2.value;
        break;
      }

      // MOVI / LOAD_CONST: register, constant pool index
      case OpCode::MOVI:
      case OpCode::LOAD_CONST: {
        auto op1 = parseOperand(tok.line);
        instr.op1 = op1.value;
        expectComma();
        auto op2 = parseOperand(tok.line);
        instr.op2 = op2.value;
        break;
      }

      // LOOP: target, counter_register
      case OpCode::LOOP: {
        auto op1 = parseOperand(tok.line);
        instr.op1 = op1.value;
        expectComma();
        auto op2 = parseOperand(tok.line);
        instr.op2 = op2.value;
        break;
      }

      // Three operands: dst, src1, src2
      case OpCode::ADD:
      case OpCode::SUB:
      case OpCode::MUL:
      case OpCode::DIV:
      case OpCode::MOD:
      case OpCode::FADD:
      case OpCode::FSUB:
      case OpCode::FMUL:
      case OpCode::FDIV:
      case OpCode::FMOD:
      case OpCode::FPOW:
      case OpCode::AND:
      case OpCode::OR:
      case OpCode::XOR:
      case OpCode::SHL:
      case OpCode::SHR:
      case OpCode::SAR:
      case OpCode::ROL:
      case OpCode::ROR:
      case OpCode::MEMCPY:
      case OpCode::MEMSET:
      case OpCode::REALLOC:
      case OpCode::AGET:
      case OpCode::ASET:
      case OpCode::LEA: {
        auto op1 = parseOperand(tok.line);
        instr.op1 = op1.value;
        expectComma();
        auto op2 = parseOperand(tok.line);
        instr.op2 = op2.value;
        expectComma();
        auto op3 = parseOperand(tok.line);
        instr.op3 = op3.value;
        break;
      }

      // LOADH: dst, base_reg, offset (raw immediate)
      case OpCode::LOADH: {
        auto op1 = parseOperand(tok.line);
        instr.op1 = op1.value;
        expectComma();
        auto op2 = parseOperand(tok.line);
        instr.op2 = op2.value;
        expectComma();
        auto op3 = parseOperand(tok.line);
        // Use raw integer for the offset, not constant pool index
        instr.op3 =
            op3.isImmediate ? static_cast<uint16_t>(op3.rawInt) : op3.value;
        break;
      }

      // STOREH: base_reg, offset, src_reg
      case OpCode::STOREH: {
        auto op1 = parseOperand(tok.line);
        instr.op1 = op1.value;
        expectComma();
        auto op2 = parseOperand(tok.line);
        // Use raw integer for the offset, not constant pool index
        instr.op2 =
            op2.isImmediate ? static_cast<uint16_t>(op2.rawInt) : op2.value;
        expectComma();
        auto op3 = parseOperand(tok.line);
        instr.op3 = op3.value;
        break;
      }

      default:
        throw AssemblerError(tok.line,
                             "Unhandled opcode in assembler: " + tok.text);
    }

    result.instructions.push_back(instr);
  }

  return result;
}

// ============================================================================
// Disassembler
// ============================================================================

std::string Assembler::disassemble(const std::vector<Instruction>& program,
                                   const ConstantPool& constants) {
  std::ostringstream out;
  auto regName = [](uint16_t r) -> std::string {
    switch (r) {
      case RSP:
        return "rsp";
      case RFP:
        return "rfp";
      case RPC:
        return "rpc";
      case RFLAGS:
        return "rflags";
      case RRV:
        return "rrv";
      default:
        if (r <= 15) return "r" + std::to_string(r);
        return "?" + std::to_string(r);
    }
  };

  for (size_t i = 0; i < program.size(); ++i) {
    const auto& instr = program[i];
    out << std::setw(4) << std::setfill('0') << i << "  " << std::setw(10)
        << std::left << std::setfill(' ') << opcodeName(instr.opcode);

    switch (instr.opcode) {
      case OpCode::NOP:
      case OpCode::HALT:
      case OpCode::RET:
      case OpCode::DUP:
      case OpCode::SWAP:
      case OpCode::ROT:
      case OpCode::DEPTH:
      case OpCode::LEAVE:
      case OpCode::DUMP_REGS:
      case OpCode::DUMP_STACK:
      case OpCode::DUMP_HEAP:
      case OpCode::TRACE_ON:
      case OpCode::TRACE_OFF:
      case OpCode::ENDTRY:
      case OpCode::GC_COLLECT:
      case OpCode::GC_ON:
      case OpCode::GC_OFF:
      case OpCode::GC_STATS:
      case OpCode::BREAKPOINT:
        break;

      case OpCode::PUSH:
      case OpCode::POP:
      case OpCode::INC:
      case OpCode::DEC:
      case OpCode::NEG:
      case OpCode::FNEG:
      case OpCode::FREE:
      case OpCode::PRINT:
      case OpCode::PRINTLN:
      case OpCode::INPUT:
      case OpCode::INPUTF:
      case OpCode::INPUTS:
      case OpCode::THROW:
      case OpCode::DEBUG:
        out << regName(instr.op1);
        break;

      case OpCode::JMP:
      case OpCode::JZ:
      case OpCode::JNZ:
      case OpCode::JG:
      case OpCode::JL:
      case OpCode::JGE:
      case OpCode::JLE:
      case OpCode::JO:
      case OpCode::JNO:
      case OpCode::CALL:
      case OpCode::TRY:
        out << "@" << instr.op1;
        break;

      case OpCode::PRINTS:
        out << "#" << instr.op1;
        if (instr.op1 < constants.size())
          out << " ; \"" << constants.get(instr.op1).asString() << "\"";
        break;

      case OpCode::PICK:
      case OpCode::ENTER:
      case OpCode::SYSCALL:
        out << "#" << instr.op1;
        break;

      case OpCode::MOV:
      case OpCode::LOAD:
      case OpCode::STORE:
      case OpCode::XCHG:
      case OpCode::ABS:
      case OpCode::FABS:
      case OpCode::FSQRT:
      case OpCode::FSIN:
      case OpCode::FCOS:
      case OpCode::ITOF:
      case OpCode::FTOI:
      case OpCode::ITOS:
      case OpCode::FTOS:
      case OpCode::NOT:
      case OpCode::CMP:
      case OpCode::FCMP:
      case OpCode::TEST:
      case OpCode::ALLOC:
      case OpCode::ANEW:
      case OpCode::ALEN:
        out << regName(instr.op1) << ", " << regName(instr.op2);
        break;

      case OpCode::MOVI:
      case OpCode::LOAD_CONST:
        out << regName(instr.op1) << ", #" << instr.op2;
        if (instr.op2 < constants.size())
          out << " ; " << constants.get(instr.op2).asString();
        break;

      case OpCode::LOOP:
        out << "@" << instr.op1 << ", " << regName(instr.op2);
        break;

      default:
        out << regName(instr.op1) << ", " << regName(instr.op2) << ", "
            << regName(instr.op3);
        break;
    }

    out << "\n";
  }

  return out.str();
}
