LLVM_INSTALL_PATH=`pwd`/llvm-project/install

pushd `pwd`
mkdir -p build
cd build
export LD_LIBRARY_PATH=${XILINX_VITIS}/tps/lnx64/cmake-3.24.2/libs/Ubuntu:${LD_LIBRARY_PATH}
${XILINX_VITIS}/tps/lnx64/cmake-3.24.2/bin/cmake \
	-DCMAKE_INSTALL_PREFIX=${LLVM_INSTALL_PATH} \
	-DCMAKE_CXX_FLAGS="-std=c++11" \
	../lib
make install
popd

