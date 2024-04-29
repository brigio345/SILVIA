; ModuleID = 'proj/sol/.autopilot/db/a.o.3.bc'
target datalayout = "e-m:e-i64:64-i128:128-i256:256-i512:512-i1024:1024-i2048:2048-i4096:4096-n8:16:32:64-S128-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024"
target triple = "fpga64-xilinx-none"

@empty_0 = internal unnamed_addr constant [1 x i8] zeroinitializer

define internal fastcc { i16, i16 } @_simd_mul_signed_2(i9 %a_val, i9 %d_val, i9 %b_val) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty_0)
  call void (...)* @_ssdm_op_SpecLatency(i64 0, i64 0, [1 x i8]* @empty_0)
  %b_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %b_val) nounwind, !bitwidth !1548
  %d_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %d_val) nounwind, !bitwidth !1548
  %a_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %a_val) nounwind, !bitwidth !1548
  %A = call i27 @_ssdm_op_BitConcatenate.i27.i9.i18(i9 %a_val_read, i18 0), !bitwidth !1644
  %sext_ln25_1 = sext i9 %d_val_read to i27, !bitwidth !1644
  %add_ln25 = add nsw i27 %A, %sext_ln25_1, !bitwidth !1644
  %sext_ln25_2 = sext i27 %add_ln25 to i34, !bitwidth !1647
  %sext_ln25_3 = sext i9 %b_val_read to i34, !bitwidth !1647
  %mul_ln25 = mul i34 %sext_ln25_2, %sext_ln25_3, !bitwidth !1647
  %trunc_ln13 = trunc i34 %mul_ln25 to i16, !bitwidth !1600
  %tmp = call i1 @_ssdm_op_BitSelect.i1.i34.i32(i34 %mul_ln25, i32 17), !bitwidth !1652
  %zext_ln14 = zext i1 %tmp to i16, !bitwidth !1653
  %trunc_ln = call i16 @_ssdm_op_PartSelect.i16.i34.i32.i32(i34 %mul_ln25, i32 18, i32 33), !bitwidth !1600
  %add_ln14 = add i16 %trunc_ln, %zext_ln14, !bitwidth !1600
  %mrv = insertvalue { i16, i16 } undef, i16 %add_ln14, 0, !bitwidth !1599
  %mrv_1 = insertvalue { i16, i16 } %mrv, i16 %trunc_ln13, 1, !bitwidth !1599
  ret { i16, i16 } %mrv_1, !bitwidth !1599
}

define internal fastcc { i16, i16 } @_simd_mul_signed_inline_2(i9 %a_val, i9 %d_val, i9 %b_val) nounwind readnone {
entry:
  %A = call i27 @_ssdm_op_BitConcatenate.i27.i9.i18(i9 %a_val, i18 0), !bitwidth !1644
  %sext_ln25_1 = sext i9 %d_val to i27, !bitwidth !1644
  %add_ln25 = add nsw i27 %A, %sext_ln25_1, !bitwidth !1644
  %sext_ln25_2 = sext i27 %add_ln25 to i34, !bitwidth !1647
  %sext_ln25_3 = sext i9 %b_val to i34, !bitwidth !1647
  %mul_ln25 = mul i34 %sext_ln25_2, %sext_ln25_3, !bitwidth !1647
  %trunc_ln13 = trunc i34 %mul_ln25 to i16, !bitwidth !1600
  %tmp = call i1 @_ssdm_op_BitSelect.i1.i34.i32(i34 %mul_ln25, i32 17), !bitwidth !1652
  %zext_ln14 = zext i1 %tmp to i16, !bitwidth !1653
  %trunc_ln = call i16 @_ssdm_op_PartSelect.i16.i34.i32.i32(i34 %mul_ln25, i32 18, i32 33), !bitwidth !1600
  %add_ln14 = add i16 %trunc_ln, %zext_ln14, !bitwidth !1600
  %mrv = insertvalue { i16, i16 } undef, i16 %add_ln14, 0, !bitwidth !1599
  %mrv_1 = insertvalue { i16, i16 } %mrv, i16 %trunc_ln13, 1, !bitwidth !1599
  ret { i16, i16 } %mrv_1, !bitwidth !1599
}

