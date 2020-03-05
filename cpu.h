#pragma once
#include <functional>
void jumpToVector(int vector);
void cpuInit();
void cpuImgui();
void cpuUpdate();

struct opcodeStruct
{
	std::function<void(void)> funct;
	const char * opName;
};

extern opcodeStruct opcode[256];

extern unsigned char ram[0x10000];
extern unsigned char * rom;
extern unsigned char TC;
extern unsigned short PC;
extern bool haltMode;
