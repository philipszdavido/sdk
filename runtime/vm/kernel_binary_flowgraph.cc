// Copyright (c) 2016, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "vm/kernel_binary_flowgraph.h"

#if !defined(DART_PRECOMPILED_RUNTIME)

namespace dart {
namespace kernel {

#define Z (zone_)
#define H (translation_helper_)

StreamingConstantEvaluator::StreamingConstantEvaluator(
    StreamingFlowGraphBuilder* builder,
    Zone* zone,
    TranslationHelper* h,
    DartTypeTranslator* type_translator)
    : builder_(builder),
      isolate_(Isolate::Current()),
      zone_(zone),
      translation_helper_(*h),
      // type_translator_(*type_translator),
      script_(Script::Handle(
          zone,
          builder == NULL ? Script::null()
                          : builder_->parsed_function()->function().script())),
      result_(Instance::Handle(zone)) {}


Instance& StreamingConstantEvaluator::EvaluateExpression() {
  intptr_t offset = builder_->ReaderOffset();
  if (!GetCachedConstant(offset, &result_)) {
    uint8_t payload = 0;
    Tag tag = builder_->ReadTag(&payload);
    switch (tag) {
      case kDoubleLiteral:
        EvaluateDoubleLiteral();
        break;
      default:
        UNREACHABLE();
    }

    CacheConstantValue(offset, result_);
  }
  // We return a new `ZoneHandle` here on purpose: The intermediate language
  // instructions do not make a copy of the handle, so we do it.
  return dart::Instance::ZoneHandle(Z, result_.raw());
}

void StreamingConstantEvaluator::EvaluateDoubleLiteral() {
  int str_index = builder_->ReadUInt();
  result_ = dart::Double::New(builder_->DartString(str_index), Heap::kOld);
  result_ = H.Canonicalize(result_);
}

bool StreamingConstantEvaluator::GetCachedConstant(intptr_t kernel_offset,
                                                   Instance* value) {
  if (builder_ == NULL) return false;

  const Function& function = builder_->parsed_function()->function();
  if (function.kind() == RawFunction::kImplicitStaticFinalGetter) {
    // Don't cache constants in initializer expressions. They get
    // evaluated only once.
    return false;
  }

  bool is_present = false;
  ASSERT(!script_.InVMHeap());
  if (script_.compile_time_constants() == Array::null()) {
    return false;
  }
  KernelConstantsMap constants(script_.compile_time_constants());
  *value ^= constants.GetOrNull(kernel_offset, &is_present);
  // Mutator compiler thread may add constants while background compiler
  // is running, and thus change the value of 'compile_time_constants';
  // do not assert that 'compile_time_constants' has not changed.
  constants.Release();
  if (FLAG_compiler_stats && is_present) {
    H.thread()->compiler_stats()->num_const_cache_hits++;
  }
  return is_present;
}


void StreamingConstantEvaluator::CacheConstantValue(intptr_t kernel_offset,
                                                    const Instance& value) {
  ASSERT(Thread::Current()->IsMutatorThread());

  if (builder_ == NULL) return;

  const Function& function = builder_->parsed_function()->function();
  if (function.kind() == RawFunction::kImplicitStaticFinalGetter) {
    // Don't cache constants in initializer expressions. They get
    // evaluated only once.
    return;
  }
  const intptr_t kInitialConstMapSize = 16;
  ASSERT(!script_.InVMHeap());
  if (script_.compile_time_constants() == Array::null()) {
    const Array& array = Array::Handle(
        HashTables::New<KernelConstantsMap>(kInitialConstMapSize, Heap::kNew));
    script_.set_compile_time_constants(array);
  }
  KernelConstantsMap constants(script_.compile_time_constants());
  constants.InsertNewOrGetValue(kernel_offset, value);
  script_.set_compile_time_constants(constants.Release());
}


Fragment StreamingFlowGraphBuilder::BuildAt(intptr_t kernel_offset) {
  SetOffset(kernel_offset);

  uint8_t payload = 0;
  Tag tag = ReadTag(&payload);
  switch (tag) {
    case kInvalidExpression:
      return BuildInvalidExpression();
    //    case kVariableGet:
    //      return VariableGet::ReadFrom(reader_);
    //    case kSpecializedVariableGet:
    //      return VariableGet::ReadFrom(reader_, payload);
    //    case kVariableSet:
    //      return VariableSet::ReadFrom(reader_);
    //    case kSpecializedVariableSet:
    //      return VariableSet::ReadFrom(reader_, payload);
    //    case kPropertyGet:
    //      return PropertyGet::ReadFrom(reader_);
    //    case kPropertySet:
    //      return PropertySet::ReadFrom(reader_);
    //    case kDirectPropertyGet:
    //      return DirectPropertyGet::ReadFrom(reader_);
    //    case kDirectPropertySet:
    //      return DirectPropertySet::ReadFrom(reader_);
    //    case kStaticGet:
    //      return StaticGet::ReadFrom(reader_);
    //    case kStaticSet:
    //      return StaticSet::ReadFrom(reader_);
    //    case kMethodInvocation:
    //      return MethodInvocation::ReadFrom(reader_);
    //    case kDirectMethodInvocation:
    //      return DirectMethodInvocation::ReadFrom(reader_);
    //    case kStaticInvocation:
    //      return StaticInvocation::ReadFrom(reader_, false);
    //    case kConstStaticInvocation:
    //      return StaticInvocation::ReadFrom(reader_, true);
    //    case kConstructorInvocation:
    //      return ConstructorInvocation::ReadFrom(reader_, false);
    //    case kConstConstructorInvocation:
    //      return ConstructorInvocation::ReadFrom(reader_, true);
    //    case kNot:
    //      return Not::ReadFrom(reader_);
    //    case kLogicalExpression:
    //      return LogicalExpression::ReadFrom(reader_);
    //    case kConditionalExpression:
    //      return ConditionalExpression::ReadFrom(reader_);
    //    case kStringConcatenation:
    //      return StringConcatenation::ReadFrom(reader_);
    //    case kIsExpression:
    //      return IsExpression::ReadFrom(reader_);
    //    case kAsExpression:
    //      return AsExpression::ReadFrom(reader_);
    //    case kSymbolLiteral:
    //      return SymbolLiteral::ReadFrom(reader_);
    //    case kTypeLiteral:
    //      return TypeLiteral::ReadFrom(reader_);
    case kThisExpression:
      return BuildThisExpression();
    case kRethrow:
      return BuildRethrow();
    //    case kThrow:
    //      return Throw::ReadFrom(reader_);
    //    case kListLiteral:
    //      return ListLiteral::ReadFrom(reader_, false);
    //    case kConstListLiteral:
    //      return ListLiteral::ReadFrom(reader_, true);
    //    case kMapLiteral:
    //      return MapLiteral::ReadFrom(reader_, false);
    //    case kConstMapLiteral:
    //      return MapLiteral::ReadFrom(reader_, true);
    //    case kAwaitExpression:
    //      return AwaitExpression::ReadFrom(reader_);
    //    case kFunctionExpression:
    //      return FunctionExpression::ReadFrom(reader_);
    //    case kLet:
    //      return Let::ReadFrom(reader_);
    //    case kBigIntLiteral:
    //      return BigintLiteral::ReadFrom(reader_);
    case kStringLiteral:
      return BuildStringLiteral();
    case kSpecialIntLiteral:
      return BuildIntLiteral(payload);
    case kNegativeIntLiteral:
      return BuildIntLiteral(true);
    case kPositiveIntLiteral:
      return BuildIntLiteral(false);
    case kDoubleLiteral:
      return BuildDoubleLiteral();
    case kTrueLiteral:
      return BuildBoolLiteral(true);
    case kFalseLiteral:
      return BuildBoolLiteral(false);
    case kNullLiteral:
      return BuildNullLiteral();
    default:
      UNREACHABLE();
  }

  return Fragment();
}

intptr_t StreamingFlowGraphBuilder::GetStringTableOffset(intptr_t index) {
  if (string_table_offsets_ != NULL && string_table_entries_read_ > index) {
    return string_table_offsets_[index];
  }
  if (string_table_offsets_ == NULL) {
    reader_->set_offset(4);  // Skip kMagicProgramFile - now at string table.
    string_table_size_ = ReadListLength();
    string_table_offsets_ = new intptr_t[string_table_size_];
    string_table_offsets_[0] = ReaderOffset();
    string_table_entries_read_ = 1;
  }

  ASSERT(string_table_size_ > index);
  --string_table_entries_read_;
  reader_->set_offset(string_table_offsets_[string_table_entries_read_]);
  for (; string_table_entries_read_ < index; ++string_table_entries_read_) {
    string_table_offsets_[string_table_entries_read_] = ReaderOffset();
    uint32_t bytes = ReadUInt();
    SkipBytes(bytes);
  }
  string_table_offsets_[string_table_entries_read_] = ReaderOffset();
  ++string_table_entries_read_;

  return string_table_offsets_[index];
}

intptr_t StreamingFlowGraphBuilder::ReaderOffset() {
  return reader_->offset();
}

void StreamingFlowGraphBuilder::SetOffset(intptr_t offset) {
  reader_->set_offset(offset);
}

void StreamingFlowGraphBuilder::SkipBytes(intptr_t bytes) {
  reader_->set_offset(ReaderOffset() + bytes);
}

uint32_t StreamingFlowGraphBuilder::ReadUInt() {
  return reader_->ReadUInt();
}

intptr_t StreamingFlowGraphBuilder::ReadListLength() {
  return reader_->ReadListLength();
}

TokenPosition StreamingFlowGraphBuilder::ReadPosition(bool record) {
  return reader_->ReadPosition(record);
}

Tag StreamingFlowGraphBuilder::ReadTag(uint8_t* payload) {
  return reader_->ReadTag(payload);
}

CatchBlock* StreamingFlowGraphBuilder::catch_block() {
  return flow_graph_builder_->catch_block_;
}

ScopeBuildingResult* StreamingFlowGraphBuilder::scopes() {
  return flow_graph_builder_->scopes_;
}

ParsedFunction* StreamingFlowGraphBuilder::parsed_function() {
  return flow_graph_builder_->parsed_function_;
}

dart::String& StreamingFlowGraphBuilder::DartSymbol(intptr_t str_index) {
  intptr_t saved_offset = ReaderOffset();

  SetOffset(GetStringTableOffset(str_index));
  uint32_t bytes = ReadUInt();
  const uint8_t* data = &reader_->buffer()[ReaderOffset()];

  SetOffset(saved_offset);
  return H.DartSymbol(data, bytes);
}

dart::String& StreamingFlowGraphBuilder::DartString(intptr_t str_index) {
  intptr_t saved_offset = ReaderOffset();

  SetOffset(GetStringTableOffset(str_index));
  uint32_t bytes = ReadUInt();
  const uint8_t* data = &reader_->buffer()[ReaderOffset()];

  SetOffset(saved_offset);
  return H.DartString(data, bytes);
}

Fragment StreamingFlowGraphBuilder::DebugStepCheck(TokenPosition position) {
  return flow_graph_builder_->DebugStepCheck(position);
}

Fragment StreamingFlowGraphBuilder::LoadLocal(LocalVariable* variable) {
  return flow_graph_builder_->LoadLocal(variable);
}

Fragment StreamingFlowGraphBuilder::PushArgument() {
  return flow_graph_builder_->PushArgument();
}

Fragment StreamingFlowGraphBuilder::RethrowException(TokenPosition position,
                                                     int catch_try_index) {
  return flow_graph_builder_->RethrowException(position, catch_try_index);
}

Fragment StreamingFlowGraphBuilder::ThrowNoSuchMethodError() {
  return flow_graph_builder_->ThrowNoSuchMethodError();
}

Fragment StreamingFlowGraphBuilder::Constant(const Object& value) {
  return flow_graph_builder_->Constant(value);
}

Fragment StreamingFlowGraphBuilder::IntConstant(int64_t value) {
  return flow_graph_builder_->IntConstant(value);
}

Fragment StreamingFlowGraphBuilder::BuildInvalidExpression() {
  // The frontend will take care of emitting normal errors (like
  // [NoSuchMethodError]s) and only emit [InvalidExpression]s in very special
  // situations (e.g. an invalid annotation).
  return ThrowNoSuchMethodError();
}

Fragment StreamingFlowGraphBuilder::BuildThisExpression() {
  return LoadLocal(scopes()->this_variable);
}

Fragment StreamingFlowGraphBuilder::BuildRethrow() {
  TokenPosition position = ReadPosition();
  Fragment instructions = DebugStepCheck(position);
  instructions += LoadLocal(catch_block()->exception_var());
  instructions += PushArgument();
  instructions += LoadLocal(catch_block()->stack_trace_var());
  instructions += PushArgument();
  instructions += RethrowException(position, catch_block()->catch_try_index());

  return instructions;
}

Fragment StreamingFlowGraphBuilder::BuildStringLiteral() {
  intptr_t str_index = ReadUInt();
  return Constant(DartSymbol(str_index));
}

Fragment StreamingFlowGraphBuilder::BuildIntLiteral(uint8_t payload) {
  int64_t value = static_cast<int32_t>(payload) - SpecializedIntLiteralBias;
  return IntConstant(value);
}

Fragment StreamingFlowGraphBuilder::BuildIntLiteral(bool is_negative) {
  int64_t value = is_negative ? -static_cast<int64_t>(ReadUInt()) : ReadUInt();
  return IntConstant(value);
}

Fragment StreamingFlowGraphBuilder::BuildDoubleLiteral() {
  SkipBytes(-1);  // EvaluateExpression needs the tag.
  return Constant(constant_evaluator_.EvaluateExpression());
}

Fragment StreamingFlowGraphBuilder::BuildBoolLiteral(bool value) {
  return Constant(Bool::Get(value));
}

Fragment StreamingFlowGraphBuilder::BuildNullLiteral() {
  return Constant(Instance::ZoneHandle(Z, Instance::null()));
}

}  // namespace kernel
}  // namespace dart

#endif  // !defined(DART_PRECOMPILED_RUNTIME)
