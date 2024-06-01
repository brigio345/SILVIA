; ModuleID = 'add4simd.bc'
target datalayout = "e-m:e-i64:64-i128:128-i256:256-i512:512-i1024:1024-i2048:2048-i4096:4096-n8:16:32:64-S128-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024"
target triple = "fpga64-xilinx-none"

@empty = internal unnamed_addr constant [1 x i8] zeroinitializer

define internal fastcc { i12, i12, i12, i12 } @_simd_add_12b(i12 %a0_val, i12 %b0_val, i12 %a1_val, i12 %b1_val, i12 %a2_val, i12 %b2_val, i12 %a3_val, i12 %b3_val) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty)
  call void (...)* @_ssdm_op_SpecLatency(i64 1, i64 1, [1 x i8]* @empty)
  %b3_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %b3_val) nounwind, !bitwidth !0
  %a3_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %a3_val) nounwind, !bitwidth !0
  %b2_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %b2_val) nounwind, !bitwidth !0
  %a2_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %a2_val) nounwind, !bitwidth !0
  %b1_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %b1_val) nounwind, !bitwidth !0
  %a1_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %a1_val) nounwind, !bitwidth !0
  %b0_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %b0_val) nounwind, !bitwidth !0
  %a0_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %a0_val) nounwind, !bitwidth !0
  %add_ln25 = add i12 %b0_val_read, %a0_val_read, !bitwidth !0
  %add_ln26 = add i12 %b1_val_read, %a1_val_read, !bitwidth !0
  %add_ln27 = add i12 %b2_val_read, %a2_val_read, !bitwidth !0
  %add_ln28 = add i12 %b3_val_read, %a3_val_read, !bitwidth !0
  %mrv = insertvalue { i12, i12, i12, i12 } undef, i12 %add_ln25, 0, !bitwidth !1
  %mrv_1 = insertvalue { i12, i12, i12, i12 } %mrv, i12 %add_ln26, 1, !bitwidth !1
  %mrv_2 = insertvalue { i12, i12, i12, i12 } %mrv_1, i12 %add_ln27, 2, !bitwidth !1
  %mrv_3 = insertvalue { i12, i12, i12, i12 } %mrv_2, i12 %add_ln28, 3, !bitwidth !1
  ret { i12, i12, i12, i12 } %mrv_3, !bitwidth !1
}

define internal fastcc { i24, i24 } @_simd_add_24b(i24 %a0_val, i24 %b0_val, i24 %a1_val, i24 %b1_val) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty)
  call void (...)* @_ssdm_op_SpecLatency(i64 1, i64 1, [1 x i8]* @empty)
  %b1_val_read = call i24 @_ssdm_op_Read.ap_auto.i24(i24 %b1_val) nounwind, !bitwidth !2
  %a1_val_read = call i24 @_ssdm_op_Read.ap_auto.i24(i24 %a1_val) nounwind, !bitwidth !2
  %b0_val_read = call i24 @_ssdm_op_Read.ap_auto.i24(i24 %b0_val) nounwind, !bitwidth !2
  %a0_val_read = call i24 @_ssdm_op_Read.ap_auto.i24(i24 %a0_val) nounwind, !bitwidth !2
  %add_ln25 = add i24 %b0_val_read, %a0_val_read, !bitwidth !2
  %add_ln26 = add i24 %b1_val_read, %a1_val_read, !bitwidth !2
  %mrv = insertvalue { i24, i24 } undef, i24 %add_ln25, 0, !bitwidth !1
  %mrv_1 = insertvalue { i24, i24 } %mrv, i24 %add_ln26, 1, !bitwidth !1
  ret { i24, i24 } %mrv_1, !bitwidth !1
}

define internal fastcc { i12, i12, i12, i12 } @_simd_sub_4(i12 %a0_val, i12 %b0_val, i12 %a1_val, i12 %b1_val, i12 %a2_val, i12 %b2_val, i12 %a3_val, i12 %b3_val) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty)
  call void (...)* @_ssdm_op_SpecLatency(i64 1, i64 1, [1 x i8]* @empty)
  %b3_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %b3_val) nounwind, !bitwidth !0
  %a3_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %a3_val) nounwind, !bitwidth !0
  %b2_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %b2_val) nounwind, !bitwidth !0
  %a2_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %a2_val) nounwind, !bitwidth !0
  %b1_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %b1_val) nounwind, !bitwidth !0
  %a1_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %a1_val) nounwind, !bitwidth !0
  %b0_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %b0_val) nounwind, !bitwidth !0
  %a0_val_read = call i12 @_ssdm_op_Read.ap_auto.i12(i12 %a0_val) nounwind, !bitwidth !0
  %add_ln25 = sub i12 %a0_val_read, %b0_val_read, !bitwidth !0
  %add_ln26 = sub i12 %a1_val_read, %b1_val_read, !bitwidth !0
  %add_ln27 = sub i12 %a2_val_read, %b2_val_read, !bitwidth !0
  %add_ln28 = sub i12 %a3_val_read, %b3_val_read, !bitwidth !0
  %mrv = insertvalue { i12, i12, i12, i12 } undef, i12 %add_ln25, 0, !bitwidth !1
  %mrv_1 = insertvalue { i12, i12, i12, i12 } %mrv, i12 %add_ln26, 1, !bitwidth !1
  %mrv_2 = insertvalue { i12, i12, i12, i12 } %mrv_1, i12 %add_ln27, 2, !bitwidth !1
  %mrv_3 = insertvalue { i12, i12, i12, i12 } %mrv_2, i12 %add_ln28, 3, !bitwidth !1
  ret { i12, i12, i12, i12 } %mrv_3, !bitwidth !1
}

define internal fastcc { i24, i24 } @_simd_sub_2(i24 %a0_val, i24 %b0_val, i24 %a1_val, i24 %b1_val) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty)
  call void (...)* @_ssdm_op_SpecLatency(i64 1, i64 1, [1 x i8]* @empty)
  %b1_val_read = call i24 @_ssdm_op_Read.ap_auto.i24(i24 %b1_val) nounwind, !bitwidth !2
  %a1_val_read = call i24 @_ssdm_op_Read.ap_auto.i24(i24 %a1_val) nounwind, !bitwidth !2
  %b0_val_read = call i24 @_ssdm_op_Read.ap_auto.i24(i24 %b0_val) nounwind, !bitwidth !2
  %a0_val_read = call i24 @_ssdm_op_Read.ap_auto.i24(i24 %a0_val) nounwind, !bitwidth !2
  %add_ln25 = sub i24 %a0_val_read, %b0_val_read, !bitwidth !2
  %add_ln26 = sub i24 %a1_val_read, %b1_val_read, !bitwidth !2
  %mrv = insertvalue { i24, i24 } undef, i24 %add_ln25, 0, !bitwidth !1
  %mrv_1 = insertvalue { i24, i24 } %mrv, i24 %add_ln26, 1, !bitwidth !1
  ret { i24, i24 } %mrv_1, !bitwidth !1
}

define weak void @_ssdm_op_SpecPipeline(...) nounwind {
entry:
  ret void
}

define weak void @_ssdm_op_SpecLatency(...) nounwind {
entry:
  ret void
}

define weak i12 @_ssdm_op_Read.ap_auto.i12(i12) {
entry:
  ret i12 %0
}

define weak i24 @_ssdm_op_Read.ap_auto.i24(i24) {
entry:
  ret i24 %0
}

!0 = metadata !{i32 12, i32 12, i32 0, i32 2}
!1 = metadata !{i32 0, i32 0, i32 0, i32 2}
!2 = metadata !{i32 24, i32 24, i32 0, i32 2}
