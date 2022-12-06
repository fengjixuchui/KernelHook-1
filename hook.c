#include "hook.h"

VOID wpoff1()
{
	//KIRQL  irql = KeRaiseIrqlToDpcLevel();
	_disable();
	ULONG64 cr0 = __readcr0() & 0xfffffffffffeffffi64;
	__writecr0(cr0);
	//return irql;
}

VOID wpon1(/*KIRQL  irql*/)
{
	ULONG64 cr0 = __readcr0() | 0x10000i64;
	__writecr0(cr0);
	_enable();
	//KeLowerIrql(irql);
}

ULONG64 wpoff()
{
	_disable();
	ULONG64 mcr0 = __readcr0();
	__writecr0(mcr0 & (~0x10000));

	return mcr0;
}

VOID wpon(ULONG64 mcr0)
{
	__writecr0(mcr0);
	_enable();
}

int get_hook_len(ULONG64 Addr, ULONG64 size, BOOLEAN isX64)
{
	PUCHAR tempAddr = Addr;
	int totalSize = 0;
	int len = 0;

	if (isX64)
	{
		do
		{
			len = insn_len_x86_64((ULONG64)tempAddr);

			tempAddr = tempAddr + len;

			totalSize += len;

		} while (totalSize < size);
	}
	else
	{
		do
		{
			len = insn_len_x86_32((ULONG64)tempAddr);

			tempAddr = tempAddr + len;

			totalSize += len;

		} while (totalSize < size);
	}

	return totalSize;
}

ULONG64 allocateMemory(ULONG64 size)
{
	if (size < PAGE_SIZE) size = PAGE_SIZE;
	// �����޸��ڴ���䷽ʽʹ�������
	return ExAllocatePool(NonPagedPool, size);
}

VOID freeMemory(ULONG64 addr)
{
	return ExFreePool(addr);
}

// 36��hookhandler��ƫ��
// 67��origin��ַ��ƫ��
UCHAR handler_shellcode[] =
{
0x41,0x57                         // push r15                           
,0x41,0x56                       // push r14                           
,0x41,0x55                         // push r13                           
,0x41,0x54                         // push r12                           
,0x41,0x53                         // push r11                           
,0x41,0x52                         // push r10                           
,0x41,0x51                         // push r9                            
,0x41,0x50                         // push r8                            
,0x57                            // push rdi                           
,0x56                            // push rsi                           
,0x55                            // push rbp                           
,0x54                            // push rsp                           
,0x53                            // push rbx                           
,0x52                            // push rdx                           
,0x51                            // push rcx                           
,0x50                            // push rax          
,0x9C                            // pushfq
,0x48,0x8B,0xCC                       // mov rcx,rsp                        
,0x48,0x81,0xEC,0x00,0x01,0x00,0x00              // sub rsp,100        

// call hookhandler
,0xEB,0x08 // jmp (��������Ĵ���)
,0x00,0x00 // add byte ptr ds : [rax] ,al |
,0x00,0x00 // add byte ptr ds : [rax] ,al |
,0x00,0x00 // add byte ptr ds : [rax] ,al |
,0x00,0x00 // add byte ptr ds : [rax] ,al |
,0xFF,0x15,0xF2,0xFF,0xFF,0xFF // call qword ptr ds : [7FF9AAEA069B] |

,0x48,0x81,0xC4,0x00,0x01,0x00,0x00 // add rsp,100 |

// ���rax=1��˵����Ҫֱ�ӷ��أ�����ֵ����rcx�С�����jmp��ԭ��Ҫִ�еĴ��봦����ִ�С�
,0x3C,0x00 // cmp al,0
,0x75,0x0E // jne (��������14���ֽ�)
,0xFF,0x25,0x00,0x00,0x00,0x00 // jmp qword ptr ds : [7FF9AAEA06FA] 
,0x00,0x00 // add byte ptr ds : [rax] ,al 
,0x00,0x00 // add byte ptr ds : [rax] ,al 
,0x00,0x00 // add byte ptr ds : [rax] ,al 
,0x00,0x00 // add byte ptr ds : [rax] ,al 

,0x9D                            // pop rflags
,0x58                            // pop rax   
,0x59                            // pop rcx                            
,0x5A                            // pop rdx                            
,0x5B                            // pop rbx                            
,0x5C                            // pop rsp                            
,0x5D                            // pop rbp                            
,0x5E                            // pop rsi                            
,0x5F                            // pop rdi                            
,0x41,0x58                         // pop r8                             
,0x41,0x59                         // pop r9                             
,0x41,0x5A                         // pop r10                            
,0x41,0x5B                         // pop r11                            
,0x41,0x5C                         // pop r12                            
,0x41,0x5D                         // pop r13                            
,0x41,0x5E                         // pop r14                            
,0x41,0x5F                         // pop r15
,0xC3							   // ret
};

