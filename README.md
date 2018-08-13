# Point-Free Libtool  

Copyright (c) 2018 Andrew Gozillon & Paul Keir, University of the West of Scotland.

## The Tool 

The Point-Free Clang Libtool is a console application that will convert pointful template metafunctions to point-free template metafunction classes (MFC). The output point-free template metafunctions utilise an `m_invoke` typedef member template, which is compatible with the `eval` alias template from the Curtains metaprogramming library. The Curtains library allows implicit currying of MFCs. In some cases the generated MFCs are more concise than the original pointful implementations. The project is inspired by the Haskell **pointfree** tool (http://hackage.haskell.org/package/pointfree).

## What is a Point-Free function?

Point-Free programming is a style of programming where functions are designed to take no arguments (the opposite would be a pointful function that takes arguments, each parameter a "point"). Instead, functions are created through the composition or partial application of a curated set of higher-order functions or combinators. The final composition of these combinators will still accept the same number of parameters; and manipulate them to produce an identical result. This style of programming, also known as tacit programming, can lead to more concise and equational functions and is encountered in functional programming languages like Haskell.

The following simple example converts a pointful lambda function to a point-free function using the Haskell **pointfree** command-line tool. The result, `const`, assumes that the user has the `const` combinator available to them; though note that **pointfree** uses only a subset of functions from the Haskell prelude.

```
$ pf \x y -> x
const
```

The result from a similar example makes use of another elementary combinator, `id`; and also relies on Haskell's implicit currying:

```
$ pf \x y -> y
const id
```
## Point-Free Template Metaprogramming?

As with the Haskell **pointfree** tool, users of our tool are encouraged to have fun, and explore the point-free idiom; potentially re-using existing metafunction combinators from the Curtains library. 

Our Point-Free tool expects the user to provide three things: a file name; the name of a class template (i.e. the metafunction); and the name of the typedef member containing the metafunction result (the name `type` is used by default). By providing the class template within a file, we allow the user to make use of auxiliary classes in the definition in the class template's definition.

Everything following "--" is an argument directed towards the Clang compiler rather than the tool itself. In this case we've elected to set the standard and pass the Curtains library to it.

The following C++ code excert can be compared to the first Haskell example above. Here the *pointful* metafunction class template `First` ``returns'' the first template argument via the `type` member.

```
template <class T, class U>
struct First { using type = T; };
```

Should the `First` definition exist within a file called TemplateTest.cpp, the following invocation of the Point-Free libtool will output `const_` - being an elementary MFC analogue of the Haskell `const` within the Curtains library:

```
$ point-free TemplateTest.cpp -structorclass=First -typealiasordef=type -- -std=c++17 -I ~/projects/curtains
const_

We are then able to make the following two assertions:

```
static_assert(std::is_same_v<First<int,char>::type,int>);
static_assert(std::is_same_v<First<int,char>::type,eval<const_,int,char>>);
```

Reproducing the *second* Haskell example will likewise also involve the common `id` combinator. More significantly though, `const id` is a curried expression, and the result makes use of the intrinsic currying offered by the `eval` alias template from the Curtains library.

```
template <class T, class U>
struct Second { using type = U; };
```

So, with the `Second` class template definition above, also located within TemplateTest.cpp, the following invocation will produce the expected result:

```
$ point-free TemplateTest.cpp -structorclass=Second -typealiasordef=type -- -std=c++17 -I ~/projects/curtains
eval<const_,id>
```

We are then able to make the following two assertions:

```
static_assert(std::is_same_v<Second<int,char>::type,char>);
static_assert(std::is_same_v<Second<int,char>::type,eval<eval<const_,id>,int,char>>);
```

## How Can This Benefit Template Metaprogramming?  

In the case of templates point-free metafunctions can allow for more concise definitions and in some cases the resulting point-free metafunction can be used in lieu of type level lambdas, a feature modern C++ currently does not have (which in itself aids concise code).

For example, we have the following template metafunction that can be used to perform a fold left operation on types (it makes use of some Curtains library features): 

```
template <class F, class V, class XS>
struct fldl {
  template <class X, class G>
  struct s1 {
    template <class A>
    struct s2 {
      using type = eval<G,eval<F,A,X>>;
    };
    using type = curtains::quote_c<s2>;
  };
  using type = eval<foldr, curtains::quote_c<s1>,id,XS,V>;
};
```

This can be condensed into a one line nameless point-free metafunction using the Curtains library, which can then be used similarly to a lambda:    

```
eval<eval<compose,flip>,eval<eval<flip,eval<eval<compose,quote_c<foldr_c>>,eval<eval<compose,eval<compose,eval<flip,compose>>>,flip>>>,quote<id_t>>>;
```

All it takes to invoke this is to apply another eval operation with the required number of arguments. However, this is clearly not easy to write by hand and that's where the Point-Free Libtool comes in, you can invoke the tool on the above example and attain the same result (in actuality the above example could be made more concise, there are more eval operations than necessary).   

## Building

This project needs to be compiled in conjunction with the Clang/LLVM compiler (https://github.com/llvm-mirror/clang & https://github.com/llvm-mirror/llvm).

If you follow the steps provided in Clangs "Getting Started" tutorial (https://clang.llvm.org/get_started.html) then you simply need to copy the contents of this repository into the Tools/Extra directory of the Clang project (The Extra directory is an optional directory of Clang, Step 4 of Clangs "Getting Started" gives direction on where to place it). However, I wouldn't recommend overwriting Clangs existing CMakeLists file with the one from this repository, instead copy the relevant instructions into the file from the Clang repository.  

Once the Point-Free folder is in the correct place and your on Step 7 of Clangs "Getting Started" tutorial you must add a few additional compile flags when invoking cmake (namely runtime type information and exception handling):
```
cmake -DLLVM_ENABLE_RTTI:BOOL=TRUE -DLLVM_ENABLE_EH:BOOL=TRUE ../llvm
```
Afterwards you can simply invoke: 
```
make point-free
```
instead of: 
```
make clang
```

It is notable that as this tool is a Libtool it's possible that it may have some minor inconsistency with later versions of Clang (Clang 6.0 should work), these differences should be small and will manifest as errors during compilation. If any are found feel free to email: andrew.gozillon@uws.ac.uk or submit a pull request if you fix them yourself!

## Links 

Curtains API Repository: https://bitbucket.org/pgk/curtains
 
Test File Repository: https://github.com/agozillon/point-free_tests
 
