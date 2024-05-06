pushd `pwd`
mkdir -p build
cd build
cmake -DCMAKE_INSTALL_PREFIX=`pwd`/../llvm/install ../lib
make install
popd

