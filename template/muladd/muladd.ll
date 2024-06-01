; ModuleID = 'proj/sol/.autopilot/db/a.o.3.bc'
target datalayout = "e-m:e-i64:64-i128:128-i256:256-i512:512-i1024:1024-i2048:2048-i4096:4096-n8:16:32:64-S128-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024"
target triple = "fpga64-xilinx-none"

@empty_0 = internal unnamed_addr constant [1 x i8] zeroinitializer

define internal fastcc i32 @_simd_muladd_signed_4b(i4 %w0, i4 %w1, i4 %w2, i4 %w3, i5 %a, i32 %pcin) nounwind readnone noinline {
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
  %P = call i32 @_ssdm_op_BitConcatenate.i32.i8.i8.i8.i8(i8 %p0, i8 %p1, i8 %p2, i8 %p3), !bitwidth !4
  ret i32 %P, !bitwidth !1599
}

define internal fastcc { i32, i1, i1, i1 } @_simd_muladd_unsigned_4b(i4 %a0_val, i4 %a1_val, i4 %a2_val, i4 %a3_val, i5 %w_val, i32 %pcin) nounwind readnone {
entry:
  %lshr_ln = call i3 @_ssdm_op_PartSelect.i3.i4.i32.i32(i4 %a0_val, i32 1, i32 3), !bitwidth !1492
  %zext_ln26 = zext i3 %lshr_ln to i4, !bitwidth !1493
  %A = call i27 @_ssdm_op_BitConcatenate.i27.i4.i4.i4.i4.i4.i3.i4(i4 %a3_val, i4 0, i4 %a2_val, i4 0, i4 %a1_val, i3 0, i4 %zext_ln26), !bitwidth !1494
  %zext_ln29 = zext i27 %A to i31, !bitwidth !1529
  %sext_ln29 = sext i5 %w_val to i31, !bitwidth !1530
  %P = mul i31 %zext_ln29, %sext_ln29, !bitwidth !1531
  %p0_msbs = trunc i31 %P to i7, !bitwidth !6
  %shl_p0_msbs = call i8 @_ssdm_op_BitConcatenate.i8.i7.i1(i7 %p0_msbs, i1 false), !bitwidth !2
  %a0_lsb = trunc i4 %a0_val to i1, !bitwidth !1652
  %p0_correction = select i1 %a0_lsb, i5 %w_val, i5 0, !bitwidth !0
  %sext_p0_correction = sext i5 %p0_correction to i8, !bitwidth !1
  %p0 = add i8 %shl_p0_msbs, %sext_p0_correction, !bitwidth !2
  %p1 = call i8 @_ssdm_op_PartSelect.i8.i31.i32.i32(i31 %P, i32 7, i32 14), !bitwidth !2
  %p2 = call i8 @_ssdm_op_PartSelect.i8.i31.i32.i32(i31 %P, i32 15, i32 22), !bitwidth !2
  %p3 = call i8 @_ssdm_op_PartSelect.i8.i31.i32.i32(i31 %P, i32 23, i32 30), !bitwidth !2
  %p1_correction = call i1 @_ssdm_op_BitSelect.i1.i31.i32(i31 %P, i32 6), !bitwidth !1652
  %p2_correction = call i1 @_ssdm_op_BitSelect.i1.i31.i32(i31 %P, i32 14), !bitwidth !1652
  %p3_correction = call i1 @_ssdm_op_BitSelect.i1.i31.i32(i31 %P, i32 22), !bitwidth !1652
  %prods = call i32 @_ssdm_op_BitConcatenate.i32.i8.i8.i8.i8(i8 %p0, i8 %p1, i8 %p2, i8 %p3), !bitwidth !4
  %prods0 = insertvalue { i32, i1, i1, i1 } undef, i32 %prods, 0, !bitwidth !1599
  %prods1 = insertvalue { i32, i1, i1, i1 } %prods0, i1 %p1_correction, 1, !bitwidth !1599
  %prods2 = insertvalue { i32, i1, i1, i1 } %prods1, i1 %p2_correction, 2, !bitwidth !1599
  %prods3 = insertvalue { i32, i1, i1, i1 } %prods2, i1 %p3_correction, 3, !bitwidth !1599
  ret { i32, i1, i1, i1 } %prods3, !bitwidth !1599
}

