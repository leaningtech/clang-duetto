// RUN: %clang -target mips-unknown-linux-gnu -S -emit-llvm -o - %s | FileCheck -check-prefix=O32 %s
// RUN: %clang -target mipsel-unknown-linux-gnu -S -emit-llvm -o - %s | FileCheck -check-prefix=O32 %s
// RUN: %clang -target mips64-unknown-linux-gnu -S -emit-llvm -o - %s -mabi=n32 | FileCheck -check-prefix=N32 %s
// RUN: %clang -target mips64el-unknown-linux-gnu -S -emit-llvm -o - %s -mabi=n32 | FileCheck -check-prefix=N32 %s
// RUN: %clang -target mips64-unknown-linux-gnu -S -emit-llvm -o - %s | FileCheck -check-prefix=N64 %s
// RUN: %clang -target mips64el-unknown-linux-gnu -S -emit-llvm -o - %s | FileCheck -check-prefix=N64 %s

// O32: define void @fn28(%struct._Z2T2* noalias sret %agg.result, i8 signext %arg0)
// N32: define void @fn28(i8 signext %arg0)
// N64: define void @fn28(i8 signext %arg0)

typedef struct T2 {  } T2;
T2 T2_retval;
T2 fn28(char arg0) {
  return T2_retval;
}
