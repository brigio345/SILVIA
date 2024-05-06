pushd `pwd`
mkdir -p llvm
cd llvm
wget https://releases.llvm.org/3.1/llvm-3.1.src.tar.gz
tar -xf llvm-3.1.src.tar.gz
wget https://releases.llvm.org/3.1/clang-3.1.src.tar.gz
tar -xf clang-3.1.src.tar.gz
mv clang-3.1.src llvm-3.1.src/tools/clang
mkdir -p build
mkdir -p install
cd build
cmake -DCMAKE_INSTALL_PREFIX=`pwd`/../install ../llvm-3.1.src
make install -j 8
popd

