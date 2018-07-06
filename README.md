# Point-Free Libtool  

Copyright (c) 2018 Andrew Gozillon & Paul Keir, University of the West of Scotland.

## The Tool 

The Point-Free Clang Libtool is a console application that will convert pointful template metafunctions to point-free template metafunctions. These new point-free template metafunctions can be used as type level lambdas and in some cases are more concise than the original pointful implementations. 

These point-free metafunctions are enabled by a metaprogramming library called Curtains that allows the currying of templates and contains several useful combinator functions.   

## What is Point-Free?   

Point-Free programming is a style of programming where functions are designed to take no arguments (the opposite would be a pointful function that takes arguments, each parameter a "point"). Instead meaningful functions are created through the composition and chaining of multiple higher-order functions or combinators through currying. These combinator functions themselves can take parameters and usually manipulate there inputs in some way; for example getting rid of some of there arguments. The end composition of these curried functions will take a number of parameters and manipulate the parameters to get the end result. This style of programming can lead to more concise and equational functions and is found in functional programming languages like Haskell.  

A simple Haskell example of a pointful function (left of =>) to a point-free function (right of =>) would be the following:
```
\x y -> y => const id
```
The pointful function is a simple lambda that takes two arguments and returns the second. The point-free version of this is two curried together functions; const which takes two arguments and discards the second, returning the first and id which returns its argument. When combined they make curried function that takes two arguments and returns the second.  
     
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

eval<eval<compose,flip>,eval<eval<flip,eval<eval<compose,quote_c<foldr_c>>,eval<eval<compose,eval<compose,eval<flip,compose>>>,flip>>>,quote<id_t>>>;

All it takes to invoke this is to apply another eval operation with the required number of arguments. However, this is clearly not easy to write by hand and that's where the Point-Free Libtool comes in, you can invoke the tool on the above example and attain the same result (in actuallity the above example could be made more concise, there are more eval operations than neccessary).   

## How to compile it 

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

It is notable that as this tool is a Libtool it's possible that it may have some minor inconsistinces with later versions of Clang (Clang 6.0 should work), these differences should be small and will manifest as errors during compilation. If any are found feel free to email: andrew.gozillon@uws.ac.uk or submit a pull request if you fix them yourself!

## How to use it 

Using the tool is relatively simple and is done through the command line, an example command to the tool can be found below (it makes use of the fldl example above).
```  
point-free TemplateTest.cpp -structorclass=fldl -typealiasordef=type -- -std=c++17 -I ~/projects/curtains
```
The first argument to the point-free executeable is the source code the template you wish to convert is contained in. The second argument is the name of the template structure or class that you wish converted. The third argument is the type alias or type definition you wish converted within the template, this defaults to "type" if no argument is given. Everything following "--" is arguments directed towards the Clang compiler rather than the tool itself, in this case we've elected to set the standard and pass the Curtains library to it. Once the command has been entered the result of the conversion will be printed to screen. 

## Links 

Curtains API Repository: 
Test File Repository:
 