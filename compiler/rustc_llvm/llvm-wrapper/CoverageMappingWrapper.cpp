#include "LLVMWrapper.h"
#include "llvm/ProfileData/Coverage/CoverageMapping.h"
#include "llvm/ProfileData/Coverage/CoverageMappingWriter.h"
#include "llvm/ProfileData/InstrProf.h"
#include "llvm/ADT/ArrayRef.h"

#include <iostream>

using namespace llvm;

struct LLVMRustCounterMappingRegion {
  coverage::Counter Count;
  coverage::Counter FalseCount;
  uint32_t FileID;
  uint32_t ExpandedFileID;
  uint32_t LineStart;
  uint32_t ColumnStart;
  uint32_t LineEnd;
  uint32_t ColumnEnd;
  coverage::CounterMappingRegion::RegionKind Kind;
};

extern "C" void LLVMRustCoverageWriteFilenamesSectionToBuffer(
    const char* const Filenames[],
    size_t FilenamesLen,
    RustStringRef BufferOut) {
  SmallVector<std::string,32> FilenameRefs;
  for (size_t i = 0; i < FilenamesLen; i++) {
    FilenameRefs.push_back(std::string(Filenames[i]));
  }
  auto FilenamesWriter =
      coverage::CoverageFilenamesSectionWriter(ArrayRef<std::string>(FilenameRefs));
  RawRustStringOstream OS(BufferOut);
  FilenamesWriter.write(OS);
}

extern "C" void LLVMRustCoverageWriteMappingToBuffer(
    const unsigned *VirtualFileMappingIDs,
    unsigned NumVirtualFileMappingIDs,
    const coverage::CounterExpression *Expressions,
    unsigned NumExpressions,
    LLVMRustCounterMappingRegion *RustMappingRegions,
    unsigned NumMappingRegions,
    RustStringRef BufferOut) {
  // Convert from FFI representation to LLVM representation.
  SmallVector<coverage::CounterMappingRegion, 0> MappingRegions;
  MappingRegions.reserve(NumMappingRegions);
  for (const auto &Region : ArrayRef<LLVMRustCounterMappingRegion>(
           RustMappingRegions, NumMappingRegions)) {
    MappingRegions.emplace_back(
        Region.Count, Region.FalseCount, Region.FileID, Region.ExpandedFileID,
        Region.LineStart, Region.ColumnStart, Region.LineEnd, Region.ColumnEnd,
        Region.Kind);
  }
  auto CoverageMappingWriter = coverage::CoverageMappingWriter(
      ArrayRef<unsigned>(VirtualFileMappingIDs, NumVirtualFileMappingIDs),
      ArrayRef<coverage::CounterExpression>(Expressions, NumExpressions),
      MappingRegions);
  RawRustStringOstream OS(BufferOut);
  CoverageMappingWriter.write(OS);
}

extern "C" LLVMValueRef LLVMRustCoverageCreatePGOFuncNameVar(LLVMValueRef F, const char *FuncName) {
  StringRef FuncNameRef(FuncName);
  return wrap(createPGOFuncNameVar(*cast<Function>(unwrap(F)), FuncNameRef));
}

extern "C" uint64_t LLVMRustCoverageHashCString(const char *StrVal) {
  StringRef StrRef(StrVal);
  return IndexedInstrProf::ComputeHash(StrRef);
}

extern "C" uint64_t LLVMRustCoverageHashByteArray(
    const char *Bytes,
    unsigned NumBytes) {
  StringRef StrRef(Bytes, NumBytes);
  return IndexedInstrProf::ComputeHash(StrRef);
}

static void WriteSectionNameToString(LLVMModuleRef M,
                                     InstrProfSectKind SK,
                                     RustStringRef Str) {
  Triple TargetTriple(unwrap(M)->getTargetTriple());
  auto name = getInstrProfSectionName(SK, TargetTriple.getObjectFormat());
  RawRustStringOstream OS(Str);
  OS << name;
}

extern "C" void LLVMRustCoverageWriteMapSectionNameToString(LLVMModuleRef M,
                                                            RustStringRef Str) {
  WriteSectionNameToString(M, IPSK_covmap, Str);
}

extern "C" void LLVMRustCoverageWriteFuncSectionNameToString(LLVMModuleRef M,
                                                             RustStringRef Str) {
  WriteSectionNameToString(M, IPSK_covfun, Str);
}

extern "C" void LLVMRustCoverageWriteMappingVarNameToString(RustStringRef Str) {
  auto name = getCoverageMappingVarName();
  RawRustStringOstream OS(Str);
  OS << name;
}

extern "C" uint32_t LLVMRustCoverageMappingVersion() {
  return coverage::CovMapVersion::Version6;
}
