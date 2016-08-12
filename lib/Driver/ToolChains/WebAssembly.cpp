//===--- WebAssembly.cpp - WebAssembly ToolChain Implementation -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "WebAssembly.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang::driver::toolchains;
using namespace clang;
using namespace llvm::opt;

wasm::Linker::Linker(const ToolChain &TC)
    : GnuTool("wasm::Linker", "lld", TC) {}

/// Following the conventions in https://wiki.debian.org/Multiarch/Tuples,
/// we remove the vendor field to form the multiarch triple.
static std::string getMultiarchTriple(const Driver &D,
                                      const llvm::Triple &TargetTriple,
                                      StringRef SysRoot) {
    return (TargetTriple.getArchName() + "-" +
            TargetTriple.getOSAndEnvironmentName()).str();
}

bool wasm::Linker::isLinkJob() const { return true; }

bool wasm::Linker::hasIntegratedCPP() const { return false; }

void wasm::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                const InputInfo &Output,
                                const InputInfoList &Inputs,
                                const ArgList &Args,
                                const char *LinkingOutput) const {

  const ToolChain &ToolChain = getToolChain();
  const char *Linker = Args.MakeArgString(ToolChain.GetLinkerPath());
  ArgStringList CmdArgs;

  if (Args.hasArg(options::OPT_s))
    CmdArgs.push_back("--strip-all");

  Args.AddAllArgs(CmdArgs, options::OPT_L);
  Args.AddAllArgs(CmdArgs, options::OPT_u);
  ToolChain.AddFilePathLibArgs(Args, CmdArgs);

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles))
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crt1.o")));

  AddLinkerInputs(ToolChain, Inputs, Args, CmdArgs, JA);

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    if (ToolChain.ShouldLinkCXXStdlib(Args))
      ToolChain.AddCXXStdlibLibArgs(Args, CmdArgs);

    if (Args.hasArg(options::OPT_pthread))
      CmdArgs.push_back("-lpthread");

    CmdArgs.push_back("-lc");
    AddRunTimeLibs(ToolChain, ToolChain.getDriver(), CmdArgs, Args);
  }

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  C.addCommand(llvm::make_unique<Command>(JA, *this, Linker, CmdArgs, Inputs));
}

WebAssembly::WebAssembly(const Driver &D, const llvm::Triple &Triple,
                         const llvm::opt::ArgList &Args)
    : ToolChain(D, Triple, Args) {

  assert(Triple.isArch32Bit() != Triple.isArch64Bit());

  getProgramPaths().push_back(getDriver().getInstalledDir());

  if (getTriple().getOS() == llvm::Triple::UnknownOS) {
    // Theoretically an "unknown" OS should mean no standard libraries, however
    // it could also mean that a custom set of libraries is in use, so just add
    // /lib to the search path. Disable multiarch in this case, to discourage
    // paths containing "unknown" from acquiring meanings.
    getFilePaths().push_back(getDriver().SysRoot + "/lib");
  } else {
    const std::string MultiarchTriple =
        getMultiarchTriple(getDriver(), Triple, getDriver().SysRoot);
    getFilePaths().push_back(getDriver().SysRoot + "/lib/" + MultiarchTriple);
  }
}

bool WebAssembly::IsMathErrnoDefault() const { return false; }

bool WebAssembly::IsObjCNonFragileABIDefault() const { return true; }

bool WebAssembly::UseObjCMixedDispatch() const { return true; }

bool WebAssembly::isPICDefault() const { return false; }

bool WebAssembly::isPIEDefault() const { return false; }

bool WebAssembly::isPICDefaultForced() const { return false; }

bool WebAssembly::IsIntegratedAssemblerDefault() const { return true; }

bool WebAssembly::hasBlocksRuntime() const { return false; }

// TODO: Support profiling.
bool WebAssembly::SupportsProfiling() const { return false; }

bool WebAssembly::HasNativeLLVMSupport() const { return true; }

void WebAssembly::addClangTargetOptions(const ArgList &DriverArgs,
                                        ArgStringList &CC1Args,
                                        Action::OffloadKind) const {
  if (DriverArgs.hasFlag(clang::driver::options::OPT_fuse_init_array,
                         options::OPT_fno_use_init_array, true))
    CC1Args.push_back("-fuse-init-array");
}

