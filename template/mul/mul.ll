; ModuleID = 'proj/sol/.autopilot/db/a.o.3.bc'
target datalayout = "e-m:e-i64:64-i128:128-i256:256-i512:512-i1024:1024-i2048:2048-i4096:4096-n8:16:32:64-S128-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024"
target triple = "fpga64-xilinx-none"

@empty_0 = internal unnamed_addr constant [1 x i8] zeroinitializer

define internal fastcc { i8, i8, i8, i8 } @_simd_mul_4(i4 %w0, i4 %w1, i4 %w2, i4 %w3, i5 %a) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty_0)
  call void (...)* @_ssdm_op_SpecLatency(i64 5, i64 5, [1 x i8]* @empty_0)
  %w0_read = call i4 @_ssdm_op_Read.ap_auto.i4(i4 %w0) nounwind, !bitwidth !0
  %w1_read = call i4 @_ssdm_op_Read.ap_auto.i4(i4 %w1) nounwind, !bitwidth !0
  %w2_read = call i4 @_ssdm_op_Read.ap_auto.i4(i4 %w2) nounwind, !bitwidth !0
  %w3_read = call i4 @_ssdm_op_Read.ap_auto.i4(i4 %w3) nounwind, !bitwidth !0
  %a_read = call i5 @_ssdm_op_Read.ap_auto.i5(i5 %a) nounwind, !bitwidth !3
  %w0_sext = sext i4 %w0_read to i8, !bitwidth !1
  %w1_sext = sext i4 %w1_read to i8, !bitwidth !1
  %w2_sext = sext i4 %w2_read to i8, !bitwidth !1
  %w3_sext = sext i4 %w3_read to i8, !bitwidth !1
  %a_sext = sext i5 %a_read to i8, !bitwidth !1
  %p0 = mul i8 %w0_sext, %a_sext, !bitwidth !2
  %p1 = mul i8 %w1_sext, %a_sext, !bitwidth !2
  %p2 = mul i8 %w2_sext, %a_sext, !bitwidth !2
  %p3 = mul i8 %w3_sext, %a_sext, !bitwidth !2
  %P0 = insertvalue { i8, i8, i8, i8 } undef, i8 %p0, 0, !bitwidth !1599
  %P1 = insertvalue { i8, i8, i8, i8 } %P0, i8 %p1, 1, !bitwidth !1599
  %P2 = insertvalue { i8, i8, i8, i8 } %P1, i8 %p2, 2, !bitwidth !1599
  %P3 = insertvalue { i8, i8, i8, i8 } %P2, i8 %p3, 3, !bitwidth !1599
  ret { i8, i8, i8, i8 } %P3, !bitwidth !1599
}

define internal fastcc { i16, i16 } @_simd_mul_signed_2(i9 %a_val, i9 %d_val, i9 %b_val) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty_0)
  call void (...)* @_ssdm_op_SpecLatency(i64 0, i64 0, [1 x i8]* @empty_0)
  %b_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %b_val) nounwind, !bitwidth !1548
  %d_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %d_val) nounwind, !bitwidth !1548
  %a_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %a_val) nounwind, !bitwidth !1548
  %A = call i25 @_ssdm_op_BitConcatenate.i25.i9.i16(i9 %a_val_read, i16 0), !bitwidth !1644
  %sext_ln25_1 = sext i9 %d_val_read to i25, !bitwidth !1644
  %add_ln25 = add nsw i25 %A, %sext_ln25_1, !bitwidth !1644
  %sext_ln25_2 = sext i25 %add_ln25 to i32, !bitwidth !1552
  %sext_ln25_3 = sext i9 %b_val_read to i32, !bitwidth !1552
  %mul_ln25 = mul i32 %sext_ln25_2, %sext_ln25_3, !bitwidth !1552
  %trunc_ln13 = trunc i32 %mul_ln25 to i16, !bitwidth !1600
  %tmp = call i1 @_ssdm_op_BitSelect.i1.i32.i32(i32 %mul_ln25, i32 15), !bitwidth !1652
  %zext_ln14 = zext i1 %tmp to i16, !bitwidth !1653
  %trunc_ln = call i16 @_ssdm_op_PartSelect.i16.i32.i32.i32(i32 %mul_ln25, i32 16, i32 31), !bitwidth !1600
  %add_ln14 = add i16 %trunc_ln, %zext_ln14, !bitwidth !1600
  %mrv = insertvalue { i16, i16 } undef, i16 %add_ln14, 0, !bitwidth !1599
  %mrv_1 = insertvalue { i16, i16 } %mrv, i16 %trunc_ln13, 1, !bitwidth !1599
  ret { i16, i16 } %mrv_1, !bitwidth !1599
}

