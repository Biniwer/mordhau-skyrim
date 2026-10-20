; made by gen_exports.py: every export of dwmapi.dll jumps to the real one, looked up on its first call
EXTERN g_real:QWORD
EXTERN ResolveReal:PROC
.code
resolve PROC FRAME
	push rcx
	.pushreg rcx
	push rdx
	.pushreg rdx
	push r8
	.pushreg r8
	push r9
	.pushreg r9
	sub rsp, 104
	.allocstack 104
	.endprolog
	movdqu XMMWORD PTR [rsp + 32], xmm0
	movdqu XMMWORD PTR [rsp + 48], xmm1
	movdqu XMMWORD PTR [rsp + 64], xmm2
	movdqu XMMWORD PTR [rsp + 80], xmm3
	mov ecx, eax
	call ResolveReal
	movdqu xmm0, XMMWORD PTR [rsp + 32]
	movdqu xmm1, XMMWORD PTR [rsp + 48]
	movdqu xmm2, XMMWORD PTR [rsp + 64]
	movdqu xmm3, XMMWORD PTR [rsp + 80]
	add rsp, 104
	pop r9
	pop r8
	pop rdx
	pop rcx
	jmp rax
resolve ENDP
f_0 PROC
	mov rax, QWORD PTR [g_real + 0]
	test rax, rax
	jz first_0
	jmp rax
first_0:
	mov eax, 0
	jmp resolve
f_0 ENDP
f_1 PROC
	mov rax, QWORD PTR [g_real + 8]
	test rax, rax
	jz first_1
	jmp rax
first_1:
	mov eax, 1
	jmp resolve
f_1 ENDP
f_2 PROC
	mov rax, QWORD PTR [g_real + 16]
	test rax, rax
	jz first_2
	jmp rax
first_2:
	mov eax, 2
	jmp resolve
f_2 ENDP
f_3 PROC
	mov rax, QWORD PTR [g_real + 24]
	test rax, rax
	jz first_3
	jmp rax
first_3:
	mov eax, 3
	jmp resolve
f_3 ENDP
f_4 PROC
	mov rax, QWORD PTR [g_real + 32]
	test rax, rax
	jz first_4
	jmp rax
first_4:
	mov eax, 4
	jmp resolve
f_4 ENDP
f_5 PROC
	mov rax, QWORD PTR [g_real + 40]
	test rax, rax
	jz first_5
	jmp rax
first_5:
	mov eax, 5
	jmp resolve
f_5 ENDP
f_6 PROC
	mov rax, QWORD PTR [g_real + 48]
	test rax, rax
	jz first_6
	jmp rax
first_6:
	mov eax, 6
	jmp resolve
f_6 ENDP
f_7 PROC
	mov rax, QWORD PTR [g_real + 56]
	test rax, rax
	jz first_7
	jmp rax
first_7:
	mov eax, 7
	jmp resolve
f_7 ENDP
f_8 PROC
	mov rax, QWORD PTR [g_real + 64]
	test rax, rax
	jz first_8
	jmp rax
first_8:
	mov eax, 8
	jmp resolve
f_8 ENDP
f_9 PROC
	mov rax, QWORD PTR [g_real + 72]
	test rax, rax
	jz first_9
	jmp rax
first_9:
	mov eax, 9
	jmp resolve
f_9 ENDP
f_10 PROC
	mov rax, QWORD PTR [g_real + 80]
	test rax, rax
	jz first_10
	jmp rax
first_10:
	mov eax, 10
	jmp resolve
f_10 ENDP
f_11 PROC
	mov rax, QWORD PTR [g_real + 88]
	test rax, rax
	jz first_11
	jmp rax
first_11:
	mov eax, 11
	jmp resolve
f_11 ENDP
f_12 PROC
	mov rax, QWORD PTR [g_real + 96]
	test rax, rax
	jz first_12
	jmp rax
first_12:
	mov eax, 12
	jmp resolve
f_12 ENDP
f_13 PROC
	mov rax, QWORD PTR [g_real + 104]
	test rax, rax
	jz first_13
	jmp rax
first_13:
	mov eax, 13
	jmp resolve
f_13 ENDP
f_14 PROC
	mov rax, QWORD PTR [g_real + 112]
	test rax, rax
	jz first_14
	jmp rax
first_14:
	mov eax, 14
	jmp resolve
