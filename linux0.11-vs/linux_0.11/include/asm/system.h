static __inline VOID move_to_user_mode()
{
	__asm
	{
		mov		eax, esp		
		push	0x17			// ���Ƚ���ջ��ѡ���( SS )��ջ.	
		push	eax				// Ȼ�󽫱���Ķ�ջָ��ֵ( esp )��ջ
		pushfd					// ����־�Ĵ���( eflags )������ջ
		push	0x0F			// ���ں˴����ѡ���( cs )��ջ  0xF -> 1111b
		push	LN1				// �������� LN1 ��ƫ�Ƶ�ַ( eip )��ջ
		iretd					// ִ���жϷ���ָ��,�����ת�������� LN1 ��
LN1 :							// ��ʱ��ʼִ������0
		mov	eax	, 0x17			// ��ʼ���μĴ���ָ�򱾾ֲ�������ݶ�
		mov	ds	, ax
		mov	es	, ax
		mov	fs	, ax
		mov	gs	, ax
	}
}

#define sti()	__asm sti
#define cli()	__asm cli
#define nop()	__asm nop

#define iret()	__asm iretd

/*
 * 
 * Gate Descriptor
 *
 *   31                            15             8              0
 *	 +-----------------------------+-+---+-+------+--------------+
 *	 |         Offset              |P|DPL|0|TYPE-4|   Reserved   |
 *	 +-----------------------------+-+---+-+------+--------------+
 *	 |      Segment Selector       |         Offset              |
 *	 +-----------------------------+-----------------------------+
 * 
 * 
 *   Gate Type: A 4-bit value which defines the type of gate this Interrupt Descriptor represents. There are five valid type values:
 *   0b0101 or 0x5: Task Gate, note that in this case, the Offset value is unused and should be set to zero.
 *   0b0110 or 0x6: 16-bit Interrupt Gate
 *   0b0111 or 0x7: 16-bit Trap Gate
 *   0b1110 or 0xE: 32-bit Interrupt Gate
 *   0b1111 or 0xF: 32-bit Trap Gate
 * 
 */

static __inline VOID _set_gate( Desc_Struct *gate_addr, LONG type, LONG dpl, VOID *addr )
{
	gate_addr->a = ( ( ULONG )addr & 0x0000FFFF ) + 0x00080000;
		
	gate_addr->b = ( ( ULONG )addr & 0xFFFF0000 ) + ( 0x8000 + ( dpl << 13 ) + ( type << 8 ) );
}

#define set_intr_gate( n,addr ) \
	_set_gate( &idt[ n ],14,0,addr )

#define set_trap_gate( n,addr ) \
	_set_gate( &idt[ n ],15,0,addr )

#define set_system_gate( n,addr ) \
	_set_gate( &idt[ n ],15,3,addr )

/*
 * 
 * Segment Descriptor
 *
 *   31              23            15             8              0
 *	 +---------------+-+-+-+-+-----+-+---+-+------+--------------+
 *	 |   Base(31-24) |G|0|0|A|Limit|P|DPL|0|TYPE-4|     Base     |
 *	 +---------------+-+-+-+-+-----+-+---+-+------+--------------+
 *	 |            Base             |         Limit               |
 *	 +-----------------------------+-----------------------------+
 * 
 * 
 *   TYPE(40..43):	
 *			0001b=0x1	80286-TSS, 16 bit
 *			0010b=0x2	LDT
 *	 		0011b=0x3	activ 80286-TSS, 16 bit
 *			1001b=0x9	80386-TSS, 32 bit
 *			1011b=0xB	activ 80386-TSS, 32 bit
 * 
 *   S	(44): Storage Segment	= 0 for System Segments.
 * 
 *   Limit(48..51) : Limit 16..19	high part of the Limit.
 * 
 */
static __inline VOID _set_tssldt_desc( CHAR *n, VOID *addr, CHAR type )
{
	*( short* )n			= 0x68;
	*( short* )( n + 2 )	=   ( ULONG )addr		  & 0xFFFF;
	*( n + 4 )				= ( ( ULONG )addr >> 16 ) & 0xFF;
	*( n + 5 )				= type;
	*( n + 6 )				= 0;
	*( n + 7 )				= ( CHAR )( ( ( ULONG )addr >> 28 ) );
}

#define set_tss_desc( n,addr ) _set_tssldt_desc( ( ( CHAR* )( n ) ), addr, 0x89 )
#define set_ldt_desc( n,addr ) _set_tssldt_desc( ( ( CHAR* )( n ) ), addr, 0x82 )
