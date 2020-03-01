#include "glouboy.h"
#include "io.h"

#define tac ram[IO_REGISTER | TAC]
#define tima ram[IO_REGISTER | TIMA]
#define tma ram[IO_REGISTER | TMA]
#define div ram[IO_REGISTER | DIV]

static int oldTC = 0;
unsigned short internalDIV = 0xABCC;
unsigned short nextUpdate = 0xABCC;
void timerUpdateIncrementTreshold()
{
	if ((tac & 0x03) == 0)
		nextUpdate = internalDIV + 4096;
	if ((tac & 0x03) == 1)
		nextUpdate = internalDIV + 16;
	if ((tac & 0x03) == 2)
		nextUpdate = internalDIV + 64;
	if ((tac & 0x03) == 3)
		nextUpdate = internalDIV + 256;
}

int timerDivWrite()
{
	oldTC = TC;
	internalDIV = 0;
	timerUpdateIncrementTreshold();
	return 0;
}

void timerUpdate()
{
	int diffTC = TC - oldTC;

	int newValue = tima;
	if (tac & 0x04)
	{
		for (int i = 0; i < diffTC; i++)
		{
			internalDIV ++;
			if (nextUpdate == internalDIV)
			{
				newValue++;
				timerUpdateIncrementTreshold();
			}
		}
		div = internalDIV >> 8;
	}
	
	if (newValue > 0xff)
	{
		tima = tma;
		trigInterrupt(IRQ_TIMER);
	}
	else
	{
		tima = newValue;

	}
	oldTC = TC;
}