ToolChain::RuntimeLibType WebAssembly::GetDefaultRuntimeLibType() const {
  return ToolChain::RLT_CompilerRT;
}

ToolChain::CXXStdlibType
WebAssembly::GetCXXStdlibType(const ArgList &Args) const {
  if (Arg *A = Args.getLastArg(options::OPT_stdlib_EQ)) {
    StringRef Value = A->getValue();
    if (Value != "libc++")
      getDriver().Diag(diag::err_drv_invalid_stdlib_name)
          << A->getAsString(Args);
  }
  return ToolChain::CST_Libcxx;
}

void WebAssembly::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                            ArgStringList &CC1Args) const {
  if (!DriverArgs.hasArg(options::OPT_nostdinc)) {
    if (getTriple().getOS() != llvm::Triple::UnknownOS) {
      const std::string MultiarchTriple =
          getMultiarchTriple(getDriver(), getTriple(), getDriver().SysRoot);
      addSystemInclude(DriverArgs, CC1Args, getDriver().SysRoot + "/include/" + MultiarchTriple);
    }
    addSystemInclude(DriverArgs, CC1Args, getDriver().SysRoot + "/include");
  }
}

void WebAssembly::AddClangCXXStdlibIncludeArgs(const ArgList &DriverArgs,
                                               ArgStringList &CC1Args) const {
  if (!DriverArgs.hasArg(options::OPT_nostdlibinc) &&
      !DriverArgs.hasArg(options::OPT_nostdincxx)) {
    if (getTriple().getOS() != llvm::Triple::UnknownOS) {
      const std::string MultiarchTriple =
          getMultiarchTriple(getDriver(), getTriple(), getDriver().SysRoot);
      addSystemInclude(DriverArgs, CC1Args,
                       getDriver().SysRoot + "/include/" + MultiarchTriple + "/c++/v1");
    }
    addSystemInclude(DriverArgs, CC1Args,
                     getDriver().SysRoot + "/include/c++/v1");
  }
}

void WebAssembly::AddCXXStdlibLibArgs(const llvm::opt::ArgList &Args,
                                      llvm::opt::ArgStringList &CmdArgs) const {

  switch (GetCXXStdlibType(Args)) {
  case ToolChain::CST_Libcxx:
    CmdArgs.push_back("-lc++");
    CmdArgs.push_back("-lc++abi");
    break;
  case ToolChain::CST_Libstdcxx:
    llvm_unreachable("invalid stdlib name");
  }
}

std::string WebAssembly::getThreadModel() const {
  // The WebAssembly MVP does not yet support threads; for now, use the
  // "single" threading model, which lowers atomics to non-atomic operations.
  // When threading support is standardized and implemented in popular engines,
  // this override should be removed.
  return "single";
}

Tool *WebAssembly::buildLinker() const {
  return new tools::wasm::Linker(*this);
}

void cheerp::Link::ConstructJob(Compilation &C, const JobAction &JA,
                                const InputInfo &Output,
                                const InputInfoList &Inputs,
                                const ArgList &Args,
                                const char *LinkingOutput) const {
  ArgStringList CmdArgs;

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  for (InputInfoList::const_iterator
         it = Inputs.begin(), ie = Inputs.end(); it != ie; ++it) {
    const InputInfo &II = *it;
    if(II.isFilename())
      CmdArgs.push_back(II.getFilename());
  }

  // Add standard libraries
  if (!Args.hasArg(options::OPT_nostdlib) &&
      !Args.hasArg(options::OPT_nodefaultlibs)) {
    if (C.getDriver().CCCIsCXX()) {
      CmdArgs.push_back(LLVM_PREFIX "/lib/libstdlibs.bc");
    } else {
      CmdArgs.push_back(LLVM_PREFIX "/lib/libc.bc");
      CmdArgs.push_back(LLVM_PREFIX "/lib/libm.bc");
    }
  }

  // Do not add the same library more than once
  std::set<std::string> usedLibs;
  for (arg_iterator it = Args.filtered_begin(options::OPT_l),
         ie = Args.filtered_end(); it != ie; ++it) {
    std::string libName("lib");
    libName += (*it)->getValue();
    libName += ".bc";
    const std::string& foundLib = getToolChain().GetFilePath(libName.c_str());
    if (usedLibs.count(foundLib))
      continue;
    usedLibs.insert(foundLib);
    CmdArgs.push_back(Args.MakeArgString(foundLib));
  }

  const char *Exec = Args.MakeArgString((getToolChain().GetProgramPath("llvm-link")));
  C.addCommand(llvm::make_unique<Command>(JA, *this, Exec, CmdArgs));
}

