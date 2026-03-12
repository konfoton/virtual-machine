#pragma once
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "opcodes.h"
#include "value.h"

// Assembler: converts text assembly into bytecoded instructions + constant
// pool.
//
// Syntax:
//   label:                   ; define a label
//   opcode operand, ...      ; instruction
//   .const name value        ; define a named constant
//   .string name "text"      ; define a string constant
//   ; comment                ; line comment
//
// Registers: r0-r15, rsp, rfp, rrv
// Immediates: #42, #3.14, #"hello"
// Labels: @label_name (in jump/call targets)

struct AssemblerError : public std::runtime_error {
  int line;
  AssemblerError(int line, const std::string& msg)
      : std::runtime_error("Line " + std::to_string(line) + ": " + msg),
        line(line) {}
};

class Assembler {
 public:
  struct AssembleResult {
    std::vector<Instruction> instructions;
    ConstantPool constants;
    std::unordered_map<std::string, uint16_t> labelMap;
  };

  AssembleResult assemble(const std::string& source);

  // Disassemble back to text
  static std::string disassemble(const std::vector<Instruction>& program,
                                 const ConstantPool& constants);

 private:
  struct Token {
    enum Type {
      LABEL,
      OPCODE,
      REGISTER,
      IMMEDIATE_INT,
      IMMEDIATE_FLOAT,
      IMMEDIATE_STRING,
      LABEL_REF,
      DIRECTIVE,
      COMMA,
      NEWLINE,
      END
    };
    Type type;
    std::string text;
    int64_t intVal = 0;
    double floatVal = 0.0;
    int line = 0;
  };

  std::vector<Token> tokenize(const std::string& source);
  uint16_t parseRegister(const std::string& name, int line);
};
