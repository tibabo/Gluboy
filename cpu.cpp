#include "glouboy.h"
#include "cpu.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "io.h"

bool haltMode = false;
unsigned short PC;
unsigned short SP;

opcodeStruct opcode[256];
opcodeStruct CBopcode[256];
unsigned char TC = 0;

struct {
	struct
	{
		unsigned char dummy0 : 1;
		unsigned char dummy1 : 1;
		unsigned char dummy2 : 1;
		unsigned char dummy3 : 1;
		unsigned char C : 1;
		unsigned char H : 1;
		unsigned char N : 1;
		unsigned char Z : 1;
	} flag;
	unsigned char reg_a;
} afstruct;

#define flags afstruct.flag

#define af (*((unsigned short*)&afstruct))
unsigned short bc, de, hl;
#define reg_f  (*((unsigned char*)&afstruct))
#define reg_a  (*(((unsigned char*)&afstruct) + 1))

#define reg_c  (*((unsigned char*)&bc))
#define reg_b  (*(((unsigned char*)&bc) + 1))

#define reg_e  (*((unsigned char*)&de))
#define reg_d  (*(((unsigned char*)&de) + 1))

#define reg_l  (*((unsigned char*)&hl))
#define reg_h  (*(((unsigned char*)&hl) + 1))


unsigned char ram[0x10000];


#define update_carry() {flags.H = (carrybits & 0x10)>0; flags.C = (carrybits & 0x100)>0;}

#define INC_ss(a) { [](){ TC += 8;  a++;  PC +=1;}, "INC " #a  }
#define INC_r(a,clock) { [](){ TC += clock;  flags.N = 0; flags.H = (a & 0x0F) == 0x0F; a += 1;  flags.Z = (a == 0);  PC +=1;}, "INC " #a }

#define DEC_r(a,clock) { [](){ TC += clock;  flags.N = 1; flags.H = (a & 0x0F) == 0x00; a -= 1;  flags.Z = (a == 0);  PC +=1;}, "DEC " #a }
#define LD_r_n(a,clock) { [](){ TC += clock;  a = ram[PC + 1];                                                     PC +=2;}, "LD " #a ", 0x%02x" }
#define LD_ss_n(a) {[](){ TC += 12; a = ram[PC + 1] | (ram[PC + 2]<<8);                                            PC +=3;}, "LD " #a ", 0x%02x%02x" }
#define LD_ssp_r(a,r) {[](){ TC += 8;               writeRam(a,r);                                                 PC +=1;}, "LD (" #a "), A" }
#define ADD_ss_ss(a,b) {[](){ TC += 8;  flags.N = 0; uint32_t sum = a + b; flags.H = ((a & 0xfff) + (b & 0xfff)) > 0xfff;  a = sum; flags.C = sum > 0xFFFF; PC +=1;}, "ADD " #a ", " #b } 
#define DEC_ss(a) { [](){ TC += 8;  a--;                                                               PC +=1;}, "DEC " #a }
#define LD_r_ss(r,ss) { [](){ TC += 8; r = ram[ss];                                                     PC +=1;}, "LD " #r ", (" #ss ")" }
#define LD_r_r(a,b) { [](){ TC += 4; a = b;                                                             PC +=1;}, "LD " #a ", " #b }
#define SUB_r(x,clock) {  [](){ TC += clock; unsigned char value = x; flags.N = 1; int result = reg_a - value; int carrybits = value ^ reg_a ^ result; reg_a = result; update_carry(); flags.Z = (reg_a == 0); PC +=1;}, "SUB " #x }

#define ADD_r(x,clock)  { [](){ TC += clock; unsigned char value = x; flags.N = 0; int result = reg_a + value; int carrybits = value ^ reg_a ^ result; reg_a = result; update_carry(); flags.Z = (reg_a == 0); PC +=1;}, "ADD A, " #x }

#define AND_r(x,clock)  { [](){ TC += clock;  flags.N = 0; int result = reg_a & x; reg_a = result; flags.H = 1; flags.C = 0; flags.Z = (reg_a == 0); PC +=1;}, "AND A, " #x }
#define OR_r(x,clock)   { [](){ TC += clock;  flags.N = 0; int result = reg_a | x; reg_a = result; flags.H = 0; flags.C = 0; flags.Z = (reg_a == 0); PC +=1;}, "OR A, " #x }
#define ADC_r(x,clock)  { [](){ TC += clock;  flags.N = 0; unsigned char value = x; int result = reg_a + value + flags.C; flags.H = ((reg_a & 0x0F) + (value & 0x0F) + flags.C ) > 0x0F; flags.C = result > 0xFF; reg_a = result; flags.Z = (reg_a == 0); PC +=1;}, "ADC A, " #x }
#define SBC_r(x,clock)  { [](){ TC += clock;  flags.N = 1; unsigned char value = x;  int result = reg_a - value - flags.C; int carrybits = value ^ reg_a ^ result ^ flags.C; update_carry(); reg_a = result; flags.Z = (reg_a == 0); PC +=1;}, "SBC A, " #x }
#define XOR_r(x,clock)  { [](){ TC += clock;  flags.N = 0; int result = reg_a ^ x; reg_a = result; flags.H = 0; flags.C = 0; flags.Z = (reg_a == 0); PC +=1;}, "XOR A, " #x }
#define CP_r(x,clock)   { [](){ TC += clock;  flags.N = 1; unsigned char value = x; int result = reg_a - value; int carrybits = value ^ reg_a ^ result ^ flags.C; flags.H = (carrybits & 0x10)>0; flags.C = (reg_a < value);  flags.Z = (result == 0); PC +=1;}, "CP A, " #x }

#define JR_X_r8(r)      { [](){ if(r) {TC += 12;  PC += (signed char)(ram[PC + 1]) + 2;} else {TC += 8; PC += 2;} }, "JP " #r ", %d" }

#define JP_X_A16(r)     { [](){ if(r) {TC += 16;  PC = ram[PC + 1] | (ram[PC + 2]<<8);} else {TC += 12; PC += 3;} }, "JP " #r ", 0x%02x%02x" }

