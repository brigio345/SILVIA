pushd `pwd`
cd llvm-project
mkdir -p build
mkdir -p install
cd build
${XILINX_VITIS}/tps/lnx64/cmake-3.24.2/bin/cmake \
	-DCMAKE_INSTALL_PREFIX=`pwd`/../install \
	-DCMAKE_C_COMPILER=${XILINX_HLS}/tps/lnx64/gcc-6.2.0/bin/gcc \
	-DCMAKE_CXX_COMPILER=${XILINX_HLS}/tps/lnx64/gcc-6.2.0/bin/g++ \
	-DCMAKE_CXX_FLAGS="-w -std=c++98" \
	-DPYTHON_INCLUDE_DIR=${XILINX_VITIS}/tps/lnx64/python-2.7.5/include/python2.7 \
	-DPYTHON_EXECUTABLE=${XILINX_VITIS}/tps/lnx64/python-2.7.5/bin/python2 \
../llvm
make install -j 8
popd