f_14 ENDP
f_15 PROC
	mov rax, QWORD PTR [g_real + 120]
	test rax, rax
	jz first_15
	jmp rax
first_15:
	mov eax, 15
	jmp resolve
f_15 ENDP
f_16 PROC
	mov rax, QWORD PTR [g_real + 128]
	test rax, rax
	jz first_16
	jmp rax
first_16:
	mov eax, 16
	jmp resolve
f_16 ENDP
f_17 PROC
	mov rax, QWORD PTR [g_real + 136]
	test rax, rax
	jz first_17
	jmp rax
first_17:
	mov eax, 17
	jmp resolve
f_17 ENDP
f_18 PROC
	mov rax, QWORD PTR [g_real + 144]
	test rax, rax
	jz first_18
	jmp rax
first_18:
	mov eax, 18
	jmp resolve
f_18 ENDP
f_19 PROC
	mov rax, QWORD PTR [g_real + 152]
	test rax, rax
	jz first_19
	jmp rax
first_19:
	mov eax, 19
	jmp resolve
f_19 ENDP
f_20 PROC
	mov rax, QWORD PTR [g_real + 160]
	test rax, rax
	jz first_20
	jmp rax
first_20:
	mov eax, 20
	jmp resolve
f_20 ENDP
f_21 PROC
	mov rax, QWORD PTR [g_real + 168]
	test rax, rax
	jz first_21
	jmp rax
first_21:
	mov eax, 21
	jmp resolve
f_21 ENDP
f_22 PROC
	mov rax, QWORD PTR [g_real + 176]
	test rax, rax
	jz first_22
	jmp rax
first_22:
	mov eax, 22
	jmp resolve
f_22 ENDP
f_23 PROC
	mov rax, QWORD PTR [g_real + 184]
	test rax, rax
	jz first_23
	jmp rax
first_23:
	mov eax, 23
	jmp resolve
f_23 ENDP
f_24 PROC
	mov rax, QWORD PTR [g_real + 192]
	test rax, rax
	jz first_24
	jmp rax
first_24:
	mov eax, 24
	jmp resolve
f_24 ENDP
f_25 PROC
	mov rax, QWORD PTR [g_real + 200]
	test rax, rax
	jz first_25
	jmp rax
first_25:
	mov eax, 25
	jmp resolve
f_25 ENDP
f_26 PROC
	mov rax, QWORD PTR [g_real + 208]
	test rax, rax
	jz first_26
	jmp rax
first_26:
	mov eax, 26
	jmp resolve
f_26 ENDP
f_27 PROC
	mov rax, QWORD PTR [g_real + 216]
	test rax, rax
	jz first_27
	jmp rax
first_27:
	mov eax, 27
	jmp resolve
f_27 ENDP
f_28 PROC
	mov rax, QWORD PTR [g_real + 224]
	test rax, rax
	jz first_28
	jmp rax
first_28:
	mov eax, 28
	jmp resolve
f_28 ENDP
f_29 PROC
	mov rax, QWORD PTR [g_real + 232]
	test rax, rax
	jz first_29
	jmp rax
first_29:
	mov eax, 29
	jmp resolve
f_29 ENDP
f_30 PROC
	mov rax, QWORD PTR [g_real + 240]
	test rax, rax
	jz first_30
	jmp rax
first_30:
	mov eax, 30
	jmp resolve
f_30 ENDP
f_31 PROC
	mov rax, QWORD PTR [g_real + 248]
	test rax, rax
	jz first_31
	jmp rax
first_31:
	mov eax, 31
	jmp resolve
f_31 ENDP
f_32 PROC
	mov rax, QWORD PTR [g_real + 256]
	test rax, rax
	jz first_32
	jmp rax
first_32:
	mov eax, 32
	jmp resolve
f_32 ENDP
f_33 PROC
	mov rax, QWORD PTR [g_real + 264]
	test rax, rax
	jz first_33
	jmp rax
first_33:
	mov eax, 33
	jmp resolve
f_33 ENDP
f_34 PROC
	mov rax, QWORD PTR [g_real + 272]
	test rax, rax
	jz first_34
	jmp rax
first_34:
	mov eax, 34
	jmp resolve
f_34 ENDP
f_35 PROC
	mov rax, QWORD PTR [g_real + 280]
	test rax, rax
	jz first_35
	jmp rax
first_35:
	mov eax, 35
	jmp resolve
