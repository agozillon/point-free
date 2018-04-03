# Clang LibTool point-free

Place this folder (point-free) and the CMakeLists.txt file from the directory above into the Clang/tools/Extra subdirectory. Alternatively do not copy the CMakeLists.txt file and instead make the required modifications to the existing /Extra CMakeLists.txt file to add point-free to the compile path.    
 
cmake -DLLVM_ENABLE_RTTI:BOOL=TRUE -DLLVM_ENABLE_EH:BOOL=TRUE ../llvm
make point-free
