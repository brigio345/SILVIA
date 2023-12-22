import pynq
from pynq import Overlay
from pynq import allocate
import numpy as np
import sys

np.set_printoptions(threshold=sys.maxsize)

overlay = Overlay("/home/ubuntu/overlay0/design_1.bit")

N = 2048

a = allocate(shape=(N,), dtype="<i1")
b = allocate(shape=(N,), dtype="<i1")
c = allocate(shape=(N,), dtype="<i4")
dout = allocate(shape=(N,), dtype="<i4")

for i in range(N):
    a[i] = random_number = np.random.randint(-128, 128)
    b[i] = random_number = np.random.randint(-128, 128)
    c[i] = random_number = np.random.randint(-128, 128)
    dout[i] = -1

a.sync_to_device()
b.sync_to_device()
c.sync_to_device()

overlay.dut_0.write(0x10, a.device_address)
overlay.dut_0.write(0x14, a.device_address >> 32)

overlay.dut_0.write(0x1C, b.device_address)
overlay.dut_0.write(0x20, b.device_address >> 32)

overlay.dut_0.write(0x28, c.device_address)
overlay.dut_0.write(0x2C, c.device_address >> 32)

overlay.dut_0.write(0x34, dout.device_address)
overlay.dut_0.write(0x38, dout.device_address >> 32)

overlay.dut_0.write(0x00, 1)

while not overlay.dut_0.read(0x00) & 2:
    pass

dout.sync_from_device()

print("\n" + "-" * 80)
print(dout)
print("-" * 80)

print("\n" + "-" * 80)
print("Testing muladd...")
print("-" * 80)
for i in range(N):
    if dout[i] != a[i].astype("<i4") * b[i] + c[i]:
        print(
            "ERROR @ {}: {} != {}".format(i, dout[i], a[i].astype("<i4") * b[i] + c[i])
        )

print("\n" + "-" * 80)
print("DONE")