define internal fastcc { i8, i8, i8, i8 } @_simd_muladd_signed_extract_4b({ i32, i1, i1, i1 } %prods) nounwind readnone {
  %M_val = extractvalue { i32, i1, i1, i1 } %prods, 0, !bitwidth !4
  %p0 = call i8 @_ssdm_op_PartSelect.i8.i32.i32.i32(i32 %M_val, i32 0, i32 7), !bitwidth !5
  %p1 = call i8 @_ssdm_op_PartSelect.i8.i32.i32.i32(i32 %M_val, i32 8, i32 15), !bitwidth !5
  %p2 = call i8 @_ssdm_op_PartSelect.i8.i32.i32.i32(i32 %M_val, i32 16, i32 23), !bitwidth !5
  %p3 = call i8 @_ssdm_op_PartSelect.i8.i32.i32.i32(i32 %M_val, i32 24, i32 31), !bitwidth !5
  %p1_correction = extractvalue { i32, i1, i1, i1 } %prods, 1, !bitwidth !1652
  %zext_p1_correction = zext i1 %p1_correction to i8, !bitwidth !1566
  %p1_corrected = add i8 %p1, %zext_p1_correction, !bitwidth !2
  %p2_correction = extractvalue { i32, i1, i1, i1 } %prods, 2, !bitwidth !1652
  %zext_p2_correction = zext i1 %p2_correction to i8, !bitwidth !1566
  %p2_corrected = add i8 %p2, %zext_p2_correction, !bitwidth !2
  %p3_correction = extractvalue { i32, i1, i1, i1 } %prods, 3, !bitwidth !1652
  %zext_p3_correction = zext i1 %p3_correction to i8, !bitwidth !1566
  %p3_corrected = add i8 %p3, %zext_p3_correction, !bitwidth !2
  %P0 = insertvalue { i8, i8, i8, i8 } undef, i8 %p3_corrected, 0, !bitwidth !1599
  %P1 = insertvalue { i8, i8, i8, i8 } %P0, i8 %p2_corrected, 1, !bitwidth !1599
  %P2 = insertvalue { i8, i8, i8, i8 } %P1, i8 %p1_corrected, 2, !bitwidth !1599
  %P3 = insertvalue { i8, i8, i8, i8 } %P2, i8 %p0, 3, !bitwidth !1599
  ret { i8, i8, i8, i8 } %P3, !bitwidth !1599
}

define internal fastcc { i8, i8, i8, i8 } @_simd_muladd_unsigned_extract_4b({ i32, i1, i1, i1 } %prods) nounwind readnone {
  %M_val = extractvalue { i32, i1, i1, i1 } %prods, 0, !bitwidth !4
  %p3 = call i8 @_ssdm_op_PartSelect.i8.i32.i32.i32(i32 %M_val, i32 0, i32 7), !bitwidth !5
  %p2 = call i8 @_ssdm_op_PartSelect.i8.i32.i32.i32(i32 %M_val, i32 8, i32 15), !bitwidth !5
  %p1 = call i8 @_ssdm_op_PartSelect.i8.i32.i32.i32(i32 %M_val, i32 16, i32 23), !bitwidth !5
  %p0 = call i8 @_ssdm_op_PartSelect.i8.i32.i32.i32(i32 %M_val, i32 24, i32 31), !bitwidth !5
  %P0 = insertvalue { i8, i8, i8, i8 } undef, i8 %p0, 0, !bitwidth !1599
  %P1 = insertvalue { i8, i8, i8, i8 } %P0, i8 %p1, 1, !bitwidth !1599
  %P2 = insertvalue { i8, i8, i8, i8 } %P1, i8 %p2, 2, !bitwidth !1599
  %P3 = insertvalue { i8, i8, i8, i8 } %P2, i8 %p3, 3, !bitwidth !1599
  ret { i8, i8, i8, i8 } %P3, !bitwidth !1599
}

define internal fastcc i36 @_simd_muladd_8b(i9 %a_val, i9 %d_val, i9 %b_val, i36 %PCIN_val) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty_0)
  call void (...)* @_ssdm_op_SpecLatency(i64 0, i64 0, [1 x i8]* @empty_0)
  %PCIN_val_read = call i36 @_ssdm_op_Read.ap_auto.i36(i36 %PCIN_val) nounwind, !bitwidth !1552
  %b_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %b_val) nounwind, !bitwidth !1548
  %d_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %d_val) nounwind, !bitwidth !1548
  %a_val_read = call i9 @_ssdm_op_Read.ap_auto.i9(i9 %a_val) nounwind, !bitwidth !1548
  %A = call i27 @_ssdm_op_BitConcatenate.i27.i9.i18(i9 %a_val_read, i18 0), !bitwidth !1644
  %sext_ln25_1 = sext i9 %d_val_read to i27, !bitwidth !1644
  %add_ln25 = add nsw i27 %A, %sext_ln25_1, !bitwidth !1644
  %sext_ln25_2 = sext i27 %add_ln25 to i36, !bitwidth !1647
  %sext_ln25_3 = sext i9 %b_val_read to i36, !bitwidth !1647
  %mul_ln25 = mul i36 %sext_ln25_2, %sext_ln25_3, !bitwidth !1647
  %add_ln25_1 = add i36 %mul_ln25, %PCIN_val_read, !bitwidth !1552
  ret i36 %add_ln25_1, !bitwidth !1599
}

