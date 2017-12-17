// clang++ -std=c++11 -Xclang -ast-dump -fsyntax-only
// point-free TemplateTest.cpp -- -std=c++11

/*
template <typename T>
struct eval {
    using type = T;
};

template <typename T>
struct quote {
    using type = T;
};
*/
template <typename X, typename Y>  
struct Foo {
    using type = X;//eval<X>;
    //typedef X type;
};

//template <typename X, typename Y>
//using Foo_t = typename Foo<X,Y>::type;
//using Foo_q = quote<Foo_t>;
