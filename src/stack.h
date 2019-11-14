// Copyright 2019 YHSPY. All rights reserved.
#ifndef STACK_H_
#define STACK_H_

#include <stack>
#include <memory>
#include <type_traits>
#include "src/macros.h"
#include "src/types.h"

using std::stack;
using std::shared_ptr;
using std::static_pointer_cast;
using std::enable_shared_from_this;
using std::is_same;

class Frame : public enable_shared_from_this<Frame> {
 public:
  explicit Frame(StackFrameTypes type) : type(type) {}
  ~Frame() = default;

  template<typename T>
  shared_ptr<T> as() {
    return static_pointer_cast<T>(shared_from_this());
  }
 private:
  StackFrameTypes type = StackFrameTypes::kValues;
};

template <typename T>
class ValuesFrame : public Frame {
  // here we directly use "int32_t", "int64_t", "float", "double" to present 4 kinds of Wasm values;
  union val {
    int32_t vI32Const;
    int64_t vI64Const;
    float vF32Const;
    double vF64Const;
  };
  
 public:
  FORBID_COPYING(ValuesFrame);
  ValuesFrame(T value) : Frame(StackFrameTypes::kValues) {
    if (is_same<T, int32_t>::value) {
      data.vI32Const = value;
    } else if (is_same<T, int64_t>::value) {
      data.vI64Const = value;
    } else if (is_same<T, float>::value) {
      data.vF32Const = value;
    } else if (is_same<T, double>::value) {
      data.vF64Const = value;
    }
  }
  const T& value() {
    return *reinterpret_cast<T*>(&data);
  };
 private:
   val data;
};

class LabelsFrame : public Frame {
 public:
  FORBID_COPYING(LabelsFrame);
  LabelsFrame() : Frame(StackFrameTypes::kLabels) {}
};

class ActivationsFrame : public Frame {
 public:
  FORBID_COPYING(ActivationsFrame);
  ActivationsFrame() : Frame(StackFrameTypes::kActivations) {}
};

// for saving "Values" / "Labels" / "Activations";
class Stack {
 public:
  Stack() = default;
  ~Stack() = default;
  
 private:
  stack<Frame*> data;
};

#endif  // STACK_H_