#define CALL_X_A16(r)   { [](){ if(r) {TC+=24; int destination = ram[PC + 1] | (ram[PC + 2]<<8); PC+=3; ram[--SP] = PC>>8; ram[--SP] = PC; PC = destination;} else { TC+=12; PC+=3; }},  "CALL " #r ", 0x%02x%02x"}
#define PUSH_ss(r)      { [](){ TC+=16; ram[--SP] = r>>8; ram[--SP] = r; PC +=1; }, "PUSH " #r }
#define RET_X(r)        { [](){ if(r) {TC+=20; PC = ram[SP] | (ram[SP+1]<<8); SP+=2;} else {TC+=8; PC+=1; } }, "RET " #r }
#define POP_ss(r)       { [](){ TC+=12; r = ram[SP] | (ram[SP+1]<<8); SP+=2; PC +=1; }, "POP " #r }

#define RST_n(r)        { [](){ TC+=16; PC += 1; ram[--SP] = PC>>8; ram[--SP] = PC;  PC = r; }, "RST " #r}
#define RLC_n(r)        { [](){ TC += 8; char carry = (r & 0x80)>>7; r = (r << 1) | carry; flags.C = carry; flags.N = 0; flags.Z = !r; flags.H = 0;   PC +=2;}, "RLC " #r }
#define RRC_n(r)        { [](){ TC += 8; char carry = (r & 0x01); r = (r >> 1) | carry<<7; flags.C = carry; flags.N = 0; flags.Z = !r; flags.H = 0;   PC +=2;}, "RRC " #r }
#define RL_n(r)         { [](){ TC += 8; char carry = (r & 0x80)>>7; r = (r << 1) | flags.C; flags.C = carry; flags.N = 0; flags.Z = !r; flags.H = 0;   PC +=2;}, "RL " #r }
#define RR_n(r)         { [](){ TC += 8; char carry = (r & 0x01); r = (r >> 1) | (flags.C<<7); flags.C = carry; flags.N = 0; flags.Z = !r; flags.H = 0;   PC +=2;}, "RR " #r }
#define SLA_n(r)        { [](){ TC += 8; flags.C = (r & 0x80)>>7; r = (r << 1); flags.N = 0; flags.Z = !r; flags.H = 0;   PC +=2;}, "SLA " #r }
#define SRA_n(r)        { [](){ TC += 8; char carry = (r & 0x01); r = (r >> 1) | (r & 0x80); flags.C = carry; flags.N = 0; flags.Z = !r; flags.H = 0;   PC +=2;}, "SRA " #r }
#define SWAP(r)         { [](){ TC += 8; r = r>>4 | r<<4; flags.C = 0; flags.N = 0; flags.Z = !r; flags.H = 0;   PC +=2;}, "SWAP " #r }
#define SRL(r)          { [](){ TC += 8; char carry = (r & 0x01); r = (r >> 1); flags.C = carry; flags.N = 0; flags.Z = !r; flags.H = 0;   PC +=2;}, "SRL " #r }
#define BIT(b,r)        { [](){ TC += 8; flags.Z = !(r & (0x01 << b)); flags.N = 0; flags.H = 1; PC +=2;}, "BIT " #b ", " #r};
#define BIT_HL(b)       { [](){ TC += 8; flags.Z = !(ram[hl] & (0x01 << b)); flags.N = 0; flags.H = 1; PC +=2;}, "BIT " #b ", (hl)"};

#define SET(b,r)        { [](){ TC += 8; r |= (0x01 << b); PC +=2;}, "SET " #b ", " #r};
#define SET_HL(b)       { [](){ TC += 8; writeRam(hl, ram[hl] | (0x01 << b)); PC +=2;}, "SET " #b ", (hl)"};
#define RES(b,r)        { [](){ TC += 8; r &= 0xff^(0x01 << b); PC +=2;} , "RES " #b ", " #r};
#define RES_HL(b)       { [](){ TC += 8; writeRam(hl, ram[hl] & (0xff^(0x01 << b))); PC +=2;} , "RES " #b ", (hl)"};

void jumpToVector(int vector)
{
	ram[--SP] = (unsigned char)(PC >> 8);
	ram[--SP] = (unsigned char)PC;
	PC = vector;
}