f_35 ENDP
f_36 PROC
	mov rax, QWORD PTR [g_real + 288]
	test rax, rax
	jz first_36
	jmp rax
first_36:
	mov eax, 36
	jmp resolve
f_36 ENDP
f_37 PROC
	mov rax, QWORD PTR [g_real + 296]
	test rax, rax
	jz first_37
	jmp rax
first_37:
	mov eax, 37
	jmp resolve
f_37 ENDP
f_38 PROC
	mov rax, QWORD PTR [g_real + 304]
	test rax, rax
	jz first_38
	jmp rax
first_38:
	mov eax, 38
	jmp resolve
f_38 ENDP
f_39 PROC
	mov rax, QWORD PTR [g_real + 312]
	test rax, rax
	jz first_39
	jmp rax
first_39:
	mov eax, 39
	jmp resolve
f_39 ENDP
f_40 PROC
	mov rax, QWORD PTR [g_real + 320]
	test rax, rax
	jz first_40
	jmp rax
first_40:
	mov eax, 40
	jmp resolve
f_40 ENDP
f_41 PROC
	mov rax, QWORD PTR [g_real + 328]
	test rax, rax
	jz first_41
	jmp rax
first_41:
	mov eax, 41
	jmp resolve
f_41 ENDP
f_42 PROC
	mov rax, QWORD PTR [g_real + 336]
	test rax, rax
	jz first_42
	jmp rax
first_42:
	mov eax, 42
	jmp resolve
f_42 ENDP
f_43 PROC
	mov rax, QWORD PTR [g_real + 344]
	test rax, rax
	jz first_43
	jmp rax
first_43:
	mov eax, 43
	jmp resolve
f_43 ENDP
f_44 PROC
	mov rax, QWORD PTR [g_real + 352]
	test rax, rax
	jz first_44
	jmp rax
first_44:
	mov eax, 44
	jmp resolve
f_44 ENDP
f_45 PROC
	mov rax, QWORD PTR [g_real + 360]
	test rax, rax
	jz first_45
	jmp rax
first_45:
	mov eax, 45
	jmp resolve
f_45 ENDP
f_46 PROC
	mov rax, QWORD PTR [g_real + 368]
	test rax, rax
	jz first_46
	jmp rax
first_46:
	mov eax, 46
	jmp resolve
f_46 ENDP
f_47 PROC
	mov rax, QWORD PTR [g_real + 376]
	test rax, rax
	jz first_47
	jmp rax
first_47:
	mov eax, 47
	jmp resolve
f_47 ENDP
f_48 PROC
	mov rax, QWORD PTR [g_real + 384]
	test rax, rax
	jz first_48
	jmp rax
first_48:
	mov eax, 48
	jmp resolve
f_48 ENDP
f_49 PROC
	mov rax, QWORD PTR [g_real + 392]
	test rax, rax
	jz first_49
	jmp rax
first_49:
	mov eax, 49
	jmp resolve
f_49 ENDP
f_50 PROC
	mov rax, QWORD PTR [g_real + 400]
	test rax, rax
	jz first_50
	jmp rax
first_50:
	mov eax, 50
	jmp resolve
f_50 ENDP
f_51 PROC
	mov rax, QWORD PTR [g_real + 408]
	test rax, rax
	jz first_51
	jmp rax
first_51:
	mov eax, 51
	jmp resolve
f_51 ENDP
f_52 PROC
	mov rax, QWORD PTR [g_real + 416]
	test rax, rax
	jz first_52
	jmp rax
first_52:
	mov eax, 52
	jmp resolve
f_52 ENDP
f_53 PROC
	mov rax, QWORD PTR [g_real + 424]
	test rax, rax
	jz first_53
	jmp rax
first_53:
	mov eax, 53
	jmp resolve
f_53 ENDP
f_54 PROC
	mov rax, QWORD PTR [g_real + 432]
	test rax, rax
	jz first_54
	jmp rax
first_54:
	mov eax, 54
	jmp resolve
f_54 ENDP
f_55 PROC
	mov rax, QWORD PTR [g_real + 440]
	test rax, rax
	jz first_55
	jmp rax
first_55:
	mov eax, 55
	jmp resolve
f_55 ENDP
f_56 PROC
	mov rax, QWORD PTR [g_real + 448]
	test rax, rax
	jz first_56
	jmp rax
first_56:
	mov eax, 56
	jmp resolve