define internal fastcc i36 @_simd_muladd_inline_8b(i9 %a_val, i9 %d_val, i9 %b_val, i36 %PCIN_val) nounwind readnone {
entry:
  %A = call i27 @_ssdm_op_BitConcatenate.i27.i9.i18(i9 %a_val, i18 0), !bitwidth !1644
  %sext_ln25_1 = sext i9 %d_val to i27, !bitwidth !1644
  %add_ln25 = add nsw i27 %A, %sext_ln25_1, !bitwidth !1644
  %sext_ln25_2 = sext i27 %add_ln25 to i36, !bitwidth !1647
  %sext_ln25_3 = sext i9 %b_val to i36, !bitwidth !1647
  %mul_ln25 = mul i36 %sext_ln25_2, %sext_ln25_3, !bitwidth !1647
  %add_ln25_1 = add i36 %mul_ln25, %PCIN_val, !bitwidth !1552
  ret i36 %add_ln25_1, !bitwidth !1599
}

define internal fastcc { i18, i18 } @_simd_muladd_signed_extract_8b(i36 %M_val) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty_0)
  call void (...)* @_ssdm_op_SpecLatency(i64 0, i64 0, [1 x i8]* @empty_0)
  %M_val_read = call i36 @_ssdm_op_Read.ap_auto.i36(i36 %M_val) nounwind, !bitwidth !1561
  %trunc_ln13 = trunc i36 %M_val_read to i18, !bitwidth !1600
  %tmp = call i1 @_ssdm_op_BitSelect.i1.i36.i32(i36 %M_val_read, i32 17), !bitwidth !1652
  %zext_ln14 = zext i1 %tmp to i18, !bitwidth !1653
  %trunc_ln = call i18 @_ssdm_op_PartSelect.i18.i36.i32.i32(i36 %M_val_read, i32 18, i32 35), !bitwidth !1600
  %add_ln14 = add i18 %trunc_ln, %zext_ln14, !bitwidth !1600
  %mrv = insertvalue { i18, i18 } undef, i18 %add_ln14, 0, !bitwidth !1599
  %mrv_1 = insertvalue { i18, i18 } %mrv, i18 %trunc_ln13, 1, !bitwidth !1599
  ret { i18, i18 } %mrv_1, !bitwidth !1599
}

define internal fastcc { i18, i18 } @_simd_muladd_signed_extract_inline_8b(i36 %M_val) nounwind readnone {
entry:
  %trunc_ln13 = trunc i36 %M_val to i18, !bitwidth !1600
  %tmp = call i1 @_ssdm_op_BitSelect.i1.i36.i32(i36 %M_val, i32 17), !bitwidth !1652
  %zext_ln14 = zext i1 %tmp to i18, !bitwidth !1653
  %trunc_ln = call i18 @_ssdm_op_PartSelect.i18.i36.i32.i32(i36 %M_val, i32 18, i32 35), !bitwidth !1600
  %add_ln14 = add i18 %trunc_ln, %zext_ln14, !bitwidth !1600
  %mrv = insertvalue { i18, i18 } undef, i18 %add_ln14, 0, !bitwidth !1599
  %mrv_1 = insertvalue { i18, i18 } %mrv, i18 %trunc_ln13, 1, !bitwidth !1599
  ret { i18, i18 } %mrv_1, !bitwidth !1599
}

define internal fastcc { i18, i18 } @_simd_muladd_unsigned_extract_8b(i36 %M_val) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty_0)
  call void (...)* @_ssdm_op_SpecLatency(i64 0, i64 0, [1 x i8]* @empty_0)
  %M_val_read = call i36 @_ssdm_op_Read.ap_auto.i36(i36 %M_val) nounwind, !bitwidth !1561
  %trunc_ln13 = trunc i36 %M_val_read to i18, !bitwidth !1600
  %trunc_ln = call i18 @_ssdm_op_PartSelect.i18.i36.i32.i32(i36 %M_val_read, i32 18, i32 35), !bitwidth !1600
  %mrv = insertvalue { i18, i18 } undef, i18 %trunc_ln, 0, !bitwidth !1599
  %mrv_1 = insertvalue { i18, i18 } %mrv, i18 %trunc_ln13, 1, !bitwidth !1599
  ret { i18, i18 } %mrv_1, !bitwidth !1599
}

