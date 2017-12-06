
set(LLVM_LINK_COMPONENTS
  Option
  Support
  AsmParser
  BitReader
  MC
  )

add_clang_tool(point-free
 PointFree.cpp
 test.c  # Remove test files later, seeing if I can get around having them in the same folder and not crashing cmake.
 test-functions.cpp 
)

target_link_libraries(point-free
  PRIVATE
  clangAST
  clangAnalysis
  clangBasic
  clangDriver
  clangEdit  
  clangFormat
  clangFrontend
  clangLex
  clangParse
  clangRewrite
  clangRewriteCore
  clangRewriteFrontend
  clangSerialization
  clangSema
  clangTooling
  clangToolingCore
  clangToolingRefactor
  )

install(TARGETS point-free RUNTIME DESTINATION bin)
	
