#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <optional>

using namespace llvm;

namespace {

constexpr StringLiteral CheckedAttribute = "btap.bounds.checked";

struct AccessInfo {
  Instruction *Access;
  GetElementPtrInst *GEP;
  Value *Index;
  uint64_t Bound;
};

// This intentionally recognizes one canonical pattern emitted for:
//   int values[N]; values[index] = ...; / ... = values[index];
// It does not try to prove arbitrary pointer provenance or track aliases.
static std::optional<AccessInfo> findSupportedAccess(Instruction &I) {
  Value *Pointer = nullptr;
  Type *MemoryType = nullptr;

  if (auto *Load = dyn_cast<LoadInst>(&I)) {
    if (Load->isVolatile() || Load->isAtomic())
      return std::nullopt;
    Pointer = Load->getPointerOperand();
    MemoryType = Load->getType();
  } else if (auto *Store = dyn_cast<StoreInst>(&I)) {
    if (Store->isVolatile() || Store->isAtomic())
      return std::nullopt;
    Pointer = Store->getPointerOperand();
    MemoryType = Store->getValueOperand()->getType();
  } else {
    return std::nullopt;
  }

  auto *GEP = dyn_cast<GetElementPtrInst>(Pointer);
  if (!GEP || GEP->getNumIndices() != 2)
    return std::nullopt;

  auto *Array = dyn_cast<ArrayType>(GEP->getSourceElementType());
  if (!Array || !Array->getElementType()->isIntegerTy() ||
      GEP->getResultElementType() != Array->getElementType() ||
      MemoryType != Array->getElementType())
    return std::nullopt;

  // The first array GEP index selects the allocated array object.  Only zero
  // is supported: an alloca of an array is one fixed-size object, not an array
  // of arrays.  Requiring the uncast alloca keeps this baseline honest.
  auto IndexIt = GEP->idx_begin();
  auto *FirstIndex = dyn_cast<ConstantInt>(IndexIt->get());
  if (!FirstIndex || !FirstIndex->isZero())
    return std::nullopt;
  ++IndexIt;

  auto *Alloca = dyn_cast<AllocaInst>(GEP->getPointerOperand());
  if (!Alloca || Alloca->getAllocatedType() != Array ||
      !isa<ConstantInt>(Alloca->getArraySize()))
    return std::nullopt;

  return AccessInfo{&I, GEP, IndexIt->get(), Array->getNumElements()};
}

static bool isKnownSafeConstant(Value *Index, uint64_t Bound) {
  auto *ConstantIndex = dyn_cast<ConstantInt>(Index);
  return ConstantIndex && !ConstantIndex->isNegative() &&
         ConstantIndex->getValue().ult(Bound);
}

static FunctionCallee getReportFunction(Module &M) {
  LLVMContext &C = M.getContext();
  Type *I64 = Type::getInt64Ty(C);
  Type *I32 = Type::getInt32Ty(C);
  Type *I8Ptr = PointerType::getUnqual(C);
  FunctionType *Type = FunctionType::get(
      Type::getVoidTy(C), {I64, I64, I8Ptr, I8Ptr, I32, I32}, false);
  FunctionCallee Report = M.getOrInsertFunction("btap_report_oob", Type);
  if (auto *F = dyn_cast<Function>(Report.getCallee())) {
    F->addFnAttr(Attribute::NoReturn);
    F->addFnAttr(Attribute::Cold);
  }
  return Report;
}

static void insertCheck(const AccessInfo &Info, FunctionCallee Report) {
  Instruction *Access = Info.Access;
  Function &F = *Access->getFunction();
  Module &M = *F.getParent();
  LLVMContext &C = M.getContext();
  const DataLayout &DL = M.getDataLayout();

  // GEP indices are interpreted at the target pointer-index width.  Do the
  // same conversion for the signed-negative and unsigned-upper-bound tests.
  auto *IndexType = cast<IntegerType>(DL.getIndexType(
      Info.GEP->getPointerOperandType()));
  IRBuilder<> Before(Access);
  Value *Index = Before.CreateSExtOrTrunc(Info.Index, IndexType, "btap.index");

  // A poison index must not make the *check* poison.  For a source program
  // which already computes poison, behavior is not defined; freezing here
  // prevents this pass from introducing a new poison branch.  Removing the
  // inbounds flag below ensures a non-poison out-of-range index reaches this
  // check rather than turning the GEP into poison first.
  Index = Before.CreateFreeze(Index, "btap.index.frozen");
  Value *Negative = Before.CreateICmpSLT(Index, ConstantInt::get(IndexType, 0),
                                         "btap.negative");
  Value *TooLarge = Before.CreateICmpUGE(
      Index, ConstantInt::get(IndexType, Info.Bound), "btap.too_large");
  Value *Invalid = Before.CreateOr(Negative, TooLarge, "btap.invalid");

  // Clang commonly emits inbounds here.  Leaving it in place could make an
  // invalid dynamic index poison before the instrumentation branch executes.
  Info.GEP->setIsInBounds(false);
  Instruction *ThenTerm = SplitBlockAndInsertIfThen(Invalid, Access, false);
  IRBuilder<> Failure(ThenTerm);

  std::string File = "<unknown>";
  uint32_t Line = 0;
  uint32_t Column = 0;
  if (const DebugLoc &Loc = Access->getDebugLoc()) {
    File = Loc->getFilename().str();
    Line = Loc->getLine();
    Column = Loc->getColumn();
  }
  Value *FunctionName = Failure.CreateGlobalString(F.getName(), "btap.function");
  Value *SourceFile = Failure.CreateGlobalString(File, "btap.file");
  Value *ReportedIndex = Failure.CreateSExtOrTrunc(
      Index, Type::getInt64Ty(C), "btap.reported_index");
  Failure.CreateCall(Report, {ReportedIndex,
                              ConstantInt::get(Type::getInt64Ty(C), Info.Bound),
                              FunctionName, SourceFile,
                              ConstantInt::get(Type::getInt32Ty(C), Line),
                              ConstantInt::get(Type::getInt32Ty(C), Column)});
  Failure.CreateUnreachable();
  ThenTerm->eraseFromParent();
}

class BoundsCheckPass : public PassInfoMixin<BoundsCheckPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    if (F.isDeclaration() || F.getName() == "btap_report_oob" ||
        F.hasFnAttribute(CheckedAttribute))
      return PreservedAnalyses::all();

    SmallVector<AccessInfo, 8> Accesses;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto Access = findSupportedAccess(I))
          if (!isKnownSafeConstant(Access->Index, Access->Bound))
            Accesses.push_back(*Access);

    if (Accesses.empty())
      return PreservedAnalyses::all();

    FunctionCallee Report = getReportFunction(*F.getParent());
    for (const AccessInfo &Access : Accesses)
      insertCheck(Access, Report);

    // This function-level marker makes applying the plugin twice idempotent.
    F.addFnAttr(CheckedAttribute);
    return PreservedAnalyses::none();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "BTAPBoundsCheck", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name != "btap-bounds-check")
                    return false;
                  FPM.addPass(BoundsCheckPass());
                  return true;
                });
          }};
}