define internal fastcc { i18, i18 } @_simd_muladd_unsigned_extract_inline_8b(i36 %M_val) nounwind readnone {
entry:
  %trunc_ln13 = trunc i36 %M_val to i18, !bitwidth !1600
  %trunc_ln = call i18 @_ssdm_op_PartSelect.i18.i36.i32.i32(i36 %M_val, i32 18, i32 35), !bitwidth !1600
  %mrv = insertvalue { i18, i18 } undef, i18 %trunc_ln, 0, !bitwidth !1599
  %mrv_1 = insertvalue { i18, i18 } %mrv, i18 %trunc_ln13, 1, !bitwidth !1599
  ret { i18, i18 } %mrv_1, !bitwidth !1599
}

define weak void @_ssdm_op_SpecPipeline(...) nounwind {
entry:
  ret void
}

define weak void @_ssdm_op_SpecLatency(...) nounwind {
entry:
  ret void
}

define weak i36 @_ssdm_op_Read.ap_auto.i36(i36) {
entry:
  ret i36 %0
}

define weak i32 @_ssdm_op_Read.ap_auto.i32(i32) {
entry:
  ret i32 %0
}

define weak i9 @_ssdm_op_Read.ap_auto.i9(i9) {
entry:
  ret i9 %0
}

define weak i5 @_ssdm_op_Read.ap_auto.i5(i5) {
entry:
  ret i5 %0
}

define weak i4 @_ssdm_op_Read.ap_auto.i4(i4) {
entry:
  ret i4 %0
}

define weak i1 @_ssdm_op_BitSelect.i1.i31.i32(i31, i32) nounwind readnone {
entry:
  %empty = trunc i32 %1 to i31
  %empty_11 = shl i31 1, %empty
  %empty_12 = and i31 %0, %empty_11
  %empty_13 = icmp ne i31 %empty_12, 0
  ret i1 %empty_13
}

define weak i1 @_ssdm_op_BitSelect.i1.i36.i32(i36, i32) nounwind readnone {
entry:
  %empty = zext i32 %1 to i36
  %empty_7 = shl i36 1, %empty
  %empty_8 = and i36 %0, %empty_7
  %empty_9 = icmp ne i36 %empty_8, 0
  ret i1 %empty_9
}

define weak i32 @_ssdm_op_BitConcatenate.i32.i8.i8.i8.i8(i8, i8, i8, i8) nounwind readnone {
entry:
  %zext_0 = zext i8 %0 to i32
  %zext_1 = zext i8 %1 to i32
  %zext_2 = zext i8 %2 to i32
  %zext_3 = zext i8 %3 to i32
  %shl_0 = shl i32 %zext_0, 24
  %shl_1 = shl i32 %zext_1, 16
  %shl_2 = shl i32 %zext_2, 8
  %res_0_1 = or i32 %shl_0, %shl_1
  %res_0_1_2 = or i32 %res_0_1, %shl_2
  %res = or i32 %res_0_1_2, %zext_3
  ret i32 %res
}

define weak i8 @_ssdm_op_BitConcatenate.i8.i7.i1(i7, i1) nounwind readnone {
entry:
  %empty = zext i7 %0 to i8
  %empty_14 = zext i1 %1 to i8
  %empty_15 = shl i8 %empty, 1
  %empty_16 = or i8 %empty_15, %empty_14
  ret i8 %empty_16
}

define weak i27 @_ssdm_op_BitConcatenate.i27.i4.i4.i4.i4.i4.i3.i4(i4, i4, i4, i4, i4, i3, i4) nounwind readnone {
entry:
  %empty = zext i3 %5 to i7
  %empty_17 = zext i4 %6 to i7
  %empty_18 = shl i7 %empty, 4
  %empty_19 = or i7 %empty_18, %empty_17
  %empty_20 = zext i4 %4 to i11
  %empty_21 = zext i7 %empty_19 to i11
  %empty_22 = shl i11 %empty_20, 7
  %empty_23 = or i11 %empty_22, %empty_21
  %empty_24 = zext i4 %3 to i15
  %empty_25 = zext i11 %empty_23 to i15
  %empty_26 = shl i15 %empty_24, 11
  %empty_27 = or i15 %empty_26, %empty_25
  %empty_28 = zext i4 %2 to i19
  %empty_29 = zext i15 %empty_27 to i19
  %empty_30 = shl i19 %empty_28, 15
  %empty_31 = or i19 %empty_30, %empty_29
  %empty_32 = zext i4 %1 to i23
  %empty_33 = zext i19 %empty_31 to i23
  %empty_34 = shl i23 %empty_32, 19
  %empty_35 = or i23 %empty_34, %empty_33
  %empty_36 = zext i4 %0 to i27
  %empty_37 = zext i23 %empty_35 to i27
  %empty_38 = shl i27 %empty_36, 23
  %empty_39 = or i27 %empty_38, %empty_37
  ret i27 %empty_39
}