define internal fastcc { i16, i16 } @_simd_mul_unsigned_2(i9 %a_val, i9 %d_val, i9 %b_val) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty_0)
  call void (...)* @_ssdm_op_SpecLatency(i64 0, i64 0, [1 x i8]* @empty_0)
  %b_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %b_val) nounwind, !bitwidth !1548
  %d_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %d_val) nounwind, !bitwidth !1548
  %a_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %a_val) nounwind, !bitwidth !1548
  %A = call i27 @_ssdm_op_BitConcatenate.i27.i9.i18(i9 %a_val_read, i18 0), !bitwidth !1644
  %sext_ln25_1 = sext i9 %d_val_read to i27, !bitwidth !1644
  %add_ln25 = add nsw i27 %A, %sext_ln25_1, !bitwidth !1644
  %sext_ln25_2 = sext i27 %add_ln25 to i34, !bitwidth !1647
  %sext_ln25_3 = sext i9 %b_val_read to i34, !bitwidth !1647
  %mul_ln25 = mul i34 %sext_ln25_2, %sext_ln25_3, !bitwidth !1647
  %trunc_ln13 = trunc i34 %mul_ln25 to i16, !bitwidth !1600
  %trunc_ln = call i16 @_ssdm_op_PartSelect.i16.i34.i32.i32(i34 %mul_ln25, i32 18, i32 33), !bitwidth !1600
  %mrv = insertvalue { i16, i16 } undef, i16 %trunc_ln, 0, !bitwidth !1599
  %mrv_1 = insertvalue { i16, i16 } %mrv, i16 %trunc_ln13, 1, !bitwidth !1599
  ret { i16, i16 } %mrv_1, !bitwidth !1599
}

define internal fastcc { i16, i16 } @_simd_mul_unsigned_inline_2(i9 %a_val, i9 %d_val, i9 %b_val) nounwind readnone {
entry:
  %A = call i27 @_ssdm_op_BitConcatenate.i27.i9.i18(i9 %a_val, i18 0), !bitwidth !1644
  %sext_ln25_1 = sext i9 %d_val to i27, !bitwidth !1644
  %add_ln25 = add nsw i27 %A, %sext_ln25_1, !bitwidth !1644
  %sext_ln25_2 = sext i27 %add_ln25 to i34, !bitwidth !1647
  %sext_ln25_3 = sext i9 %b_val to i34, !bitwidth !1647
  %mul_ln25 = mul i34 %sext_ln25_2, %sext_ln25_3, !bitwidth !1647
  %trunc_ln13 = trunc i34 %mul_ln25 to i16, !bitwidth !1600
  %trunc_ln = call i16 @_ssdm_op_PartSelect.i16.i34.i32.i32(i34 %mul_ln25, i32 18, i32 33), !bitwidth !1600
  %mrv = insertvalue { i16, i16 } undef, i16 %trunc_ln, 0, !bitwidth !1599
  %mrv_1 = insertvalue { i16, i16 } %mrv, i16 %trunc_ln13, 1, !bitwidth !1599
  ret { i16, i16 } %mrv_1, !bitwidth !1599
}

define weak void @_ssdm_op_SpecPipeline(...) nounwind {
entry:
  ret void
}

define weak void @_ssdm_op_SpecLatency(...) nounwind {
entry:
  ret void
}

define weak i34 @_ssdm_op_Read.ap_auto.i34(i34) {
entry:
  ret i34 %0
}

define weak i9 @_ssdm_op_Read.ap_auto.i9(i9) {
entry:
  ret i9 %0
}

define weak i1 @_ssdm_op_BitSelect.i1.i34.i32(i34, i32) nounwind readnone {
entry:
  %empty = zext i32 %1 to i34
  %empty_7 = shl i34 1, %empty
  %empty_8 = and i34 %0, %empty_7
  %empty_9 = icmp ne i34 %empty_8, 0
  ret i1 %empty_9
}

define weak i27 @_ssdm_op_BitConcatenate.i27.i9.i18(i9, i18) nounwind readnone {
entry:
  %empty = zext i9 %0 to i27
  %empty_10 = zext i18 %1 to i27
  %empty_11 = shl i27 %empty, 18
  %empty_12 = or i27 %empty_11, %empty_10
  ret i27 %empty_12
}

define weak i16 @_ssdm_op_PartSelect.i16.i34.i32.i32(i34, i32, i32) nounwind readnone {
entry:
  %empty = call i34 @llvm.part.select.i34(i34 %0, i32 %1, i32 %2)
  %empty_6 = trunc i34 %empty to i16
  ret i16 %empty_6
}

declare i34 @llvm.part.select.i34(i34, i32, i32) nounwind readnone

!1548 = metadata !{i32 9, i32 9, i32 0, i32 2}
!1552 = metadata !{i32 34, i32 34, i32 0, i32 2}
!1599 = metadata !{i32 0, i32 0, i32 0, i32 2}
!1600 = metadata !{i32 16, i32 16, i32 0, i32 2}
!1644 = metadata !{i32 27, i32 27, i32 0, i32 1}
!1647 = metadata !{i32 34, i32 34, i32 0, i32 2}
!1652 = metadata !{i32 1, i32 1, i32 0, i32 2}
!1653 = metadata !{i32 16, i32 16, i32 0, i32 0}
