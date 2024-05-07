pushd `pwd`
cd llvm-project
mkdir -p build
mkdir -p install
cd build
cmake -DCMAKE_INSTALL_PREFIX=`pwd`/../install ../llvm
make install -j 8
popd