void cpuInit()
{
	// based on no$gmb
	flags.C = 1;
	flags.H = 1;
	flags.N = 0;
	flags.Z = 1;

	af = 0x01b0;
	bc = 0x0013;
	de = 0x00d8;
	hl = 0x014D;
	SP = 0xfffe;
	PC = 0x100;


	ram[IO_REGISTER | P1] = 0xff;



	for (int i = 0; i < 256; i++)
	{
		opcode[i] = { []() {
			PC += 1;
		}, "unimplemented" };
	}
	opcode[0xf3] = { []() { /* DI */   TC += 4;  disableInterrupt(); PC += 1; }, "DI" };
	opcode[0xfB] = { []() { /* EI */   TC += 4;  delayedEnableInterrupt(); PC += 1; }, "EI" };
	opcode[0xD9] = { []() { /* RETI */ TC += 16; enableInterrupt(); PC = ram[SP] | (ram[SP + 1] << 8); SP += 2; }, "RETI" };


	opcode[0x76] = { []() { /* HALT */   TC += 4;  haltMode = true; PC += 1; }, "HALT" };

	opcode[0x00] = { []() { /* NOP */  TC += 4;  PC += 1; }, "NOP" };
	opcode[0x01] = LD_ss_n(bc);
	opcode[0x11] = LD_ss_n(de);
	opcode[0x21] = LD_ss_n(hl);
	opcode[0x31] = LD_ss_n(SP);

	opcode[0x02] = LD_ssp_r(bc, reg_a);
	opcode[0x12] = LD_ssp_r(de, reg_a);
	opcode[0x22] = LD_ssp_r(hl++, reg_a);
	opcode[0x32] = LD_ssp_r(hl--, reg_a);


	opcode[0x77] = LD_ssp_r(hl, reg_a);
	opcode[0x70] = LD_ssp_r(hl, reg_b);
	opcode[0x71] = LD_ssp_r(hl, reg_c);
	opcode[0x72] = LD_ssp_r(hl, reg_d);
	opcode[0x73] = LD_ssp_r(hl, reg_e);
	opcode[0x74] = LD_ssp_r(hl, reg_h);
	opcode[0x75] = LD_ssp_r(hl, reg_l);


	opcode[0x03] = INC_ss(bc);
	opcode[0x13] = INC_ss(de);
	opcode[0x23] = INC_ss(hl);
	opcode[0x33] = INC_ss(SP);

	opcode[0x04] = INC_r(reg_b, 4);
	opcode[0x14] = INC_r(reg_d, 4);
	opcode[0x24] = INC_r(reg_h, 4);
	opcode[0x34] = INC_r(ram[hl], 12);
	opcode[0x0C] = INC_r(reg_c, 4);
	opcode[0x1C] = INC_r(reg_e, 4);
	opcode[0x2C] = INC_r(reg_l, 4);
	opcode[0x3C] = INC_r(reg_a, 4);


	opcode[0x05] = DEC_r(reg_b, 4);
	opcode[0x15] = DEC_r(reg_d, 4);
	opcode[0x25] = DEC_r(reg_h, 4);
	opcode[0x35] = DEC_r(ram[hl], 12);

	opcode[0x0D] = DEC_r(reg_c, 4);
	opcode[0x1D] = DEC_r(reg_e, 4);
	opcode[0x2D] = DEC_r(reg_l, 4);
	opcode[0x3D] = DEC_r(reg_a, 4);

	opcode[0x06] = LD_r_n(reg_b, 8);
	opcode[0x16] = LD_r_n(reg_d, 8);
	opcode[0x26] = LD_r_n(reg_h, 8);
	opcode[0x36] = { []() { TC += 12;  writeRam(hl, ram[PC + 1]); PC += 2; }, "LD (hl), 0x%02x" };
	opcode[0x0E] = LD_r_n(reg_c, 8);
	opcode[0x1E] = LD_r_n(reg_e, 8);
	opcode[0x2E] = LD_r_n(reg_l, 8);
	opcode[0x3E] = LD_r_n(reg_a, 8);

	opcode[0x07] = { []() { /* RLCA    */ TC += 4;  char carry = (reg_a & 128) > 0; reg_a = (reg_a << 1) | carry;     flags.C = carry; flags.N = 0; flags.Z = 0; flags.H = 0;   PC += 1; } , "RLCA" };
	opcode[0x17] = { []() { /* RLA    */  TC += 4;  char carry = (reg_a & 128) > 0; reg_a = (reg_a << 1) | flags.C;   flags.C = carry; flags.N = 0; flags.Z = 0; flags.H = 0;   PC += 1; } , "RLA" };
	opcode[0x0F] = { []() { /* RRCA    */ TC += 4;  char carry = (reg_a & 1); reg_a = (reg_a >> 1) | (carry << 7);  flags.C = carry; flags.N = 0; flags.Z = 0; flags.H = 0;   PC += 1; }, "RRCA" };
	opcode[0x1F] = { []() { /* RRA    */  TC += 4;  char carry = (reg_a & 1); reg_a = (reg_a >> 1) | (flags.C << 7); flags.C = carry; flags.N = 0; flags.Z = 0; flags.H = 0;   PC += 1; }, "RRA" };

	opcode[0x08] = { []() { /* LD (A16) SP */ TC += 20; uint32_t addr = ram[PC + 1] | (ram[PC + 2] << 8); ram[addr] = SP; ram[addr + 1] = SP >> 8; PC += 3; } , "LD (0x%02x%02x) SP" };
	opcode[0x09] = ADD_ss_ss(hl, bc);
	opcode[0x19] = ADD_ss_ss(hl, de);
	opcode[0x29] = ADD_ss_ss(hl, hl);
	opcode[0x39] = ADD_ss_ss(hl, SP);

	opcode[0x0A] = LD_r_ss(reg_a, bc);
	opcode[0x1A] = LD_r_ss(reg_a, de);
	opcode[0x2A] = LD_r_ss(reg_a, hl++);
	opcode[0x3A] = LD_r_ss(reg_a, hl--);

	opcode[0x46] = LD_r_ss(reg_b, hl);
	opcode[0x56] = LD_r_ss(reg_d, hl);
	opcode[0x66] = LD_r_ss(reg_h, hl);

	opcode[0x4E] = LD_r_ss(reg_c, hl);
	opcode[0x5E] = LD_r_ss(reg_e, hl);
	opcode[0x6E] = LD_r_ss(reg_l, hl);
	opcode[0x7E] = LD_r_ss(reg_a, hl);

	opcode[0x40] = LD_r_r(reg_b, reg_b);
	opcode[0x50] = LD_r_r(reg_d, reg_b);
	opcode[0x60] = LD_r_r(reg_h, reg_b);

	opcode[0x41] = LD_r_r(reg_b, reg_c);
	opcode[0x51] = LD_r_r(reg_d, reg_c);
	opcode[0x61] = LD_r_r(reg_h, reg_c);

	opcode[0x42] = LD_r_r(reg_b, reg_d);
	opcode[0x52] = LD_r_r(reg_d, reg_d);
	opcode[0x62] = LD_r_r(reg_h, reg_d);

	opcode[0x43] = LD_r_r(reg_b, reg_e);
	opcode[0x53] = LD_r_r(reg_d, reg_e);
	opcode[0x63] = LD_r_r(reg_h, reg_e);

	opcode[0x44] = LD_r_r(reg_b, reg_h);
	opcode[0x54] = LD_r_r(reg_d, reg_h);
	opcode[0x64] = LD_r_r(reg_h, reg_h);

	opcode[0x45] = LD_r_r(reg_b, reg_l);
	opcode[0x55] = LD_r_r(reg_d, reg_l);
	opcode[0x65] = LD_r_r(reg_h, reg_l);

	opcode[0x47] = LD_r_r(reg_b, reg_a);
	opcode[0x57] = LD_r_r(reg_d, reg_a);
	opcode[0x67] = LD_r_r(reg_h, reg_a);

	opcode[0x48] = LD_r_r(reg_c, reg_b);
	opcode[0x58] = LD_r_r(reg_e, reg_b);
	opcode[0x68] = LD_r_r(reg_l, reg_b);
	opcode[0x78] = LD_r_r(reg_a, reg_b);

	opcode[0x49] = LD_r_r(reg_c, reg_c);
	opcode[0x59] = LD_r_r(reg_e, reg_c);
	opcode[0x69] = LD_r_r(reg_l, reg_c);
	opcode[0x79] = LD_r_r(reg_a, reg_c);

	opcode[0x4A] = LD_r_r(reg_c, reg_d);
	opcode[0x5A] = LD_r_r(reg_e, reg_d);
	opcode[0x6A] = LD_r_r(reg_l, reg_d);
	opcode[0x7A] = LD_r_r(reg_a, reg_d);

	opcode[0x4B] = LD_r_r(reg_c, reg_e);
	opcode[0x5B] = LD_r_r(reg_e, reg_e);
	opcode[0x6B] = LD_r_r(reg_l, reg_e);
	opcode[0x7B] = LD_r_r(reg_a, reg_e);

	opcode[0x4C] = LD_r_r(reg_c, reg_h);
	opcode[0x5C] = LD_r_r(reg_e, reg_h);
	opcode[0x6C] = LD_r_r(reg_l, reg_h);
	opcode[0x7C] = LD_r_r(reg_a, reg_h);

	opcode[0x4D] = LD_r_r(reg_c, reg_l);
	opcode[0x5D] = LD_r_r(reg_e, reg_l);
	opcode[0x6D] = LD_r_r(reg_l, reg_l);
	opcode[0x7D] = LD_r_r(reg_a, reg_l);

	opcode[0x4F] = LD_r_r(reg_c, reg_a);
	opcode[0x5F] = LD_r_r(reg_e, reg_a);
	opcode[0x6F] = LD_r_r(reg_l, reg_a);
	opcode[0x7F] = LD_r_r(reg_a, reg_a);

	opcode[0xE2] = /* LD (C) A   */{ []() { TC += 8; writeRam(0xff00 | reg_c, reg_a); PC += 1; }, " LD (C) A" };
	opcode[0xF2] = /* LD  A (C)  */{ []() { TC += 8; reg_a = ram[0xff00 | reg_c]; PC += 1; }, "LD  A (C)" };
	opcode[0xE0] = /* LDH (a8) A */{ []() { TC += 12; writeRam(0xff00 | ram[PC + 1], reg_a); PC += 2; }, "LDH (0x%02x) A" };
	opcode[0xF0] = /* LDH  A (a8)*/{ []() { TC += 12; reg_a = ram[0xff00 | ram[PC + 1]]; PC += 2; }, "LDH  A (0x%02x)" };

	opcode[0xEA] = /* LD (a16) A */{ []() { TC += 16; writeRam(ram[PC + 1] | (ram[PC + 2] << 8), reg_a); PC += 3; }, "LD (0x%02x%02x) A" };
	opcode[0xFA] = /* LD A (a16) */{ []() { TC += 16; reg_a = ram[ram[PC + 1] | (ram[PC + 2] << 8)]; PC += 3; }, "LD A (0x%02x%02x)" };

	opcode[0xF9] = /* LD SP,HL   */{ []() { TC += 8; SP = hl; PC += 1; }, "LD SP,HL" };
	opcode[0xF8] = /* LD HL,SP+r8*/{ []() { TC += 12; flags.N = 0; flags.Z = 0;
										  int x = (signed char)(ram[PC + 1]);
										  int result = SP + x;
										  flags.C = (result & 0xff) < (SP & 0xff);
										  flags.H = (result & 0x0f) < (SP & 0x0f);
										  hl = result;
										  PC += 2; }, "LD HL,SP+0x%02x" };
	opcode[0xE8] = /* ADD SP,r8*/{ []() { TC += 16; flags.N = 0; flags.Z = 0;
										  int x = (signed char)(ram[PC + 1]);
										  int result = SP + x;
										  flags.C = (result & 0xff) < (SP & 0xff);
										  flags.H = (result & 0x0f) < (SP & 0x0f);
										  SP = result;
										  PC += 2; }, "ADD SP,0x%02x" };
	opcode[0xC2] = JP_X_A16(!flags.Z);
	opcode[0xD2] = JP_X_A16(!flags.C);
	opcode[0xCA] = JP_X_A16(flags.Z);
	opcode[0xDA] = JP_X_A16(flags.C);

	opcode[0xCD] = CALL_X_A16(true);
	opcode[0xCC] = CALL_X_A16(flags.Z);
	opcode[0xDC] = CALL_X_A16(flags.C);
	opcode[0xC4] = CALL_X_A16(!flags.Z);
	opcode[0xD4] = CALL_X_A16(!flags.C);

	opcode[0xC7] = RST_n(0x00);
	opcode[0xCF] = RST_n(0x08);
	opcode[0xD7] = RST_n(0x10);
	opcode[0xDF] = RST_n(0x18);
	opcode[0xE7] = RST_n(0x20);
	opcode[0xEF] = RST_n(0x28);
	opcode[0xF7] = RST_n(0x30);
	opcode[0xFF] = RST_n(0x38);

	opcode[0xC8] = RET_X(flags.Z);
	opcode[0xD8] = RET_X(flags.C);
	opcode[0xC0] = RET_X(!flags.Z);
	opcode[0xD0] = RET_X(!flags.C);
	opcode[0xC9] = {/* RET */ []() {TC += 16; PC = ram[SP] | (ram[SP + 1] << 8); SP += 2; }, "RET" };

	opcode[0xC1] = POP_ss(bc);
	opcode[0xD1] = POP_ss(de);
	opcode[0xE1] = POP_ss(hl);
	opcode[0xF1] = POP_ss(af);

	opcode[0xC5] = PUSH_ss(bc);
	opcode[0xD5] = PUSH_ss(de);
	opcode[0xE5] = PUSH_ss(hl);
	opcode[0xF5] = PUSH_ss(af);

	opcode[0x0B] = DEC_ss(bc);
	opcode[0x1B] = DEC_ss(de);
	opcode[0x2B] = DEC_ss(hl);
	opcode[0x3B] = DEC_ss(SP);

	opcode[0x80] = ADD_r(reg_b, 4);
	opcode[0x81] = ADD_r(reg_c, 4);
	opcode[0x82] = ADD_r(reg_d, 4);
	opcode[0x83] = ADD_r(reg_e, 4);
	opcode[0x84] = ADD_r(reg_h, 4);
	opcode[0x85] = ADD_r(reg_l, 4);
	opcode[0x86] = ADD_r(ram[hl], 8);
	opcode[0x87] = ADD_r(reg_a, 4);
	opcode[0xC6] = ADD_r(ram[++PC], 8);

	opcode[0x90] = SUB_r(reg_b, 4);
	opcode[0x91] = SUB_r(reg_c, 4);
	opcode[0x92] = SUB_r(reg_d, 4);
	opcode[0x93] = SUB_r(reg_e, 4);
	opcode[0x94] = SUB_r(reg_h, 4);
	opcode[0x95] = SUB_r(reg_l, 4);
	opcode[0x96] = SUB_r(ram[hl], 8);
	opcode[0x97] = SUB_r(reg_a, 4);
	opcode[0xD6] = SUB_r(ram[++PC], 8);

	opcode[0xA0] = AND_r(reg_b, 4);
	opcode[0xA1] = AND_r(reg_c, 4);
	opcode[0xA2] = AND_r(reg_d, 4);
	opcode[0xA3] = AND_r(reg_e, 4);
	opcode[0xA4] = AND_r(reg_h, 4);
	opcode[0xA5] = AND_r(reg_l, 4);
	opcode[0xA6] = AND_r(ram[hl], 8);
	opcode[0xA7] = AND_r(reg_a, 4);
	opcode[0xE6] = AND_r(ram[++PC], 8);


	opcode[0xB0] = OR_r(reg_b, 4);
	opcode[0xB1] = OR_r(reg_c, 4);
	opcode[0xB2] = OR_r(reg_d, 4);
	opcode[0xB3] = OR_r(reg_e, 4);
	opcode[0xB4] = OR_r(reg_h, 4);
	opcode[0xB5] = OR_r(reg_l, 4);
	opcode[0xB6] = OR_r(ram[hl], 8);
	opcode[0xB7] = OR_r(reg_a, 4);
	opcode[0xF6] = OR_r(ram[++PC], 8);


	opcode[0x88] = ADC_r(reg_b, 4);
	opcode[0x89] = ADC_r(reg_c, 4);
	opcode[0x8A] = ADC_r(reg_d, 4);
	opcode[0x8B] = ADC_r(reg_e, 4);
	opcode[0x8C] = ADC_r(reg_h, 4);
	opcode[0x8D] = ADC_r(reg_l, 4);
	opcode[0x8E] = ADC_r(ram[hl], 8);
	opcode[0x8F] = ADC_r(reg_a, 4);
	opcode[0xCE] = ADC_r(ram[++PC], 8);


	opcode[0x98] = SBC_r(reg_b, 4);
	opcode[0x99] = SBC_r(reg_c, 4);
	opcode[0x9A] = SBC_r(reg_d, 4);
	opcode[0x9B] = SBC_r(reg_e, 4);
	opcode[0x9C] = SBC_r(reg_h, 4);
	opcode[0x9D] = SBC_r(reg_l, 4);
	opcode[0x9E] = SBC_r(ram[hl], 8);
	opcode[0x9F] = SBC_r(reg_a, 4);
	opcode[0xDE] = SBC_r(ram[++PC], 8);

	opcode[0xA8] = XOR_r(reg_b, 4);
	opcode[0xA9] = XOR_r(reg_c, 4);
	opcode[0xAA] = XOR_r(reg_d, 4);
	opcode[0xAB] = XOR_r(reg_e, 4);
	opcode[0xAC] = XOR_r(reg_h, 4);
	opcode[0xAD] = XOR_r(reg_l, 4);
	opcode[0xAE] = XOR_r(ram[hl], 8);
	opcode[0xAF] = XOR_r(reg_a, 4);
	opcode[0xEE] = XOR_r(ram[++PC], 8);

	opcode[0xB8] = CP_r(reg_b, 4);
	opcode[0xB9] = CP_r(reg_c, 4);
	opcode[0xBA] = CP_r(reg_d, 4);
	opcode[0xBB] = CP_r(reg_e, 4);
	opcode[0xBC] = CP_r(reg_h, 4);
	opcode[0xBD] = CP_r(reg_l, 4);
	opcode[0xBE] = CP_r(ram[hl], 8);
	opcode[0xBF] = CP_r(reg_a, 4);
	opcode[0xFE] = CP_r(ram[++PC], 8);

	opcode[0x18] = { []() { /* JR r8   */  TC += 12;  PC += (signed char)(ram[PC + 1]) + 2; }, "JR 0x%02x" };
	opcode[0x20] = JR_X_r8(!flags.Z);
	opcode[0x30] = JR_X_r8(!flags.C);
	opcode[0x28] = JR_X_r8(flags.Z);
	opcode[0x38] = JR_X_r8(flags.C);
	opcode[0x2F] = { []() { /* CPL */ TC += 4; reg_a ^= 0xFF; flags.N = 1; flags.H = 1; PC += 1; }, "CPL" };
	opcode[0x3F] = { []() { /* CCF */ TC += 4; flags.C ^= 1; flags.N = 0; flags.H = 0; PC += 1; }, "CCF" };
	opcode[0x37] = { []() { /* SCF */ TC += 4; flags.C = 1; flags.N = 0; flags.H = 0; PC += 1; }, "SCF" };
	opcode[0x27] = { []() { /* DAA */
		TC += 4;

		if (!flags.N)
		{
			if (flags.C || (reg_a > 0x99)) { reg_a += 0x60; flags.C = 1; }
			if (flags.H || ((reg_a & 0x0F) > 0x09)) { reg_a += 0x06; }

		}
		else
		{
			if (flags.C) { reg_a -= 0x60; }
			if (flags.H) { reg_a -= 0x06; }
		}

		flags.H = 0;
		flags.Z = reg_a == 0;
		PC += 1;
	}, "DAA" };
	opcode[0xE9] = { []() { /* JP (HL)  */ TC += 4;   PC = hl; }, "JP (HL)" };
	opcode[0xC3] = { []() { /* JUMP A16 */ TC += 16;  PC = ram[PC + 1] | (ram[PC + 2] << 8); }, "JUMP 0x%02x%02x" };
	opcode[0xCB] = { []() { unsigned int op = ram[PC + 1]; CBopcode[op].funct(); if ((op & 0x07) == 6) { TC += 8; } }, "prefixe CB" };

	CBopcode[0x00] = RLC_n(reg_b);
	CBopcode[0x01] = RLC_n(reg_c);
	CBopcode[0x02] = RLC_n(reg_d);
	CBopcode[0x03] = RLC_n(reg_e);
	CBopcode[0x04] = RLC_n(reg_h);
	CBopcode[0x05] = RLC_n(reg_l);
	CBopcode[0x06] = RLC_n(ram[hl]);
	CBopcode[0x07] = RLC_n(reg_a);

	CBopcode[0x08] = RRC_n(reg_b);
	CBopcode[0x09] = RRC_n(reg_c);
	CBopcode[0x0A] = RRC_n(reg_d);
	CBopcode[0x0B] = RRC_n(reg_e);
	CBopcode[0x0C] = RRC_n(reg_h);
	CBopcode[0x0D] = RRC_n(reg_l);
	CBopcode[0x0E] = RRC_n(ram[hl]);
	CBopcode[0x0F] = RRC_n(reg_a);

	CBopcode[0x10] = RL_n(reg_b);
	CBopcode[0x11] = RL_n(reg_c);
	CBopcode[0x12] = RL_n(reg_d);
	CBopcode[0x13] = RL_n(reg_e);
	CBopcode[0x14] = RL_n(reg_h);
	CBopcode[0x15] = RL_n(reg_l);
	CBopcode[0x16] = RL_n(ram[hl]);
	CBopcode[0x17] = RL_n(reg_a);

	CBopcode[0x18] = RR_n(reg_b);
	CBopcode[0x19] = RR_n(reg_c);
	CBopcode[0x1A] = RR_n(reg_d);
	CBopcode[0x1B] = RR_n(reg_e);
	CBopcode[0x1C] = RR_n(reg_h);
	CBopcode[0x1D] = RR_n(reg_l);
	CBopcode[0x1E] = RR_n(ram[hl]);
	CBopcode[0x1F] = RR_n(reg_a);

	CBopcode[0x20] = SLA_n(reg_b);
	CBopcode[0x21] = SLA_n(reg_c);
	CBopcode[0x22] = SLA_n(reg_d);
	CBopcode[0x23] = SLA_n(reg_e);
	CBopcode[0x24] = SLA_n(reg_h);
	CBopcode[0x25] = SLA_n(reg_l);
	CBopcode[0x26] = SLA_n(ram[hl]);
	CBopcode[0x27] = SLA_n(reg_a);

	CBopcode[0x28] = SRA_n(reg_b);
	CBopcode[0x29] = SRA_n(reg_c);
	CBopcode[0x2A] = SRA_n(reg_d);
	CBopcode[0x2B] = SRA_n(reg_e);
	CBopcode[0x2C] = SRA_n(reg_h);
	CBopcode[0x2D] = SRA_n(reg_l);
	CBopcode[0x2E] = SRA_n(ram[hl]);
	CBopcode[0x2F] = SRA_n(reg_a);

	CBopcode[0x30] = SWAP(reg_b);
	CBopcode[0x31] = SWAP(reg_c);
	CBopcode[0x32] = SWAP(reg_d);
	CBopcode[0x33] = SWAP(reg_e);
	CBopcode[0x34] = SWAP(reg_h);
	CBopcode[0x35] = SWAP(reg_l);
	CBopcode[0x36] = SWAP(ram[hl]);
	CBopcode[0x37] = SWAP(reg_a);

	CBopcode[0x38] = SRL(reg_b);
	CBopcode[0x39] = SRL(reg_c);
	CBopcode[0x3A] = SRL(reg_d);
	CBopcode[0x3B] = SRL(reg_e);
	CBopcode[0x3C] = SRL(reg_h);
	CBopcode[0x3D] = SRL(reg_l);
	CBopcode[0x3E] = SRL(ram[hl]);
	CBopcode[0x3F] = SRL(reg_a);

	CBopcode[0x40] = BIT(0, reg_b);
	CBopcode[0x41] = BIT(0, reg_c);
	CBopcode[0x42] = BIT(0, reg_d);
	CBopcode[0x43] = BIT(0, reg_e);
	CBopcode[0x44] = BIT(0, reg_h);
	CBopcode[0x45] = BIT(0, reg_l);
	CBopcode[0x46] = BIT_HL(0);
	CBopcode[0x47] = BIT(0, reg_a);

	CBopcode[0x48] = BIT(1, reg_b);
	CBopcode[0x49] = BIT(1, reg_c);
	CBopcode[0x4A] = BIT(1, reg_d);
	CBopcode[0x4B] = BIT(1, reg_e);
	CBopcode[0x4C] = BIT(1, reg_h);
	CBopcode[0x4D] = BIT(1, reg_l);
	CBopcode[0x4E] = BIT_HL(1);
	CBopcode[0x4F] = BIT(1, reg_a);

	CBopcode[0x50] = BIT(2, reg_b);
	CBopcode[0x51] = BIT(2, reg_c);
	CBopcode[0x52] = BIT(2, reg_d);
	CBopcode[0x53] = BIT(2, reg_e);
	CBopcode[0x54] = BIT(2, reg_h);
	CBopcode[0x55] = BIT(2, reg_l);
	CBopcode[0x56] = BIT_HL(2);
	CBopcode[0x57] = BIT(2, reg_a);

	CBopcode[0x58] = BIT(3, reg_b);
	CBopcode[0x59] = BIT(3, reg_c);
	CBopcode[0x5A] = BIT(3, reg_d);
	CBopcode[0x5B] = BIT(3, reg_e);
	CBopcode[0x5C] = BIT(3, reg_h);
	CBopcode[0x5D] = BIT(3, reg_l);
	CBopcode[0x5E] = BIT_HL(3);
	CBopcode[0x5F] = BIT(3, reg_a);

	CBopcode[0x60] = BIT(4, reg_b);
	CBopcode[0x61] = BIT(4, reg_c);
	CBopcode[0x62] = BIT(4, reg_d);
	CBopcode[0x63] = BIT(4, reg_e);
	CBopcode[0x64] = BIT(4, reg_h);
	CBopcode[0x65] = BIT(4, reg_l);
	CBopcode[0x66] = BIT_HL(4);
	CBopcode[0x67] = BIT(4, reg_a);

	CBopcode[0x68] = BIT(5, reg_b);
	CBopcode[0x69] = BIT(5, reg_c);
	CBopcode[0x6A] = BIT(5, reg_d);
	CBopcode[0x6B] = BIT(5, reg_e);
	CBopcode[0x6C] = BIT(5, reg_h);
	CBopcode[0x6D] = BIT(5, reg_l);
	CBopcode[0x6E] = BIT_HL(5);
	CBopcode[0x6F] = BIT(5, reg_a);

	CBopcode[0x70] = BIT(6, reg_b);
	CBopcode[0x71] = BIT(6, reg_c);
	CBopcode[0x72] = BIT(6, reg_d);
	CBopcode[0x73] = BIT(6, reg_e);
	CBopcode[0x74] = BIT(6, reg_h);
	CBopcode[0x75] = BIT(6, reg_l);
	CBopcode[0x76] = BIT_HL(6);
	CBopcode[0x77] = BIT(6, reg_a);

	CBopcode[0x78] = BIT(7, reg_b);
	CBopcode[0x79] = BIT(7, reg_c);
	CBopcode[0x7A] = BIT(7, reg_d);
	CBopcode[0x7B] = BIT(7, reg_e);
	CBopcode[0x7C] = BIT(7, reg_h);
	CBopcode[0x7D] = BIT(7, reg_l);
	CBopcode[0x7E] = BIT_HL(7);
	CBopcode[0x7F] = BIT(7, reg_a);

	CBopcode[0x80] = RES(0, reg_b);
	CBopcode[0x81] = RES(0, reg_c);
	CBopcode[0x82] = RES(0, reg_d);
	CBopcode[0x83] = RES(0, reg_e);
	CBopcode[0x84] = RES(0, reg_h);
	CBopcode[0x85] = RES(0, reg_l);
	CBopcode[0x86] = RES_HL(0);
	CBopcode[0x87] = RES(0, reg_a);

	CBopcode[0x88] = RES(1, reg_b);
	CBopcode[0x89] = RES(1, reg_c);
	CBopcode[0x8A] = RES(1, reg_d);
	CBopcode[0x8B] = RES(1, reg_e);
	CBopcode[0x8C] = RES(1, reg_h);
	CBopcode[0x8D] = RES(1, reg_l);
	CBopcode[0x8E] = RES_HL(1);
	CBopcode[0x8F] = RES(1, reg_a);

	CBopcode[0x90] = RES(2, reg_b);
	CBopcode[0x91] = RES(2, reg_c);
	CBopcode[0x92] = RES(2, reg_d);
	CBopcode[0x93] = RES(2, reg_e);
	CBopcode[0x94] = RES(2, reg_h);
	CBopcode[0x95] = RES(2, reg_l);
	CBopcode[0x96] = RES_HL(2);
	CBopcode[0x97] = RES(2, reg_a);

	CBopcode[0x98] = RES(3, reg_b);
	CBopcode[0x99] = RES(3, reg_c);
	CBopcode[0x9A] = RES(3, reg_d);
	CBopcode[0x9B] = RES(3, reg_e);
	CBopcode[0x9C] = RES(3, reg_h);
	CBopcode[0x9D] = RES(3, reg_l);
	CBopcode[0x9E] = RES_HL(3);
	CBopcode[0x9F] = RES(3, reg_a);

	CBopcode[0xA0] = RES(4, reg_b);
	CBopcode[0xA1] = RES(4, reg_c);
	CBopcode[0xA2] = RES(4, reg_d);
	CBopcode[0xA3] = RES(4, reg_e);
	CBopcode[0xA4] = RES(4, reg_h);
	CBopcode[0xA5] = RES(4, reg_l);
	CBopcode[0xA6] = RES_HL(4);
	CBopcode[0xA7] = RES(4, reg_a);

	CBopcode[0xA8] = RES(5, reg_b);
	CBopcode[0xA9] = RES(5, reg_c);
	CBopcode[0xAA] = RES(5, reg_d);
	CBopcode[0xAB] = RES(5, reg_e);
	CBopcode[0xAC] = RES(5, reg_h);
	CBopcode[0xAD] = RES(5, reg_l);
	CBopcode[0xAE] = RES_HL(5);
	CBopcode[0xAF] = RES(5, reg_a);

	CBopcode[0xB0] = RES(6, reg_b);
	CBopcode[0xB1] = RES(6, reg_c);
	CBopcode[0xB2] = RES(6, reg_d);
	CBopcode[0xB3] = RES(6, reg_e);
	CBopcode[0xB4] = RES(6, reg_h);
	CBopcode[0xB5] = RES(6, reg_l);
	CBopcode[0xB6] = RES_HL(6);
	CBopcode[0xB7] = RES(6, reg_a);

	CBopcode[0xB8] = RES(7, reg_b);
	CBopcode[0xB9] = RES(7, reg_c);
	CBopcode[0xBA] = RES(7, reg_d);
	CBopcode[0xBB] = RES(7, reg_e);
	CBopcode[0xBC] = RES(7, reg_h);
	CBopcode[0xBD] = RES(7, reg_l);
	CBopcode[0xBE] = RES_HL(7);
	CBopcode[0xBF] = RES(7, reg_a);

	CBopcode[0xC0] = SET(0, reg_b);
	CBopcode[0xC1] = SET(0, reg_c);
	CBopcode[0xC2] = SET(0, reg_d);
	CBopcode[0xC3] = SET(0, reg_e);
	CBopcode[0xC4] = SET(0, reg_h);
	CBopcode[0xC5] = SET(0, reg_l);
	CBopcode[0xC6] = SET_HL(0);
	CBopcode[0xC7] = SET(0, reg_a);

	CBopcode[0xC8] = SET(1, reg_b);
	CBopcode[0xC9] = SET(1, reg_c);
	CBopcode[0xCA] = SET(1, reg_d);
	CBopcode[0xCB] = SET(1, reg_e);
	CBopcode[0xCC] = SET(1, reg_h);
	CBopcode[0xCD] = SET(1, reg_l);
	CBopcode[0xCE] = SET_HL(1);
	CBopcode[0xCF] = SET(1, reg_a);

	CBopcode[0xD0] = SET(2, reg_b);
	CBopcode[0xD1] = SET(2, reg_c);
	CBopcode[0xD2] = SET(2, reg_d);
	CBopcode[0xD3] = SET(2, reg_e);
	CBopcode[0xD4] = SET(2, reg_h);
	CBopcode[0xD5] = SET(2, reg_l);
	CBopcode[0xD6] = SET_HL(2);
	CBopcode[0xD7] = SET(2, reg_a);

	CBopcode[0xD8] = SET(3, reg_b);
	CBopcode[0xD9] = SET(3, reg_c);
	CBopcode[0xDA] = SET(3, reg_d);
	CBopcode[0xDB] = SET(3, reg_e);
	CBopcode[0xDC] = SET(3, reg_h);
	CBopcode[0xDD] = SET(3, reg_l);
	CBopcode[0xDE] = SET_HL(3);
	CBopcode[0xDF] = SET(3, reg_a);

	CBopcode[0xE0] = SET(4, reg_b);
	CBopcode[0xE1] = SET(4, reg_c);
	CBopcode[0xE2] = SET(4, reg_d);
	CBopcode[0xE3] = SET(4, reg_e);
	CBopcode[0xE4] = SET(4, reg_h);
	CBopcode[0xE5] = SET(4, reg_l);
	CBopcode[0xE6] = SET_HL(4);
	CBopcode[0xE7] = SET(4, reg_a);

	CBopcode[0xE8] = SET(5, reg_b);
	CBopcode[0xE9] = SET(5, reg_c);
	CBopcode[0xEA] = SET(5, reg_d);
	CBopcode[0xEB] = SET(5, reg_e);
	CBopcode[0xEC] = SET(5, reg_h);
	CBopcode[0xED] = SET(5, reg_l);
	CBopcode[0xEE] = SET_HL(5);
	CBopcode[0xEF] = SET(5, reg_a);

	CBopcode[0xF0] = SET(6, reg_b);
	CBopcode[0xF1] = SET(6, reg_c);
	CBopcode[0xF2] = SET(6, reg_d);
	CBopcode[0xF3] = SET(6, reg_e);
	CBopcode[0xF4] = SET(6, reg_h);
	CBopcode[0xF5] = SET(6, reg_l);
	CBopcode[0xF6] = SET_HL(6);
	CBopcode[0xF7] = SET(6, reg_a);

	CBopcode[0xF8] = SET(7, reg_b);
	CBopcode[0xF9] = SET(7, reg_c);
	CBopcode[0xFA] = SET(7, reg_d);
	CBopcode[0xFB] = SET(7, reg_e);
	CBopcode[0xFC] = SET(7, reg_h);
	CBopcode[0xFD] = SET(7, reg_l);
	CBopcode[0xFE] = SET_HL(7);
	CBopcode[0xFF] = SET(7, reg_a);
}