f_56 ENDP
f_57 PROC
	mov rax, QWORD PTR [g_real + 456]
	test rax, rax
	jz first_57
	jmp rax
first_57:
	mov eax, 57
	jmp resolve
f_57 ENDP
f_58 PROC
	mov rax, QWORD PTR [g_real + 464]
	test rax, rax
	jz first_58
	jmp rax
first_58:
	mov eax, 58
	jmp resolve
f_58 ENDP
f_59 PROC
	mov rax, QWORD PTR [g_real + 472]
	test rax, rax
	jz first_59
	jmp rax
first_59:
	mov eax, 59
	jmp resolve
f_59 ENDP
f_60 PROC
	mov rax, QWORD PTR [g_real + 480]
	test rax, rax
	jz first_60
	jmp rax
first_60:
	mov eax, 60
	jmp resolve
f_60 ENDP
f_61 PROC
	mov rax, QWORD PTR [g_real + 488]
	test rax, rax
	jz first_61
	jmp rax
first_61:
	mov eax, 61
	jmp resolve
f_61 ENDP
f_62 PROC
	mov rax, QWORD PTR [g_real + 496]
	test rax, rax
	jz first_62
	jmp rax
first_62:
	mov eax, 62
	jmp resolve
f_62 ENDP
f_63 PROC
	mov rax, QWORD PTR [g_real + 504]
	test rax, rax
	jz first_63
	jmp rax
first_63:
	mov eax, 63
	jmp resolve
f_63 ENDP
f_64 PROC
	mov rax, QWORD PTR [g_real + 512]
	test rax, rax
	jz first_64
	jmp rax
first_64:
	mov eax, 64
	jmp resolve
f_64 ENDP
f_65 PROC
	mov rax, QWORD PTR [g_real + 520]
	test rax, rax
	jz first_65
	jmp rax
first_65:
	mov eax, 65
	jmp resolve
f_65 ENDP
f_66 PROC
	mov rax, QWORD PTR [g_real + 528]
	test rax, rax
	jz first_66
	jmp rax
first_66:
	mov eax, 66
	jmp resolve
f_66 ENDP
f_67 PROC
	mov rax, QWORD PTR [g_real + 536]
	test rax, rax
	jz first_67
	jmp rax
first_67:
	mov eax, 67
	jmp resolve
f_67 ENDP
f_68 PROC
	mov rax, QWORD PTR [g_real + 544]
	test rax, rax
	jz first_68
	jmp rax
first_68:
	mov eax, 68
	jmp resolve
f_68 ENDP
f_69 PROC
	mov rax, QWORD PTR [g_real + 552]
	test rax, rax
	jz first_69
	jmp rax
first_69:
	mov eax, 69
	jmp resolve
f_69 ENDP
f_70 PROC
	mov rax, QWORD PTR [g_real + 560]
	test rax, rax
	jz first_70
	jmp rax
first_70:
	mov eax, 70
	jmp resolve
f_70 ENDP
f_71 PROC
	mov rax, QWORD PTR [g_real + 568]
	test rax, rax
	jz first_71
	jmp rax
first_71:
	mov eax, 71
	jmp resolve
f_71 ENDP
f_72 PROC
	mov rax, QWORD PTR [g_real + 576]
	test rax, rax
	jz first_72
	jmp rax
first_72:
	mov eax, 72
	jmp resolve
f_72 ENDP
f_73 PROC
	mov rax, QWORD PTR [g_real + 584]
	test rax, rax
	jz first_73
	jmp rax
first_73:
	mov eax, 73
	jmp resolve
f_73 ENDP
f_74 PROC
	mov rax, QWORD PTR [g_real + 592]
	test rax, rax
	jz first_74
	jmp rax
first_74:
	mov eax, 74
	jmp resolve
f_74 ENDP
f_75 PROC
	mov rax, QWORD PTR [g_real + 600]
	test rax, rax
	jz first_75
	jmp rax
first_75:
	mov eax, 75
	jmp resolve
f_75 ENDP
f_76 PROC
	mov rax, QWORD PTR [g_real + 608]
	test rax, rax
	jz first_76
	jmp rax
first_76:
	mov eax, 76
	jmp resolve
f_76 ENDP
f_77 PROC
	mov rax, QWORD PTR [g_real + 616]
	test rax, rax
	jz first_77
	jmp rax
first_77:
	mov eax, 77
	jmp resolve