void cheerp::CheerpOptimizer::ConstructJob(Compilation &C, const JobAction &JA,
                                          const InputInfo &Output,
                                          const InputInfoList &Inputs,
                                          const ArgList &Args,
                                          const char *LinkingOutput) const {
  ArgStringList CmdArgs;

  CmdArgs.push_back("-march=cheerp");
  if(Args.hasArg(options::OPT_cheerp_preexecute))
    CmdArgs.push_back("-PreExecute");
  CmdArgs.push_back("-GlobalDepsAnalyzer");
  if(!Args.hasArg(options::OPT_cheerp_no_type_optimizer))
    CmdArgs.push_back("-TypeOptimizer");
  CmdArgs.push_back("-ReplaceNopCastsAndByteSwaps");
  CmdArgs.push_back("-Os");
  CmdArgs.push_back("-DelayAllocas");
  // Inlining from -Os may generate memcpy calls that we need to lower
  CmdArgs.push_back("-StructMemFuncLowering");
  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  const InputInfo &II = *Inputs.begin();
  CmdArgs.push_back(II.getFilename());

  // Honor -mllvm
  Args.AddAllArgValues(CmdArgs, options::OPT_mllvm);
  // Honot -cheerp-no-pointer-scev
  if (Arg *CheerpNoPointerSCEV = Args.getLastArg(options::OPT_cheerp_no_pointer_scev))
    CheerpNoPointerSCEV->render(Args, CmdArgs);

  const char *Exec = Args.MakeArgString((getToolChain().GetProgramPath("opt")));
  C.addCommand(llvm::make_unique<Command>(JA, *this, Exec, CmdArgs));
}

void cheerp::CheerpCompiler::ConstructJob(Compilation &C, const JobAction &JA,
                                          const InputInfo &Output,
                                          const InputInfoList &Inputs,
                                          const ArgList &Args,
                                          const char *LinkingOutput) const {
  ArgStringList CmdArgs;

  CmdArgs.push_back("-march=cheerp");
  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  if(Arg* cheerpSourceMap = Args.getLastArg(options::OPT_cheerp_sourcemap_EQ))
    cheerpSourceMap->render(Args, CmdArgs);
  if(Arg* cheerpSourceMapPrefix = Args.getLastArg(options::OPT_cheerp_sourcemap_prefix_EQ))
    cheerpSourceMapPrefix->render(Args, CmdArgs);
  if(Arg* cheerpPrettyCode = Args.getLastArg(options::OPT_cheerp_pretty_code))
    cheerpPrettyCode->render(Args, CmdArgs);
  if(Arg* cheerpMakeModule = Args.getLastArg(options::OPT_cheerp_make_module))
    cheerpMakeModule->render(Args, CmdArgs);
  if(Arg* cheerpNoNativeMath = Args.getLastArg(options::OPT_cheerp_no_native_math))
    cheerpNoNativeMath->render(Args, CmdArgs);
  if(Arg* cheerpNoMathImul = Args.getLastArg(options::OPT_cheerp_no_math_imul))
    cheerpNoMathImul->render(Args, CmdArgs);
  if(Arg* cheerpNoCredits = Args.getLastArg(options::OPT_cheerp_no_credits))
    cheerpNoCredits->render(Args, CmdArgs);
  if(Arg* cheerpReservedNames = Args.getLastArg(options::OPT_cheerp_reserved_names_EQ))
    cheerpReservedNames->render(Args, CmdArgs);

  const InputInfo &II = *Inputs.begin();
  CmdArgs.push_back(II.getFilename());

  // Honor -mllvm
  Args.AddAllArgValues(CmdArgs, options::OPT_mllvm);

  const char *Exec = Args.MakeArgString((getToolChain().GetProgramPath("llc")));
  C.addCommand(llvm::make_unique<Command>(JA, *this, Exec, CmdArgs));
}