UCHAR resume_code[] =
{
0x9D                              // pop rflags
,0x58                             // pop rcx                    
,0x59                            // pop rcx                            
,0x5A                            // pop rdx                            
,0x5B                            // pop rbx                            
,0x5C                            // pop rsp                            
,0x5D                            // pop rbp                            
,0x5E                            // pop rsi                            
,0x5F                            // pop rdi                            
,0x41,0x58                         // pop r8                             
,0x41,0x59                         // pop r9                             
,0x41,0x5A                         // pop r10                            
,0x41,0x5B                         // pop r11                            
,0x41,0x5C                         // pop r12                            
,0x41,0x5D                         // pop r13                            
,0x41,0x5E                         // pop r14                            
,0x41,0x5F                         // pop r15
};

static phook_record head = NULL;
static ULONG64 record_ct = 0;

VOID
my_AppendTailList(
	_Inout_ PLIST_ENTRY ListHead,
	_Inout_ PLIST_ENTRY ListToAppend
)
{
	PLIST_ENTRY ListEnd = ListHead->Blink;

	RtlpCheckListEntry(ListHead);
	RtlpCheckListEntry(ListToAppend);
	ListHead->Blink->Flink = ListToAppend;
	ListHead->Blink = ListToAppend->Blink;
	ListToAppend->Blink->Flink = ListHead;
	ListToAppend->Blink = ListEnd;
	return;
}

// TODO���жϺ�������
// ǰ������ģ�����ͺ���������������callback�ĵ�ַ�����ĸ������ģ���Ӧ���±�

