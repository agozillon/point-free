//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file borrows a few helper functions from clang-format so that I 
/// can easily call clangs reformat function. 
///
//===----------------------------------------------------------------------===//

#ifndef CLANG_FORMAT_HELPERS_H
#define CLANG_FORMAT_HELPERS_H

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/Version.h"
#include "clang/Format/Format.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Signals.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

bool fillRanges(MemoryBuffer *Code, std::vector<tooling::Range> &Ranges); 

#endif // CLANG_FORMAT_HELPERS_H