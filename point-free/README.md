# Clang LibTool point-free

cmake -DLLVM_ENABLE_RTTI:BOOL=TRUE -DLLVM_ENABLE_EH:BOOL=TRUE ../llvm
make point-free
