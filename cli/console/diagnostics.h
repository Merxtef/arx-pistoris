// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>

namespace cli {

enum class DiagnosticCode : std::uint8_t {
  kKindAmbiguous,
  kKindInvalid,
  kLogLevelAmbiguous,
  kLogLevelInvalid,
  kMissingValues,
  kInvalidNumber,
  kScaleInvalid,
  kMissingArgument,
  kInvalidMode,
  kKindConflict,
  kAmbiguousOption,
  kUnknownOption,
  kDuplicateModule,
  kModuleImplicationConflict,
  kModuleImplicationInvalid,
  kMissingInputOutput,
  kIncompatibleModules,
  kMissingDependency,
  kOutputConverterConflict,
  kInvalidModuleValue,
  kUnsupportedOutputFormat,
  kAmbiguousOutputFormat,
  kModuleOutputMismatch,
  kModuleFormatMismatch,
  kRouteConstraintConflict,
  kRouteInputMismatch,
  kRouteInvocationInvalid,
  kNoRoute,
  kAmbiguousRoute,
  kClassificationFailed,
  kResourceSelectorInvalid,
  kResourceNotFound,
  kResourceReadFailed,
  kUnknownHelpTopic,
  kLevelUnsupportedInput,
  kLevelInputFailed,
  kLevelOutputFailed,
  kLevelUnsupportedOutput,
  kIoStatFailed,
  kIoFileTooLarge,
  kIoAllocationFailed,
  kIoOpenFailed,
  kIoReadFailed,
  kIoCreateFailed,
  kIoWriteFailed,
  kModelModuleInvalid,
  kModelModuleFailed,
  kModelUnsupportedExtra,
  kModelInputFailed,
  kModelOutputFailed,
  kModelUnsupportedInput,
  kModelUnsupportedOutput,
  kModelTeaMismatch,
  kAnimationModuleFailed,
  kAnimationInputFailed,
  kAnimationOutputFailed,
  kAnimationUnsupportedInput,
  kAnimationUnsupportedOutput,
  kUnhandledException,
};

inline const char* diagnosticCodeName(DiagnosticCode code) {
  switch (code) {
    case DiagnosticCode::kKindAmbiguous:
      return "CLI_KIND_AMBIGUOUS";
    case DiagnosticCode::kKindInvalid:
      return "CLI_KIND_INVALID";
    case DiagnosticCode::kLogLevelAmbiguous:
      return "CLI_LOG_LEVEL_AMBIGUOUS";
    case DiagnosticCode::kLogLevelInvalid:
      return "CLI_LOG_LEVEL_INVALID";
    case DiagnosticCode::kMissingValues:
      return "CLI_MISSING_VALUES";
    case DiagnosticCode::kInvalidNumber:
      return "CLI_INVALID_NUMBER";
    case DiagnosticCode::kScaleInvalid:
      return "CLI_SCALE_INVALID";
    case DiagnosticCode::kMissingArgument:
      return "CLI_MISSING_ARGUMENT";
    case DiagnosticCode::kInvalidMode:
      return "CLI_INVALID_MODE";
    case DiagnosticCode::kKindConflict:
      return "CLI_KIND_CONFLICT";
    case DiagnosticCode::kAmbiguousOption:
      return "CLI_AMBIGUOUS_OPTION";
    case DiagnosticCode::kUnknownOption:
      return "CLI_UNKNOWN_OPTION";
    case DiagnosticCode::kDuplicateModule:
      return "CLI_DUPLICATE_MODULE";
    case DiagnosticCode::kModuleImplicationConflict:
      return "CLI_MODULE_IMPLICATION_CONFLICT";
    case DiagnosticCode::kModuleImplicationInvalid:
      return "CLI_MODULE_IMPLICATION_INVALID";
    case DiagnosticCode::kMissingInputOutput:
      return "CLI_MISSING_INPUT_OUTPUT";
    case DiagnosticCode::kIncompatibleModules:
      return "CLI_INCOMPATIBLE_MODULES";
    case DiagnosticCode::kMissingDependency:
      return "CLI_MISSING_DEPENDENCY";
    case DiagnosticCode::kOutputConverterConflict:
      return "CLI_OUTPUT_CONVERTER_CONFLICT";
    case DiagnosticCode::kInvalidModuleValue:
      return "CLI_INVALID_MODULE_VALUE";
    case DiagnosticCode::kUnsupportedOutputFormat:
      return "CLI_UNSUPPORTED_OUTPUT_FORMAT";
    case DiagnosticCode::kAmbiguousOutputFormat:
      return "CLI_AMBIGUOUS_OUTPUT_FORMAT";
    case DiagnosticCode::kModuleOutputMismatch:
      return "CLI_MODULE_OUTPUT_MISMATCH";
    case DiagnosticCode::kModuleFormatMismatch:
      return "CLI_MODULE_FORMAT_MISMATCH";
    case DiagnosticCode::kRouteConstraintConflict:
      return "CLI_ROUTE_CONSTRAINT_CONFLICT";
    case DiagnosticCode::kRouteInputMismatch:
      return "CLI_ROUTE_INPUT_MISMATCH";
    case DiagnosticCode::kRouteInvocationInvalid:
      return "CLI_ROUTE_INVOCATION_INVALID";
    case DiagnosticCode::kNoRoute:
      return "CLI_NO_ROUTE";
    case DiagnosticCode::kAmbiguousRoute:
      return "CLI_AMBIGUOUS_ROUTE";
    case DiagnosticCode::kClassificationFailed:
      return "CLI_CLASSIFICATION_FAILED";
    case DiagnosticCode::kResourceSelectorInvalid:
      return "CLI_RESOURCE_SELECTOR_INVALID";
    case DiagnosticCode::kResourceNotFound:
      return "CLI_RESOURCE_NOT_FOUND";
    case DiagnosticCode::kResourceReadFailed:
      return "CLI_RESOURCE_READ_FAILED";
    case DiagnosticCode::kUnknownHelpTopic:
      return "CLI_UNKNOWN_HELP_TOPIC";
    case DiagnosticCode::kLevelUnsupportedInput:
      return "CLI_LEVEL_UNSUPPORTED_INPUT";
    case DiagnosticCode::kLevelInputFailed:
      return "CLI_LEVEL_INPUT_FAILED";
    case DiagnosticCode::kLevelOutputFailed:
      return "CLI_LEVEL_OUTPUT_FAILED";
    case DiagnosticCode::kLevelUnsupportedOutput:
      return "CLI_LEVEL_UNSUPPORTED_OUTPUT";
    case DiagnosticCode::kIoStatFailed:
      return "CLI_IO_STAT_FAILED";
    case DiagnosticCode::kIoFileTooLarge:
      return "CLI_IO_FILE_TOO_LARGE";
    case DiagnosticCode::kIoAllocationFailed:
      return "CLI_IO_ALLOCATION_FAILED";
    case DiagnosticCode::kIoOpenFailed:
      return "CLI_IO_OPEN_FAILED";
    case DiagnosticCode::kIoReadFailed:
      return "CLI_IO_READ_FAILED";
    case DiagnosticCode::kIoCreateFailed:
      return "CLI_IO_CREATE_FAILED";
    case DiagnosticCode::kIoWriteFailed:
      return "CLI_IO_WRITE_FAILED";
    case DiagnosticCode::kModelModuleInvalid:
      return "CLI_MODEL_MODULE_INVALID";
    case DiagnosticCode::kModelModuleFailed:
      return "CLI_MODEL_MODULE_FAILED";
    case DiagnosticCode::kModelUnsupportedExtra:
      return "CLI_MODEL_UNSUPPORTED_EXTRA";
    case DiagnosticCode::kModelInputFailed:
      return "CLI_MODEL_INPUT_FAILED";
    case DiagnosticCode::kModelOutputFailed:
      return "CLI_MODEL_OUTPUT_FAILED";
    case DiagnosticCode::kModelUnsupportedInput:
      return "CLI_MODEL_UNSUPPORTED_INPUT";
    case DiagnosticCode::kModelUnsupportedOutput:
      return "CLI_MODEL_UNSUPPORTED_OUTPUT";
    case DiagnosticCode::kModelTeaMismatch:
      return "CLI_MODEL_TEA_MISMATCH";
    case DiagnosticCode::kAnimationModuleFailed:
      return "CLI_ANIMATION_MODULE_FAILED";
    case DiagnosticCode::kAnimationInputFailed:
      return "CLI_ANIMATION_INPUT_FAILED";
    case DiagnosticCode::kAnimationOutputFailed:
      return "CLI_ANIMATION_OUTPUT_FAILED";
    case DiagnosticCode::kAnimationUnsupportedInput:
      return "CLI_ANIMATION_UNSUPPORTED_INPUT";
    case DiagnosticCode::kAnimationUnsupportedOutput:
      return "CLI_ANIMATION_UNSUPPORTED_OUTPUT";
    case DiagnosticCode::kUnhandledException:
      return "CLI_UNHANDLED_EXCEPTION";
  }
  return "CLI_UNKNOWN_DIAGNOSTIC";
}

inline void diagnosticPrefix(DiagnosticCode code) { std::fprintf(stderr, "[%s] ", diagnosticCodeName(code)); }

inline void diagnostic(DiagnosticCode code, const char* fmt, ...) {
  diagnosticPrefix(code);
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(stderr, fmt, ap);
  va_end(ap);
  std::fputc('\n', stderr);
}

}  // namespace cli