void cpuUpdate()
{

	flags.dummy0 = 0; flags.dummy1 = 0;	flags.dummy2 = 0; flags.dummy3 = 0;
}

void cpuImgui()
{
	{
		unsigned char instruction = ram[PC];
		char instructionLabel[128];
		int nbPercent = 0;
		const char * pos = opcode[instruction].opName;
		while (pos = strchr(pos, '%'))
		{
			nbPercent++;
			pos++;
		}
		if (nbPercent == 2)		sprintf(instructionLabel, opcode[instruction].opName, ram[PC + 2], ram[PC + 1]);
		else					sprintf(instructionLabel, opcode[instruction].opName, (signed char)ram[PC + 1]);

		ImGui::Text("Current instruction : 0x%04x %s", PC, instructionLabel);
	}
	ImGui::Text("Current rom data : 0x%x 0x%x 0x%x", ram[PC], ram[PC + 1], ram[PC + 2]);

	ImGui::Text("SP:0x%04x", SP);
	ImGui::Text("PC:0x%04x", PC);
	ImGui::Text("AF:0x%04x", af);
	ImGui::Text("BC:0x%04x", bc);
	ImGui::Text("DE:0x%04x", de);
	ImGui::Text("HL:0x%04x", hl);
	ImGui::Checkbox("halt", &haltMode);
	ImGui::NewLine();
	ImGui::Text("IF:0x%02x", ram[IO_REGISTER | IF]);
	ImGui::Text("IE:0x%02x", ram[IO_REGISTER | IE]);
	ImGui::PushItemWidth(60);

	imguiRegister(ram[PC]); ImGui::SameLine(); imguiRegister(ram[PC + 1]); ImGui::SameLine(); imguiRegister(ram[PC + 2]);

	ImGui::PopItemWidth();
	ImGui::PopItemWidth();

	ImGui::Text("Flags Z:%x C:%x H:%x N:%x", flags.Z ? 1 : 0, flags.C ? 1 : 0, flags.H ? 1 : 0, flags.N ? 1 : 0);
	if (ImGui::Button("Z"))
	{
		flags.Z ^= 1;
	} ImGui::SameLine();
	if (ImGui::Button("C"))
	{
		flags.C ^= 1;
	}
	ImGui::SameLine();
	if (ImGui::Button("H"))
	{
		flags.H ^= 1;
	}
	ImGui::SameLine();
	if (ImGui::Button("N"))
	{
		flags.N ^= 1;
	}

	if (ImGui::CollapsingHeader("Stack"))
	{
		ImGui::Text(" 0 %02x %04x", ram[SP], SP);
		ImGui::Text(" 1 %02x %04x", ram[SP + 1], SP + 1);
		ImGui::Text(" 2 %02x %04x", ram[SP + 2], SP + 2);
		ImGui::Text(" 3 %02x %04x", ram[SP + 3], SP + 3);
		ImGui::Text(" 4 %02x %04x", ram[SP + 4], SP + 4);
	}

	if (ImGui::CollapsingHeader("test instruction"))
	{
		static int opcodeNumber = 0;
		ImGui::InputInt("oppcode", &opcodeNumber, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
		if (ImGui::Button("test"))
		{
			opcode[opcodeNumber].funct();
		}
	}
	if (ImGui::CollapsingHeader("test register"))
	{
		imguiRegister(reg_a);
		imguiRegister(reg_b);
		imguiRegister(reg_c);
		imguiRegister(reg_d);
		imguiRegister(reg_e);
		imguiRegister(reg_f);
		imguiRegister(reg_h);
		imguiRegister(reg_l);
		imguiRegister(SP);
		imguiRegister(PC);
		imguiRegister(af);
		imguiRegister(bc);
		imguiRegister(de);
		imguiRegister(hl);

		if (ImGui::Button("set A 5"))
		{
			reg_a = 5;
		}
		if (ImGui::Button("set F 7"))
		{
			reg_f = 7;
		}
		if (ImGui::Button("set AF 2"))
		{
			af = 2;
		}

		if (ImGui::Button("set AF 1ff"))
		{
			af = 0x1ff;
		}
	}
}