f_77 ENDP
f_78 PROC
	mov rax, QWORD PTR [g_real + 624]
	test rax, rax
	jz first_78
	jmp rax
first_78:
	mov eax, 78
	jmp resolve
f_78 ENDP
f_79 PROC
	mov rax, QWORD PTR [g_real + 632]
	test rax, rax
	jz first_79
	jmp rax
first_79:
	mov eax, 79
	jmp resolve
f_79 ENDP
f_80 PROC
	mov rax, QWORD PTR [g_real + 640]
	test rax, rax
	jz first_80
	jmp rax
first_80:
	mov eax, 80
	jmp resolve
f_80 ENDP
f_81 PROC
	mov rax, QWORD PTR [g_real + 648]
	test rax, rax
	jz first_81
	jmp rax
first_81:
	mov eax, 81
	jmp resolve
f_81 ENDP
f_82 PROC
	mov rax, QWORD PTR [g_real + 656]
	test rax, rax
	jz first_82
	jmp rax
first_82:
	mov eax, 82
	jmp resolve
f_82 ENDP
f_83 PROC
	mov rax, QWORD PTR [g_real + 664]
	test rax, rax
	jz first_83
	jmp rax
first_83:
	mov eax, 83
	jmp resolve
f_83 ENDP
f_84 PROC
	mov rax, QWORD PTR [g_real + 672]
	test rax, rax
	jz first_84
	jmp rax
first_84:
	mov eax, 84
	jmp resolve
f_84 ENDP
f_85 PROC
	mov rax, QWORD PTR [g_real + 680]
	test rax, rax
	jz first_85
	jmp rax
first_85:
	mov eax, 85
	jmp resolve
f_85 ENDP
f_86 PROC
	mov rax, QWORD PTR [g_real + 688]
	test rax, rax
	jz first_86
	jmp rax
first_86:
	mov eax, 86
	jmp resolve
f_86 ENDP
f_87 PROC
	mov rax, QWORD PTR [g_real + 696]
	test rax, rax
	jz first_87
	jmp rax
first_87:
	mov eax, 87
	jmp resolve
f_87 ENDP
f_88 PROC
	mov rax, QWORD PTR [g_real + 704]
	test rax, rax
	jz first_88
	jmp rax
first_88:
	mov eax, 88
	jmp resolve
f_88 ENDP
f_89 PROC
	mov rax, QWORD PTR [g_real + 712]
	test rax, rax
	jz first_89
	jmp rax
first_89:
	mov eax, 89
	jmp resolve
f_89 ENDP
f_90 PROC
	mov rax, QWORD PTR [g_real + 720]
	test rax, rax
	jz first_90
	jmp rax
first_90:
	mov eax, 90
	jmp resolve
f_90 ENDP
f_91 PROC
	mov rax, QWORD PTR [g_real + 728]
	test rax, rax
	jz first_91
	jmp rax
first_91:
	mov eax, 91
	jmp resolve
f_91 ENDP
f_92 PROC
	mov rax, QWORD PTR [g_real + 736]
	test rax, rax
	jz first_92
	jmp rax
first_92:
	mov eax, 92
	jmp resolve
f_92 ENDP
f_93 PROC
	mov rax, QWORD PTR [g_real + 744]
	test rax, rax
	jz first_93
	jmp rax
first_93:
	mov eax, 93
	jmp resolve
f_93 ENDP
f_94 PROC
	mov rax, QWORD PTR [g_real + 752]
	test rax, rax
	jz first_94
	jmp rax
first_94:
	mov eax, 94
	jmp resolve
f_94 ENDP
f_95 PROC
	mov rax, QWORD PTR [g_real + 760]
	test rax, rax
	jz first_95
	jmp rax
first_95:
	mov eax, 95
	jmp resolve
f_95 ENDP
f_96 PROC
	mov rax, QWORD PTR [g_real + 768]
	test rax, rax
	jz first_96
	jmp rax
first_96:
	mov eax, 96
	jmp resolve
f_96 ENDP
f_97 PROC
	mov rax, QWORD PTR [g_real + 776]
	test rax, rax
	jz first_97
	jmp rax
first_97:
	mov eax, 97
	jmp resolve
f_97 ENDP
f_98 PROC
	mov rax, QWORD PTR [g_real + 784]
	test rax, rax
	jz first_98
	jmp rax
first_98:
	mov eax, 98
	jmp resolve
f_98 ENDP
END
