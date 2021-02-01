#pragma once
#include <functional>

	
struct opcodeStruct
{
	void (*funct)();
	const char * opName;
};
extern	opcodeStruct opcode[256];

struct CPU
{
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
	unsigned char RAM[0x10000];
	unsigned char externalRAM[0x20000];
	unsigned short bc, de, hl;
	bool haltMode = false;
	unsigned short pc;
	unsigned short sp;
	struct {
		bool mode = 0;
		unsigned char upper = 0;
		unsigned char bank = 0;
		bool ramWriteEnable = false;
		unsigned char ramBank = 0;
		bool ramModified = false;
		struct {
			unsigned short frame = 0;
			unsigned short second = 0;
			unsigned short minute = 0;
			unsigned short hour = 0;
			unsigned short day = 0;
			unsigned char flag = 0;
		}rtc;
	}mapper;
	bool InterruptMasterFlag = true;
	int shouldRiseInterruptMasterFlag = 0;
	void enableInterrupt();
	void delayedEnableInterrupt();
	bool handleInterrupts();
	bool handleInterrupt(int flag, int vector);
	void trigInterrupt(int interrupt);
	void disableInterrupt();
public:
	void jumpToVector(int vector);
	void init();
	void imgui();
	void update();
	void updateRTC();
	void wakeHalteMode();

};

#define ram cpu->RAM
#define PC cpu->pc
#define SP cpu->sp
#define BC cpu->bc
#define DE cpu->de
#define HL cpu->hl
extern CPU * cpu;
extern unsigned char TC;