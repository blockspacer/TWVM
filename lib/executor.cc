// Copyright 2019 YHSPY. All rights reserved.
#include <array>
#include "lib/executor.h"
#include "lib/interpreter.h"
#include "lib/decoder.h"
#include "lib/utility.h"
#include "lib/common/macros.h"
#include "lib/common/constants.h"
#include "lib/common/opcode.h"
#include "lib/instances/ins-wasm.h"

using std::array;
using handlerProto = void (shared_ptr<WasmInstance>&, Executor*);

const bool Executor::checkStackState(shared_ptr<WasmInstance> wasmIns) {
  // check the status of stack.
  const auto valueStack = wasmIns->stack->valueStack;
  const auto leftValueSize = valueStack->size();
  (Printer::instance() << '(' << (wasmIns->startEntry ? "start" : "main") << "): ").say();
  if (leftValueSize == 1) {
    valueStack->top()->outputValue(cout << dec);
    // keep the top value on stack, just use it as final result.
    // valueStack->pop().
  } else {
    cout << "(void)";
  }
  cout << endl;
  // reset flags.
  Executor::resetExecutionEngine(valueStack->top());
  return leftValueSize <= 1;
}

const bool Executor::execute(shared_ptr<WasmInstance> wasmIns) {
  (Printer::instance() << '\n').debug();
  (Printer::instance() << "- [EXECUTING PHASE] -\n").debug();

  // save a reference.
  currentWasmIns = wasmIns;

  if (!wasmIns->startPoint) {
    // noting to be executed.
    (Printer::instance() << "no execution entry found.\n").say();
    return true;
  }

  pc = wasmIns->startPoint->pc;
  contextIndex = wasmIns->startPoint->index;

#if !defined(OPT_DCT)
  // build a handler lookup table.
  static array<handlerProto*, U8_SIZE * BYTE_LEN> opcodeTokenHandlers;
#define APPEND_HANDLER_TO_CONTAINER(name, opcode) \
  opcodeTokenHandlers[opcode] = Interpreter::do##name;
  ITERATE_ALL_OPCODE(APPEND_HANDLER_TO_CONTAINER)
#endif

  while (true) {
    if (!runningStatus) {
      // verify running reuslt by the state of final stack.
      return checkStackState(wasmIns);
    }
#if defined(OPT_DCT)
    /**
     * the structure of an opcode action:
     * |----------|----------------------|-------------------------|
     * | OpCode 1 | Invoker(uintptr_t) 8 | Immediates(var/fixed) n |
     * |----------|----------------------|-------------------------|
     */
    // don't use "switch-case" based conditional selection, since -
    // it's overhead from low (Branch-Table -> Binary-Decision-Tree -> if-else) to high, -
    // but not efficient enough on average.
    uintptr_t handlerPtr;
    // skip the identifying byte.
    memcpy(&handlerPtr, pc->data() + (innerOffset += 2), PTR_SIZE);
    innerOffset += (PTR_SIZE - 1);
    // direct call.
    reinterpret_cast<handlerProto*>(handlerPtr)(wasmIns, this);
#else
    // TTC (use "[]" to avoid the overhead of bound-checking).
    opcodeTokenHandlers[(*pc)[++innerOffset]](wasmIns, this);
#endif
  }
  return false;
}

const void Executor::crawler(
  const uint8_t* buf, size_t length, const function<bool(WasmOpCode, size_t)> &callback) {
  // skip every opcode and immediate.
  size_t offset = 0;
  while (offset != length) {
    const auto opcode = static_cast<WasmOpCode>(*(buf + offset++));
    // move pointer to the immediates.
#if defined(OPT_DCT)
    offset += PTR_SIZE;
#endif
    offset += Interpreter::calcOpCodeEntityLen(buf + offset, opcode);
    if (callback && callback(opcode, offset)) {
      return;
    }
  }
}
