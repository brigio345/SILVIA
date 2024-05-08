pushd `pwd`
cd llvm-project
mkdir -p build
mkdir -p install
cd build
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${XILINX_VITIS}/tps/lnx64/cmake-3.24.2/libs/Ubuntu
${XILINX_VITIS}/tps/lnx64/cmake-3.24.2/bin/cmake \
	-DCMAKE_INSTALL_PREFIX=`pwd`/../install \
	-DCMAKE_CXX_FLAGS="-w -std=c++98" \
	-DPYTHON_INCLUDE_DIR=${XILINX_VITIS}/tps/lnx64/python-2.7.5/include/python2.7 \
	-DPYTHON_EXECUTABLE=${XILINX_VITIS}/tps/lnx64/python-2.7.5/bin/python2 \
	../llvm
make install -j 8
popd
