; ModuleID = 'proj/sol/.autopilot/db/a.o.3.bc'
target datalayout = "e-m:e-i64:64-i128:128-i256:256-i512:512-i1024:1024-i2048:2048-i4096:4096-n8:16:32:64-S128-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024"
target triple = "fpga64-xilinx-none"

@empty_0 = internal unnamed_addr constant [1 x i8] zeroinitializer

define internal fastcc i36 @_simd_muladd_2(i8 %a_val, i8 %d_val, i8 %b_val, i36 %PCIN_val) nounwind readnone noinline {
entry:
  call void (...)* @_ssdm_op_SpecPipeline(i32 1, i32 0, i32 0, i32 0, [1 x i8]* @empty_0)
  call void (...)* @_ssdm_op_SpecLatency(i64 2, i64 2, [1 x i8]* @empty_0)
  %PCIN_val_read = call i36 @_ssdm_op_Read.ap_auto.i36(i36 %PCIN_val) nounwind, !bitwidth !1552
  %b_val_read = call i8 @_ssdm_op_Read.ap_auto.i8(i8 %b_val) nounwind, !bitwidth !1548
  %d_val_read = call i8 @_ssdm_op_Read.ap_auto.i8(i8 %d_val) nounwind, !bitwidth !1548
  %a_val_read = call i8 @_ssdm_op_Read.ap_auto.i8(i8 %a_val) nounwind, !bitwidth !1548
  %A = call i26 @_ssdm_op_BitConcatenate.i26.i8.i18(i8 %a_val_read, i18 0), !bitwidth !1639
  %sext_ln25 = sext i26 %A to i27, !bitwidth !1644
  %sext_ln25_1 = sext i8 %d_val_read to i27, !bitwidth !1644
  %add_ln25 = add nsw i27 %sext_ln25, %sext_ln25_1, !bitwidth !1644
  %sext_ln25_2 = sext i27 %add_ln25 to i35, !bitwidth !1646
  %sext_ln25_3 = sext i8 %b_val_read to i35, !bitwidth !1646
  %mul_ln25 = mul i35 %sext_ln25_2, %sext_ln25_3, !bitwidth !1647
  %sext_ln25_4 = sext i35 %mul_ln25 to i36, !bitwidth !1632
  %add_ln25_1 = add i36 %sext_ln25_4, %PCIN_val_read, !bitwidth !1552
  ret i36 %add_ln25_1, !bitwidth !1599
}

define internal fastcc { i18, i18 } @_simd_muladd_extract_2(i36 %M_val) nounwind readnone noinline {
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

define weak i8 @_ssdm_op_Read.ap_auto.i8(i8) {
entry:
  ret i8 %0
}

define weak i1 @_ssdm_op_BitSelect.i1.i36.i32(i36, i32) nounwind readnone {
entry:
  %empty = zext i32 %1 to i36
  %empty_7 = shl i36 1, %empty
  %empty_8 = and i36 %0, %empty_7
  %empty_9 = icmp ne i36 %empty_8, 0
  ret i1 %empty_9
}

define weak i26 @_ssdm_op_BitConcatenate.i26.i8.i18(i8, i18) nounwind readnone {
entry:
  %empty = zext i8 %0 to i26
  %empty_10 = zext i18 %1 to i26
  %empty_11 = shl i26 %empty, 18
  %empty_12 = or i26 %empty_11, %empty_10
  ret i26 %empty_12
}

define weak i18 @_ssdm_op_PartSelect.i18.i36.i32.i32(i36, i32, i32) nounwind readnone {
entry:
  %empty = call i36 @llvm.part.select.i36(i36 %0, i32 %1, i32 %2)
  %empty_6 = trunc i36 %empty to i18
  ret i18 %empty_6
}

declare i36 @llvm.part.select.i36(i36, i32, i32) nounwind readnone

!1548 = metadata !{i32 8, i32 8, i32 0, i32 2}
!1552 = metadata !{i32 36, i32 36, i32 0, i32 2}
!1561 = metadata !{i32 36, i32 36, i32 0, i32 2}
!1599 = metadata !{i32 0, i32 0, i32 0, i32 2}
!1600 = metadata !{i32 18, i32 18, i32 0, i32 2}
!1632 = metadata !{i32 36, i32 36, i32 0, i32 1}
!1639 = metadata !{i32 26, i32 26, i32 0, i32 2}
!1644 = metadata !{i32 27, i32 27, i32 0, i32 1}
!1646 = metadata !{i32 35, i32 35, i32 0, i32 1}
!1647 = metadata !{i32 35, i32 35, i32 0, i32 2}
!1652 = metadata !{i32 1, i32 1, i32 0, i32 2}
!1653 = metadata !{i32 18, i32 18, i32 0, i32 0}