define weak i27 @_ssdm_op_BitConcatenate.i27.i9.i18(i9, i18) nounwind readnone {
entry:
  %empty = zext i9 %0 to i27
  %empty_10 = zext i18 %1 to i27
  %empty_11 = shl i27 %empty, 18
  %empty_12 = or i27 %empty_11, %empty_10
  ret i27 %empty_12
}

define weak i3 @_ssdm_op_PartSelect.i3.i4.i32.i32(i4, i32, i32) nounwind readnone {
entry:
  %empty = call i4 @llvm.part.select.i4(i4 %0, i32 %1, i32 %2)
  %empty_10 = trunc i4 %empty to i3
  ret i3 %empty_10
}

define weak i8 @_ssdm_op_PartSelect.i8.i31.i32.i32(i31, i32, i32) nounwind readnone {
entry:
  %empty = call i31 @llvm.part.select.i31(i31 %0, i32 %1, i32 %2)
  %empty_9 = trunc i31 %empty to i8
  ret i8 %empty_9
}

define weak i8 @_ssdm_op_PartSelect.i8.i32.i32.i32(i32, i32, i32) nounwind readnone {
entry:
  %empty = call i32 @llvm.part.select.i32(i32 %0, i32 %1, i32 %2)
  %empty_6 = trunc i32 %empty to i8
  ret i8 %empty_6
}

define weak i18 @_ssdm_op_PartSelect.i18.i36.i32.i32(i36, i32, i32) nounwind readnone {
entry:
  %empty = call i36 @llvm.part.select.i36(i36 %0, i32 %1, i32 %2)
  %empty_6 = trunc i36 %empty to i18
  ret i18 %empty_6
}

declare i4 @llvm.part.select.i4(i4, i32, i32) nounwind readnone
declare i31 @llvm.part.select.i31(i31, i32, i32) nounwind readnone
declare i32 @llvm.part.select.i32(i32, i32, i32) nounwind readnone
declare i36 @llvm.part.select.i36(i36, i32, i32) nounwind readnone

!0 = metadata !{i32 4, i32 4, i32 0, i32 2}
!1 = metadata !{i32 8, i32 8, i32 0, i32 1}
!2 = metadata !{i32 8, i32 8, i32 0, i32 2}
!3 = metadata !{i32 5, i32 5, i32 0, i32 2}
!4 = metadata !{i32 32, i32 32, i32 0, i32 1}
!5 = metadata !{i32 32, i32 32, i32 0, i32 2}
!1492 = metadata !{i32 3, i32 3, i32 0, i32 2}
!1493 = metadata !{i32 4, i32 4, i32 0, i32 0}
!1494 = metadata !{i32 27, i32 27, i32 0, i32 2}
!1529 = metadata !{i32 31, i32 31, i32 0, i32 0}
!1530 = metadata !{i32 31, i32 31, i32 0, i32 1}
!1531 = metadata !{i32 31, i32 31, i32 0, i32 2}
!6 = metadata !{i32 7, i32 7, i32 0, i32 2}
!1566 = metadata !{i32 8, i32 8, i32 0, i32 0}
!1548 = metadata !{i32 9, i32 9, i32 0, i32 2}
!1552 = metadata !{i32 36, i32 36, i32 0, i32 2}
!1561 = metadata !{i32 36, i32 36, i32 0, i32 2}
!1599 = metadata !{i32 0, i32 0, i32 0, i32 2}
!1600 = metadata !{i32 18, i32 18, i32 0, i32 2}
!1644 = metadata !{i32 27, i32 27, i32 0, i32 1}
!1647 = metadata !{i32 36, i32 36, i32 0, i32 2}
!1652 = metadata !{i32 1, i32 1, i32 0, i32 2}
!1653 = metadata !{i32 18, i32 18, i32 0, i32 0}