define internal fastcc { i16, i16 } @_simd_mul_signed_inline_2(i9 %a_val, i9 %d_val, i9 %b_val) nounwind readnone {
entry:
  %A = call i25 @_ssdm_op_BitConcatenate.i25.i9.i16(i9 %a_val, i16 0), !bitwidth !1644
  %sext_ln25_1 = sext i9 %d_val to i25, !bitwidth !1644
  %add_ln25 = add nsw i25 %A, %sext_ln25_1, !bitwidth !1644
  %sext_ln25_2 = sext i25 %add_ln25 to i32, !bitwidth !1552
  %sext_ln25_3 = sext i9 %b_val to i32, !bitwidth !1552
  %mul_ln25 = mul i32 %sext_ln25_2, %sext_ln25_3, !bitwidth !1552
  %trunc_ln13 = trunc i32 %mul_ln25 to i16, !bitwidth !1600
  %tmp = call i1 @_ssdm_op_BitSelect.i1.i32.i32(i32 %mul_ln25, i32 17), !bitwidth !1652
  %zext_ln14 = zext i1 %tmp to i16, !bitwidth !1653
  %trunc_ln = call i16 @_ssdm_op_PartSelect.i16.i32.i32.i32(i32 %mul_ln25, i32 16, i32 31), !bitwidth !1600
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
  %A = call i25 @_ssdm_op_BitConcatenate.i25.i9.i16(i9 %a_val_read, i16 0), !bitwidth !1644
  %sext_ln25_1 = sext i9 %d_val_read to i25, !bitwidth !1644
  %add_ln25 = add nsw i25 %A, %sext_ln25_1, !bitwidth !1644
  %sext_ln25_2 = sext i25 %add_ln25 to i32, !bitwidth !1552
  %sext_ln25_3 = sext i9 %b_val_read to i32, !bitwidth !1552
  %mul_ln25 = mul i32 %sext_ln25_2, %sext_ln25_3, !bitwidth !1552
  %trunc_ln13 = trunc i32 %mul_ln25 to i16, !bitwidth !1600
  %trunc_ln = call i16 @_ssdm_op_PartSelect.i16.i32.i32.i32(i32 %mul_ln25, i32 16, i32 31), !bitwidth !1600
  %mrv = insertvalue { i16, i16 } undef, i16 %trunc_ln, 0, !bitwidth !1599
  %mrv_1 = insertvalue { i16, i16 } %mrv, i16 %trunc_ln13, 1, !bitwidth !1599
  ret { i16, i16 } %mrv_1, !bitwidth !1599
}

define internal fastcc { i16, i16 } @_simd_mul_unsigned_inline_2(i9 %a_val, i9 %d_val, i9 %b_val) nounwind readnone {
entry:
  %A = call i25 @_ssdm_op_BitConcatenate.i25.i9.i16(i9 %a_val, i16 0), !bitwidth !1644
  %sext_ln25_1 = sext i9 %d_val to i25, !bitwidth !1644
  %add_ln25 = add nsw i25 %A, %sext_ln25_1, !bitwidth !1644
  %sext_ln25_2 = sext i25 %add_ln25 to i32, !bitwidth !1552
  %sext_ln25_3 = sext i9 %b_val to i32, !bitwidth !1552
  %mul_ln25 = mul i32 %sext_ln25_2, %sext_ln25_3, !bitwidth !1552
  %trunc_ln13 = trunc i32 %mul_ln25 to i16, !bitwidth !1600
  %trunc_ln = call i16 @_ssdm_op_PartSelect.i16.i32.i32.i32(i32 %mul_ln25, i32 16, i32 31), !bitwidth !1600
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

define weak i4 @_ssdm_op_Read.ap_auto.i4(i4) {
entry:
  ret i4 %0
}

define weak i5 @_ssdm_op_Read.ap_auto.i5(i5) {
entry:
  ret i5 %0
}

define weak i9 @_ssdm_op_Read.ap_auto.i9(i9) {
entry:
  ret i9 %0
}

define weak i1 @_ssdm_op_BitSelect.i1.i32.i32(i32, i32) nounwind readnone {
entry:
  %empty_7 = shl i32 1, %1
  %empty_8 = and i32 %0, %empty_7
  %empty_9 = icmp ne i32 %empty_8, 0
  ret i1 %empty_9
}

define weak i25 @_ssdm_op_BitConcatenate.i25.i9.i16(i9, i16) nounwind readnone {
entry:
  %empty = zext i9 %0 to i25
  %empty_10 = zext i16 %1 to i25
  %empty_11 = shl i25 %empty, 16
  %empty_12 = or i25 %empty_11, %empty_10
  ret i25 %empty_12
}

define weak i16 @_ssdm_op_PartSelect.i16.i32.i32.i32(i32, i32, i32) nounwind readnone {
entry:
  %empty = call i32 @llvm.part.select.i32(i32 %0, i32 %1, i32 %2)
  %empty_6 = trunc i32 %empty to i16
  ret i16 %empty_6
}

declare i32 @llvm.part.select.i32(i32, i32, i32) nounwind readnone

!0 = metadata !{i32 4, i32 4, i32 0, i32 2}
!1 = metadata !{i32 8, i32 8, i32 0, i32 1}
!2 = metadata !{i32 8, i32 8, i32 0, i32 2}
!3 = metadata !{i32 5, i32 5, i32 0, i32 2}
!1548 = metadata !{i32 9, i32 9, i32 0, i32 2}
!1552 = metadata !{i32 32, i32 32, i32 0, i32 2}
!1599 = metadata !{i32 0, i32 0, i32 0, i32 2}
!1600 = metadata !{i32 16, i32 16, i32 0, i32 2}
!1644 = metadata !{i32 25, i32 25, i32 0, i32 1}
!1652 = metadata !{i32 1, i32 1, i32 0, i32 2}
!1653 = metadata !{i32 16, i32 16, i32 0, i32 0}
