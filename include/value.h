#pragma once
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// Tagged value type for the VM — supports integers, floats, strings, and heap
// pointers
enum class ValueType : uint8_t {
  Integer,
  Float,
  String,
  HeapPtr,   // Pointer into VM heap
  ArrayRef,  // Reference to a managed array
  Null,
};

struct Value {
  ValueType type;
  union {
    int64_t i;
    double f;
    uint64_t ptr;  // heap pointer or array reference id
  };
  std::string str;  // only used when type == String

  Value() : type(ValueType::Null), i(0) {}
  explicit Value(int64_t val) : type(ValueType::Integer), i(val) {}
  explicit Value(double val) : type(ValueType::Float), f(val) {}
  explicit Value(const std::string& val)
      : type(ValueType::String), i(0), str(val) {}
  explicit Value(const char* val) : type(ValueType::String), i(0), str(val) {}

  static Value makePtr(uint64_t addr) {
    Value v;
    v.type = ValueType::HeapPtr;
    v.ptr = addr;
    return v;
  }

  static Value makeArray(uint64_t id) {
    Value v;
    v.type = ValueType::ArrayRef;
    v.ptr = id;
    return v;
  }

  static Value null() { return Value(); }

  int64_t asInt() const {
    switch (type) {
      case ValueType::Integer:
        return i;
      case ValueType::Float:
        return static_cast<int64_t>(f);
      case ValueType::HeapPtr:
        return static_cast<int64_t>(ptr);
      default:
        return 0;
    }
  }

  double asFloat() const {
    switch (type) {
      case ValueType::Float:
        return f;
      case ValueType::Integer:
        return static_cast<double>(i);
      default:
        return 0.0;
    }
  }

  std::string asString() const {
    switch (type) {
      case ValueType::String:
        return str;
      case ValueType::Integer:
        return std::to_string(i);
      case ValueType::Float:
        return std::to_string(f);
      case ValueType::HeapPtr:
        return "ptr@" + std::to_string(ptr);
      case ValueType::ArrayRef:
        return "array#" + std::to_string(ptr);
      case ValueType::Null:
        return "null";
    }
    return "?";
  }

  bool isTrue() const {
    switch (type) {
      case ValueType::Integer:
        return i != 0;
      case ValueType::Float:
        return f != 0.0;
      case ValueType::String:
        return !str.empty();
      case ValueType::HeapPtr:
        return ptr != 0;
      case ValueType::Null:
        return false;
      default:
        return false;
    }
  }

  friend std::ostream& operator<<(std::ostream& os, const Value& v) {
    os << v.asString();
    return os;
  }
};

// Constant pool: stores compile-time constants referenced by instructions
struct ConstantPool {
  std::vector<Value> constants;

  uint16_t addInt(int64_t val) {
    constants.emplace_back(val);
    return static_cast<uint16_t>(constants.size() - 1);
  }

  uint16_t addFloat(double val) {
    constants.emplace_back(val);
    return static_cast<uint16_t>(constants.size() - 1);
  }

  uint16_t addString(const std::string& val) {
    constants.emplace_back(val);
    return static_cast<uint16_t>(constants.size() - 1);
  }

  const Value& get(uint16_t index) const {
    if (index >= constants.size())
      throw std::out_of_range("Constant pool index out of range: " +
                              std::to_string(index));
    return constants[index];
  }

  size_t size() const { return constants.size(); }
};

// Managed array for ANEW/AGET/ASET/ALEN
struct ManagedArray {
  std::vector<Value> elements;

  explicit ManagedArray(size_t size) : elements(size) {}
};

// Exception handler entry (for TRY/ENDTRY/THROW)
struct ExceptionHandler {
  uint64_t handlerAddress;
  uint64_t stackDepth;
  uint64_t framePointer;
};

// Call frame for function calls
struct CallFrame {
  uint64_t returnAddress;
  uint64_t savedFramePointer;
  uint16_t savedRegisters;  // bitmask of which registers were saved
};
