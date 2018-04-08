# Clang LibTool point-free

Complete the first four steps of https://clang.llvm.org/get_started.html 

1) Checkout the Point-free project
2) Place the point-free folder and CMakeLists.txt contained within into the llvm/tools/clang/tools/extra/ folder.
Note: The CMakeLists.txt file may be older than the one checked out with extra, so it might be better to take all lines relating to the point-free project from the Point-free projects CMakeLists.txt and merging it with the llvm/tools/clang/tools/extra/ CMakeLists.txt file. This at the moment is as simple as adding add_subdirectory(point-free)  

3) Enter the build folder and enter command: cmake -DLLVM_ENABLE_RTTI:BOOL=TRUE -DLLVM_ENABLE_EH:BOOL=TRUE ../llvm 

Note: As long as EH and RTTI are enabled it should compile, for example the following command also works: cmake ../llvm -DLLVM_ENABLE_RTTI:BOOL=TRUE -DLLVM_ENABLE_EH:BOOL=TRUE -DLLVM_USE_LINKER=gold -DCMAKE_INSTALL_PREFIX=$NEW_LLVM/install -DCMAKE_BUILD_TYPE=Release

4) Then after all configuartion files have compiled: make point-free


