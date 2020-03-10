#pragma once
#include <functional>

struct opcodeStruct
{
	std::function<void(void)> funct;
	const char * opName;
};

extern opcodeStruct opcode[256];

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
	unsigned short bc, de, hl;
	bool haltMode = false;
	unsigned short PC;
	unsigned short SP;
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
	void wakeHalteMode();
};
#define ram cpu->RAM
extern unsigned char TC;
extern CPU * cpu;