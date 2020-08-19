// RUN: %clang_cc1 -triple x86_64-darwin -std=c++11 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-darwin -std=c++11 -fcxx-exceptions -fexceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=EXCEPTIONS

// PR36748
// rdar://problem/45805151

// Classes to verify order of destroying function parameters.
struct S1 {
  ~S1();
};
struct S2 {
  ~S2();
};

struct Base {
  // Use variadic args to cause inlining the inherited constructor.
  Base(const S1&, const S2&, const char *fmt, ...) {}
};

struct NonTrivialDtor {
  ~NonTrivialDtor() {}
};
struct Inheritor : public NonTrivialDtor, public Base {
  using Base::Base;
};

void f() {
  Inheritor(S1(), S2(), "foo");
  // CHECK-LABEL: define void @_Z1fv
  // CHECK: %[[TMP1:.*]] = alloca %struct._Z2S1
  // CHECK: %[[TMP2:.*]] = alloca %struct._Z2S2
  // CHECK: call void (%struct._Z4Base*, %struct._Z2S1*, %struct._Z2S2*, i8*, ...) @_ZN4BaseC2ERK2S1RK2S2PKcz(%struct._Z4Base* {{.*}}, %struct._Z2S1* dereferenceable(1) %[[TMP1]], %struct._Z2S2* dereferenceable(1) %[[TMP2]], i8* {{.*}})
  // CHECK-NEXT: call void @_ZN9InheritorD1Ev(%struct._Z9Inheritor* {{.*}})
  // CHECK-NEXT: call void @_ZN2S2D1Ev(%struct._Z2S2* %[[TMP2]])
  // CHECK-NEXT: call void @_ZN2S1D1Ev(%struct._Z2S1* %[[TMP1]])

  // EXCEPTIONS-LABEL: define void @_Z1fv
  // EXCEPTIONS: %[[TMP1:.*]] = alloca %struct._Z2S1
  // EXCEPTIONS: %[[TMP2:.*]] = alloca %struct._Z2S2
  // EXCEPTIONS: invoke void (%struct._Z4Base*, %struct._Z2S1*, %struct._Z2S2*, i8*, ...) @_ZN4BaseC2ERK2S1RK2S2PKcz(%struct._Z4Base* {{.*}}, %struct._Z2S1* dereferenceable(1) %[[TMP1]], %struct._Z2S2* dereferenceable(1) %[[TMP2]], i8* {{.*}})
  // EXCEPTIONS-NEXT: to label %[[CONT:.*]] unwind label %[[LPAD:.*]]

  // EXCEPTIONS: [[CONT]]:
  // EXCEPTIONS-NEXT: call void @_ZN9InheritorD1Ev(%struct._Z9Inheritor* {{.*}})
  // EXCEPTIONS-NEXT: call void @_ZN2S2D1Ev(%struct._Z2S2* %[[TMP2]])
  // EXCEPTIONS-NEXT: call void @_ZN2S1D1Ev(%struct._Z2S1* %[[TMP1]])

  // EXCEPTIONS: [[LPAD]]:
  // EXCEPTIONS: call void @_ZN14NonTrivialDtorD2Ev(%struct._Z14NonTrivialDtor* {{.*}})
  // EXCEPTIONS-NEXT: call void @_ZN2S2D1Ev(%struct._Z2S2* %[[TMP2]])
  // EXCEPTIONS-NEXT: call void @_ZN2S1D1Ev(%struct._Z2S1* %[[TMP1]])
}
