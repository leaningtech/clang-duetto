// RUN: %clang_cc1 %s -triple=x86_64-apple-darwin10 -emit-llvm -o - | FileCheck %s
struct A { 
  void f(); 
  
  int a;
};

struct B : A { 
  double b;
};

void f() {
  B b;
  
  b.f();
}

// CHECK: define %struct._Z1B* @_Z1fP1A(%struct._Z1A* %a) [[NUW:#[0-9]+]]
B *f(A *a) {
  // CHECK-NOT: br label
  // CHECK: ret %struct._Z1B*
  return static_cast<B*>(a);
}

// PR5965
namespace PR5965 {

// CHECK: define %struct._Z1A* @_ZN6PR59651fEP1B(%struct._Z1B* %b) [[NUW]]
A *f(B* b) {
  // CHECK-NOT: br label
  // CHECK: ret %struct._Z1A*
  return b;
}

}

// Don't crash on a derived-to-base conversion of an r-value
// aggregate.
namespace test3 {
  struct A {};
  struct B : A {};

  void foo(A a);
  void test() {
    foo(B());
  }
}

// CHECK: attributes [[NUW]] = { nounwind{{.*}} }
