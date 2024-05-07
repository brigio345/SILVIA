LLVM_INSTALL_PATH=`pwd`/llvm-project/install

pushd `pwd`
mkdir -p build
cd build
${XILINX_VITIS}/tps/lnx64/cmake-3.24.2/bin/cmake -DCMAKE_INSTALL_PREFIX=${LLVM_INSTALL_PATH} ../lib
make install
popd

