#pragma once

#define IRQ_V_Blank   (1<<0)
#define IRQ_LCDC      (1<<1)
#define IRQ_TIMER     (1<<2)
#define IRQ_SERIAL    (1<<3)
#define IRQ_JOYPAD    (1<<4)

#define IO_REGISTER      0xff00

#define P1				 0x00
#define SB				 0x01
#define SC				 0x02
#define DIV				 0x04
#define TIMA			 0x05
#define TMA				 0x06
#define TAC				 0x07
#define IF				 0x0F
#define NR10			 0x10
#define NR11			 0x11
#define NR12			 0x12
#define NR13			 0x13
#define NR14			 0x14

#define NR21			 0x16
#define NR22			 0x17
#define NR23			 0x18
#define NR24			 0x19

#define NR30			 0x1A
#define NR31			 0x1B
#define NR32			 0x1C
#define NR33			 0x1D
#define NR34			 0x1E

#define NR41			 0x20
#define NR42			 0x21
#define NR43			 0x22
#define NR44			 0x23

#define NR50			 0x25
#define NR51			 0x26
#define NR52			 0x27

#define Wave_Pattern_RAM 0x30

#define LCDC             0x40
#define STAT             0x41
#define SCY              0x42
#define SCX              0x43
#define LY               0x44
#define LYC              0x45
#define DMA              0x46
#define BGP	             0x47
#define OBP0             0x48
#define OBP1             0x49
#define WY               0x4A
#define WX               0x4B
#define IE               0xff

void handleInterrupts();
void writeIO(int registerAddr, int value);
void trigInterrupt(int interrupt);
void disableInterrupt();
void enableInterrupt();
void delayedEnableInterrupt();
void writeRam(int adresse, int value);