NTSTATUS hook_by_addr(ULONG64 funcAddr, ULONG64 callbackFunc, OUT ULONG64* record_number)
{
	if (!funcAddr || !MmIsAddressValid(funcAddr))
	{
		return STATUS_NOT_FOUND;
	}

	ULONG64 handler_addr = allocateMemory(PAGE_SIZE);
	if (!handler_addr)
		return STATUS_MEMORY_NOT_ALLOCATED;
	RtlMoveMemory(handler_addr, handler_shellcode, sizeof(handler_shellcode));
	char bufcode[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

	// ��ȡ��Ҫpatch��ָ���
	ULONG64 inslen = get_hook_len(funcAddr, sizeof(bufcode), TRUE);
	ULONG64 shellcode_origin_addr = allocateMemory(PAGE_SIZE);
	if (!shellcode_origin_addr)
		return STATUS_MEMORY_NOT_ALLOCATED;


	/*
	ԭʼ����
	ff 25 xxxxxx(����ԭ��)
	*/
	RtlMoveMemory(shellcode_origin_addr, resume_code, sizeof(resume_code));
	RtlMoveMemory(shellcode_origin_addr + sizeof(resume_code), funcAddr, inslen);

	// ʹ��zydis�ҳ�ʹ������Ե�ַ�����
	ZyanU64 runtime_address = funcAddr;

	// ���������Ե�ַ�����ʱ�����˶೤�Ĵ��롣��������дff25��ʱ��Ҫ����ε�ַҲ��������
	ULONG64 resolve_relative_code_len = 0;

	// ����ȷ��ABC���Ƿ���ʹ������Ե�ַ�Ĵ���ı�ǡ�����еĻ��ں��滹Ҫ�ټ���һ��ebjmp��������resolve��Ե�ַ�Ĵ�������ԭʼ��ff25jmp����
	BOOLEAN relative_addr_used_in_abc = FALSE;

	ZyanUSize cur_disasm_offset = 0;
	ZydisDisassembledInstruction instruction;
	while (ZYAN_SUCCESS(ZydisDisassembleIntel(
		/* machine_mode:    */ ZYDIS_MACHINE_MODE_LONG_64,
		/* runtime_address: */ runtime_address,
		/* buffer:          */ shellcode_origin_addr + sizeof(resume_code) + cur_disasm_offset,
		/* length:          */ inslen - cur_disasm_offset,
		/* instruction:     */ &instruction
	))) {
		KdPrintEx((77, 0, "%llx %s\n", runtime_address, instruction.text));


		// �ж��Ƿ�����Ե�ַ
		if (instruction.info.attributes & ZYDIS_ATTRIB_IS_RELATIVE)
		{
			// ���flag��������flag�����˵Ļ�����Ҫ����һ��ebjmp��������ff25����ԭ��������Ĵ��봦��
			if (relative_addr_used_in_abc == FALSE)
			{
				relative_addr_used_in_abc = TRUE;
				// ��Ӧ���ǵ�һ�Σ�Ӧ��Ϊ0
				//ASSERT(resolve_relative_code_len == 0);
				if (resolve_relative_code_len != 0)
				{
					KdPrintEx((77, 0, "resolve_relative_code_len != 0\r\n"));
					return STATUS_INTERNAL_ERROR;
				}
				UCHAR t_ebjmp[2] = { 0xeb, 0x00 };
				// ABC�����ټ���һ��ebjmp��������������resolve�Ĵ���
				RtlMoveMemory(shellcode_origin_addr + sizeof(resume_code) + inslen + resolve_relative_code_len, t_ebjmp, sizeof(t_ebjmp));
				// ����resolve�Ĵ���ĳ���+=2
				resolve_relative_code_len += sizeof(t_ebjmp);
			}

			KdPrintEx((77, 0, "ZYDIS_ATTRIB_IS_RELATIVE\r\n"));
			KdPrintEx((77, 0, "���ָ�����Ϣ��%llx", funcAddr + cur_disasm_offset));
			for (int i = 0; i < instruction.info.length; i++)
				KdPrintEx((77, 0, " %02hhx", *(PUCHAR)(funcAddr + cur_disasm_offset + i)));
			KdPrintEx((77, 0, " %s\r\n", instruction.text));
			// 0x7X һ�ֽ������ת
			if (instruction.info.length == 2 && (*(PUCHAR)runtime_address <= 0x7f && *(PUCHAR)runtime_address >= 0x70))
			{
				KdPrintEx((77, 0, "0x7x һ�ֽ������ת��\r\n"));
				// 1.ȷ������ָ��ԭ��Ҫ��ת���ĸ���ַ
				ULONG64 original_jx_addr = funcAddr + cur_disasm_offset + 2 + *(PCHAR)(funcAddr + cur_disasm_offset + 1); // ��һ����ַ+offset
				KdPrintEx((77, 0, "original_jx_addr = %llx\r\n", original_jx_addr));
				// 2.����ff25jmp��д������

				// ��������ff25�ĵ�ַ���ں�������jcc��ת�ĵ�ַ��ʱ������õ�
				ULONG64 t_ff25jmp_addr = shellcode_origin_addr + sizeof(resume_code) + inslen + resolve_relative_code_len;

				*(PULONG64)&bufcode[6] = original_jx_addr;
				RtlMoveMemory(shellcode_origin_addr + sizeof(resume_code) + inslen + resolve_relative_code_len, bufcode, sizeof(bufcode));
				resolve_relative_code_len += sizeof(bufcode); // resolve�Ĵ��볤��+=sizeof bufcode

				// 3.����jcc��ת�ĵ�ַ����֤���ܹ���ȷ��ת���ղŹ����ff25jmp��
				KdPrintEx((77, 0, "runtime_address = %llx\r\n", runtime_address));
				ULONG64 t_dummy = t_ff25jmp_addr - (shellcode_origin_addr + sizeof(resume_code) + cur_disasm_offset + 2);
				CHAR offset_for_jx = *(PCHAR)(&t_dummy);
				if (offset_for_jx < 0 || offset_for_jx == 0)
				{
					KdPrintEx((77, 0, "offset_for_jx < 0 || offset_for_jx == 0\r\n"));
					return STATUS_INTERNAL_ERROR;
				}
				KdPrintEx((77, 0, "offset_for_jx = %02hhx\r\n", offset_for_jx));
				// д��jcc��ת�ĵ�ַ��ȥ��
				*(PCHAR)(shellcode_origin_addr + sizeof(resume_code) + cur_disasm_offset + 1) = offset_for_jx;
				KdPrintEx((77, 0, "%llx\r\n", shellcode_origin_addr));
				KdPrintEx((77, 0, "%llx\r\n", shellcode_origin_addr + sizeof(resume_code) + cur_disasm_offset));
			}
			// 0x0f 0x8x xx xx xx xx ���ֽ������ת
			else if (instruction.info.length == 6 && *(PUCHAR)runtime_address == 0x0f && *(PCUCHAR)(runtime_address + 1) <= 0x8f && *(PCUCHAR)(runtime_address + 1) >= 0x80)
			{
				KdPrintEx((77, 0, "0f 8x���ֽڶ�����%llx\r\n", runtime_address));

				// 1.ȷ������ָ��ԭ��Ҫ��ת���ĸ���ַ
				ULONG64 original_jx_addr = funcAddr + cur_disasm_offset + instruction.info.length + *(PLONG)(runtime_address + 2); // ��һ����ַ+offset
				KdPrintEx((77, 0, "original_jx_addr = %llx\r\n", original_jx_addr));
				// 2.����ff25jmp��д������
				// ��������ff25�ĵ�ַ���ں�������jcc��ת�ĵ�ַ��ʱ������õ�
				ULONG64 t_ff25jmp_addr = shellcode_origin_addr + sizeof(resume_code) + inslen + resolve_relative_code_len;

				*(PULONG64)&bufcode[6] = original_jx_addr;
				RtlMoveMemory(shellcode_origin_addr + sizeof(resume_code) + inslen + resolve_relative_code_len, bufcode, sizeof(bufcode));
				resolve_relative_code_len += sizeof(bufcode); // resolve�Ĵ��볤��+=sizeof bufcode

				// 3.����jcc��ת�ĵ�ַ����֤���ܹ���ȷ��ת���ղŹ����ff25jmp��
				KdPrintEx((77, 0, "runtime_address = %llx\r\n", runtime_address));
				ULONG64 t_dummy = t_ff25jmp_addr - (shellcode_origin_addr + sizeof(resume_code) + cur_disasm_offset + 6);
				LONG offset_for_jx = *(PLONG)(&t_dummy);
				if (offset_for_jx < 0 || offset_for_jx == 0)
				{
					KdPrintEx((77, 0, "offset_for_jx < 0 || offset_for_jx == 0\r\n"));
					return STATUS_INTERNAL_ERROR;
				}
				KdPrintEx((77, 0, "offset_for_jx = %08x\r\n", offset_for_jx));
				// д��jcc��ת�ĵ�ַ��ȥ��
				*(PLONG)(shellcode_origin_addr + sizeof(resume_code) + cur_disasm_offset + 2) = offset_for_jx;
				KdPrintEx((77, 0, "%llx\r\n", shellcode_origin_addr));
				KdPrintEx((77, 0, "%llx\r\n", shellcode_origin_addr + sizeof(resume_code) + cur_disasm_offset));
			}
			else
			{
				return STATUS_INTERNAL_ERROR;
			}
		}

		cur_disasm_offset += instruction.info.length;
		runtime_address += instruction.info.length;
	}

	// �Ѻ����Ǹ�������ת��ԭ�����Ǹ�jmp��eb�޸�һ��
	if (resolve_relative_code_len > 0x79)
	{
		KdPrintEx((77, 0, "resolve_relative_code_len > 0x79\r\n"));
		return STATUS_INTERNAL_ERROR;
	}
	ULONG64 t_dummy = resolve_relative_code_len - 2;
	*(PCHAR)(shellcode_origin_addr + sizeof(resume_code) + inslen + 1) = *(PCHAR)(&t_dummy);
	// ��ff25 jmp����ԭ����ַ�Ĵ��뿽����ABC�ĺ���
	*(PULONG64)&bufcode[6] = funcAddr + inslen;
	RtlMoveMemory(shellcode_origin_addr + sizeof(resume_code) + inslen + resolve_relative_code_len, bufcode, sizeof(bufcode));

	// �޸�handler_shellcode+0x28������HookHandler
	// �޸�handler_shellcode+0x43������
	*(PULONG64)(handler_addr + 37) = callbackFunc;
	*(PULONG64)(handler_addr + 68) = shellcode_origin_addr;

	if (!head)
	{
		head = allocateMemory(PAGE_SIZE);
		InitializeListHead(&head->entry);
	}

	// �����¼
	phook_record record = allocateMemory(PAGE_SIZE);
	record->num = record_ct;
	*record_number = record_ct;
	record_ct++;
	record->addr = funcAddr;
	record->len = inslen;
	record->handler_addr = handler_addr;
	record->shellcode_origin_addr = shellcode_origin_addr;
	RtlMoveMemory(&record->buf, funcAddr, inslen);
	//InsertHeadList(&head->entry, &record->entry);
	InitializeListHead(&record->entry);
	my_AppendTailList(&head->entry, &record->entry);

	// patchԭ����
	*(PULONG64)&bufcode[6] = handler_addr;
	ULONG64 cr0 = wpoff();
	RtlMoveMemory(funcAddr, bufcode, sizeof(bufcode));
	wpon(cr0);

	//freeMemory(handler_addr);
	//freeMemory(shellcode_origin_addr);

	return STATUS_SUCCESS;
}

ULONG64 KeFlushEntireTb();

NTSTATUS reset_hook(ULONG64 record_number)
{
	if (!head)
	{
		return STATUS_NOT_FOUND;
	}
	phook_record cur = head->entry.Flink;
	while (cur != head)
	{
		if (cur->num == record_number)
		{
			ULONG64 cr0 = wpoff();
			RtlMoveMemory(cur->addr, &cur->buf, cur->len);
			wpon(cr0);
			freeMemory(cur->handler_addr);
			freeMemory(cur->shellcode_origin_addr);
			RemoveEntryList(&cur->entry);
			freeMemory(cur);
			//KdPrintEx((77, 0, "%llx�����hook\r\n", record_number));
			KeFlushEntireTb();
			return STATUS_SUCCESS;
		}

		cur = cur->entry.Flink;
	}


	return STATUS_NOT_FOUND;
}

// prehandler��ʽ��������
// cmp XXX
// jnz ��������ԭ����code������ԭʼ�߼���Ȼ�����ص�ԭ��λ��  ; ��һЩ���������ж�
// jmp [eip]  ; һ��ff25 jmp��offset��0
// 00 00
// 00 00
// 00 00
// 00 00
// @��������ԭ����code������ԭʼ�߼���Ȼ�����ص�ԭ��λ��
// ; ������ԭʼ�߼��ɺ���Ĵ����Զ����룬�����ֶ�д��
// 
// 
// ע�������п��ܻᵼ��eflags�ĸı䡣�����Ҫ���ı�eflags����Ҫ��ջ���ٱ���һ��eflags��
// ��hook��һ���ǳ�Ƶ�������õĺ���ʱ����������prehandler����prehandler�н���Ԥ�������ĳ������������Ҫ�󣬾Ͳ���������ı���context��handler��ȥ��
// ��һ��������hook��ţ��ڶ���������prehandler�����Ƶĵ�ַ��������������prehandler������Ĵ�С�����ĸ�������prehandler����jmp��Ŀ���ַ��ƫ�ƣ������滻��
// ������handler_addr+0x600��λ�á����Ҫȷ��prehandler_buf_sizeС��0x400
NTSTATUS set_fast_prehandler(ULONG64 record_number, PUCHAR prehandler_buf, ULONG64 prehandler_buf_size, ULONG64 jmp_addr_offset)
{
	if (!head)
	{
		return STATUS_NOT_FOUND;
	}

	if (prehandler_buf_size > 0x350)
	{
		return STATUS_NO_MEMORY;
	}

	phook_record cur = head->entry.Flink;
	BOOLEAN flag = FALSE;

	while (cur != head)
	{
		if (cur->num == record_number)
		{
			flag = TRUE;
			PUCHAR prehook_buf_addr = (PUCHAR)(cur->handler_addr + 0x600);
			// �Ȱ�prehandler������+0x600��λ��
			RtlMoveMemory(prehook_buf_addr, prehandler_buf, prehandler_buf_size);
			// ���Ȼ�ȡhook����Ǹ�ff25��ת�ĵ�ַ��
			PULONG64 phook_point_jmp_addr = (PULONG64)((ULONG64)cur->addr + 6);
			ULONG64 hook_point_jmp_addr = *phook_point_jmp_addr;
			// Ȼ��������ת�ĵ�ַ���뵽prehandler��Ӧ��jmp_addr_offset��
			PULONG64 pPrehandlerJmpAddr = (PULONG64)((ULONG64)prehook_buf_addr + jmp_addr_offset);
			*pPrehandlerJmpAddr = hook_point_jmp_addr;

			// ���ѱ�����ֽڿ�����prehandler�����
			RtlMoveMemory(prehook_buf_addr + prehandler_buf_size, cur->buf, cur->len);
			// ��ff25��ת��ԭ�����Ĵ��뿽�����������
			UCHAR t_ff25_jmp_buf[] = {
				0xff, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp qword ptr ds : [7FF806EA0974] 
				0x00, 0x00, // add byte ptr ds : [rax] ,al 
				0x00, 0x00, // add byte ptr ds : [rax] ,al 
				0x00, 0x00, // add byte ptr ds : [rax] ,al 
				0x00, 0x00, // add byte ptr ds : [rax] ,al 
			};



			ULONG64 t_jmp_back_addr = cur->addr + cur->len;


			RtlMoveMemory(t_ff25_jmp_buf + 6, &t_jmp_back_addr, sizeof(ULONG64));
			RtlMoveMemory(prehook_buf_addr + prehandler_buf_size + cur->len, t_ff25_jmp_buf, sizeof t_ff25_jmp_buf);
			// ͨ��ԭ�Ӳ�����ԭʼ��hook���ff25��ת��λ�ý�����Ӧ���޸ģ���Ϊprehandler�ĵ�ַ
			//InterlockedExchange64(phook_point_jmp_addr, prehook_buf_addr);
			wpoff1();
			*phook_point_jmp_addr = prehook_buf_addr;
			wpon1();

			break;
		}
		cur = cur->entry.Flink;
	}

	if (!flag) return STATUS_NOT_FOUND;
	else return STATUS_SUCCESS;
}