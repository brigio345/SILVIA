LLVM_INSTALL_PATH=`pwd`/llvm-project/install

pushd `pwd`
mkdir -p build
cd build
cmake -DCMAKE_INSTALL_PREFIX=${LLVM_INSTALL_PATH} ../lib
make install
popd

