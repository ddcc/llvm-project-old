// RUN: %clang_cc1 -fsyntax-only -verify %s

struct A {};
enum B { Dummy };
namespace C {}
struct D : A {};
struct E : A {};
struct F : D, E {};
struct G : virtual D {};

int A::*pdi1;
int (::A::*pdi2);
int (A::*pfi)(int);

int B::*pbi; // expected-error {{expected a class or namespace}}
int C::*pci; // expected-error {{'pci' does not point into a class}}
void A::*pdv; // expected-error {{'pdv' declared as a member pointer to void}}
int& A::*pdr; // expected-error {{'pdr' declared as a member pointer to a reference}}

void f() {
  // This requires tentative parsing.
  int (A::*pf)(int, int);

  // Implicit conversion to bool.
  bool b = pdi1;
  b = pfi;

  // Conversion from null pointer constant.
  pf = 0;
  pf = __null;

  // Conversion to member of derived.
  int D::*pdid = pdi1;
  pdid = pdi2;

  // Fail conversion due to ambiguity and virtuality.
  int F::*pdif = pdi1; // expected-error {{ambiguous conversion from pointer to member of base class 'A' to pointer to member of derived class 'F':}}
  int G::*pdig = pdi1; // expected-error {{conversion from pointer to member of class 'A' to pointer to member of class 'G' via virtual base 'D' is not allowed}}

  // Conversion to member of base.
  pdi1 = pdid; // expected-error {{assigning to 'int A::*' from incompatible type 'int D::*'}}
  
  // Comparisons
  int (A::*pf2)(int, int);
  int (D::*pf3)(int, int) = 0;
  bool b1 = (pf == pf2); (void)b1;
  bool b2 = (pf != pf2); (void)b2;
  bool b3 = (pf == pf3); (void)b3;
  bool b4 = (pf != 0); (void)b4;
}

struct TheBase
{
  void d();
};

struct HasMembers : TheBase
{
  int i;
  void f();

  void g();
  void g(int);
  static void g(double);
};

namespace Fake
{
  int i;
  void f();
}

void g() {
  HasMembers hm;

  int HasMembers::*pmi = &HasMembers::i;
  int *pni = &Fake::i;
  int *pmii = &hm.i;

  void (HasMembers::*pmf)() = &HasMembers::f;
  void (*pnf)() = &Fake::f;
  &hm.f; // expected-error {{cannot create a non-constant pointer to member function}}

  void (HasMembers::*pmgv)() = &HasMembers::g;
  void (HasMembers::*pmgi)(int) = &HasMembers::g;
  void (*pmgd)(double) = &HasMembers::g;

  void (HasMembers::*pmd)() = &HasMembers::d;
}

struct Incomplete; // expected-note {{forward declaration}}

void h() {
  HasMembers hm, *phm = &hm;

  int HasMembers::*pi = &HasMembers::i;
  hm.*pi = 0;
  int i = phm->*pi;
  (void)&(hm.*pi);
  (void)&(phm->*pi);
  (void)&((&hm)->*pi); 

  void (HasMembers::*pf)() = &HasMembers::f;
  (hm.*pf)();
  (phm->*pf)();

  (void)(hm->*pi); // expected-error {{left hand operand to ->* must be a pointer to class compatible with the right hand operand, but is 'HasMembers'}}
  (void)(phm.*pi); // expected-error {{left hand operand to .* must be a class compatible with the right hand operand, but is 'HasMembers *'}}
  (void)(i.*pi); // expected-error {{left hand operand to .* must be a class compatible with the right hand operand, but is 'int'}}
  int *ptr;
  (void)(ptr->*pi); // expected-error {{left hand operand to ->* must be a pointer to class compatible with the right hand operand, but is 'int *'}}

  int A::*pai = 0;
  D d, *pd = &d;
  (void)(d.*pai);
  (void)(pd->*pai);
  F f, *ptrf = &f;
  (void)(f.*pai); // expected-error {{left hand operand to .* must be a class compatible with the right hand operand, but is 'F'}}
  (void)(ptrf->*pai); // expected-error {{left hand operand to ->* must be a pointer to class compatible with the right hand operand, but is 'F *'}}

  (void)(hm.*i); // expected-error {{pointer-to-member}}
  (void)(phm->*i); // expected-error {{pointer-to-member}}

  Incomplete *inc;
  int Incomplete::*pii = 0;
  (void)(inc->*pii); // expected-error {{pointer into incomplete}}
}

struct OverloadsPtrMem
{
  int operator ->*(const char *);
};

void i() {
  OverloadsPtrMem m;
  int foo = m->*"Awesome!";
}

namespace pr5985 {
  struct c {
    void h();
    void f() {
      void (c::*p)();
      p = &h; // expected-error {{must explicitly qualify}}
      p = &this->h; // expected-error {{cannot create a non-constant pointer to member function}}
      p = &(*this).h; // expected-error {{cannot create a non-constant pointer to member function}}
    }
  };
}

namespace pr6783 {
  struct Base {};
  struct X; // expected-note {{forward declaration}}

  int test1(int Base::* p2m, X* object)
  {
    return object->*p2m; // expected-error {{left hand operand to ->*}}
  }
}

namespace PR7176 {
  namespace base
  {
    struct Process
    { };
    struct Continuous : Process
    {
      bool cond();
    };
  }

  typedef bool( base::Process::*Condition )();

  void m()
  { (void)(Condition) &base::Continuous::cond; }
}

namespace rdar8358512 {
  // We can't call this with an overload set because we're not allowed
  // to look into overload sets unless the parameter has some kind of
  // function type.
  template <class F> void bind(F f); // expected-note 6 {{candidate template ignored}}
  template <class F, class T> void bindmem(F (T::*f)()); // expected-note 4 {{candidate template ignored}}
  template <class F> void bindfn(F (*f)()); // expected-note 4 {{candidate template ignored}}

  struct A {
    void member();

    void nonstat();
    void nonstat(int);

    void mixed();
    static void mixed(int);

    static void stat();
    static void stat(int);
    
    template <typename T> struct Test0 {
      void test() {
        bind(&nonstat); // expected-error {{no matching function for call}}
        bind(&A::nonstat); // expected-error {{no matching function for call}}

        bind(&mixed); // expected-error {{no matching function for call}}
        bind(&A::mixed); // expected-error {{no matching function for call}}

        bind(&stat); // expected-error {{no matching function for call}}
        bind(&A::stat); // expected-error {{no matching function for call}}
      }
    };

    template <typename T> struct Test1 {
      void test() {
        bindmem(&nonstat); // expected-error {{no matching function for call}}
        bindmem(&A::nonstat);

        bindmem(&mixed); // expected-error {{no matching function for call}}
        bindmem(&A::mixed);

        bindmem(&stat); // expected-error {{no matching function for call}}
        bindmem(&A::stat); // expected-error {{no matching function for call}}
      }
    };

    template <typename T> struct Test2 {
      void test() {
        bindfn(&nonstat); // expected-error {{no matching function for call}}
        bindfn(&A::nonstat); // expected-error {{no matching function for call}}

        bindfn(&mixed); // expected-error {{no matching function for call}}
        bindfn(&A::mixed); // expected-error {{no matching function for call}}

        bindfn(&stat);
        bindfn(&A::stat);
      }
    };
  };
}
