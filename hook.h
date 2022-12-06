#pragma once


#include <ntifs.h>
#include "AsmCode.h"
#include "Zydis.h"
#include <intrin.h>


typedef struct _GuestContext
{
	ULONG64 mRflags;
	ULONG64 mRax;
	ULONG64 mRcx;
	ULONG64 mRdx;
	ULONG64 mRbx;
	ULONG64 mRsp;
	ULONG64 mRbp;
	ULONG64 mRsi;
	ULONG64 mRdi;
	ULONG64 mR8;
	ULONG64 mR9;
	ULONG64 mR10;
	ULONG64 mR11;
	ULONG64 mR12;
	ULONG64 mR13;
	ULONG64 mR14;
	ULONG64 mR15;
}GuestContext, * PGuestContext;

typedef struct _hook_record
{
	LIST_ENTRY64 entry;
	ULONG64 num; // ��¼���
	ULONG64 addr; // hook�ĵ�ַ
	ULONG64 len; // �ֽڵĴ�С
	ULONG64 handler_addr;
	ULONG64 shellcode_origin_addr;
	UCHAR buf[1]; // ������ֽ�
} hook_record, * phook_record;


NTSTATUS hook_by_addr(ULONG64 funcAddr, ULONG64 callbackFunc, OUT ULONG64* record_number);
NTSTATUS reset_hook(ULONG64 record_number);
NTSTATUS set_fast_prehandler(ULONG64 record_number, PUCHAR prehandler_buf, ULONG64 prehandler_buf_size, ULONG64 jmp_addr_offset);

