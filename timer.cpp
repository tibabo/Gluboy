#include "glouboy.h"
#include "io.h"

#define tac ram[IO_REGISTER | TAC]
#define tima ram[IO_REGISTER | TIMA]
#define tma ram[IO_REGISTER | TMA]
#define div ram[IO_REGISTER | DIV]

unsigned short internalDIV = 0xABCC;
unsigned short nextUpdate = 0xABCC;

int accu = 0;

int timerDivWrite()
{
	internalDIV = 0;
	return 0;
}

void timerUpdate()
{
	internalDIV += TC;
	div = internalDIV >> 8;

	int tempTima = tima;

	if (tac & 0x04)
	{
		accu += TC;
	
		if (((tac & 0x03) == 0) && (accu >= 4096))
		{
			accu -= 4096;
			tempTima++;
		}
		if (((tac & 0x03) == 1) && (accu >= 16))
		{
			accu -= 16;
			tempTima++;
		}
		if (((tac & 0x03) == 2) && (accu >= 64))
		{
			accu -= 64;
			tempTima++;
		}
		if (((tac & 0x03) == 3) && (accu >= 256))
		{
			accu -= 256;
			tempTima++;
		}
	}
	
	if (tempTima > 0xff)
	{
		tima = tma;
		trigInterrupt(IRQ_TIMER);
	}
	else
	{
		tima = tempTima;
	}
}