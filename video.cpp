#include "imgui.h"

#include "glouboy.h"
#include "io.h"

#if defined(_MSC_VER) && _MSC_VER <= 1500 // MSVC 2008 or earlier
#include <stddef.h>     // intptr_t
#else
#include <stdint.h>     // intptr_t
#endif

// Include OpenGL header (without an OpenGL loader) requires a bit of fiddling
#if defined(_WIN32) && !defined(APIENTRY)
#define APIENTRY __stdcall                  // It is customary to use APIENTRY for OpenGL function pointer declarations on all platforms.  Additionally, the Windows OpenGL header needs APIENTRY.
#endif
#if defined(_WIN32) && !defined(WINGDIAPI)
#define WINGDIAPI __declspec(dllimport)     // Some Windows OpenGL headers need this
#endif
#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#define FULL_LINE_CYCLE 456
#define VISIBLE_LINE 144
#define TILE_SIZE_IN_BYTES 16
#define NB_TILE (32 * 32)
int background[256 * 256];
static GLuint       g_backgroundTexture = 0;
int lastTC = 0;
int totalFrameCycle = 0;
int currentLineCycle = 0;
int stateMode = 0;

int defaultPalette[4] = { 0xffd3f6ff,0xff75a8f9,0xff6f6beb,0xff583f7c};
int currentPalette[4];

#define reg_stat ram[IO_REGISTER|STAT]

void videoImguiWindow()
{
	bool Open = true;
	if (ImGui::Begin("Video", &Open, ImGuiWindowFlags_NoScrollbar))
	{
		ImGui::Image((ImTextureID)(intptr_t)g_backgroundTexture, ImVec2(256, 256));

	}
	ImGui::End();
}	

void videoReset()
{
	//reset palette
	for (int i = 0; i < 4; i++)
	{
		currentPalette[i] = defaultPalette[i];
	}

	ram[IO_REGISTER | LCDC] = 0x91;
	ram[IO_REGISTER | STAT] = 0x85;
	ram[IO_REGISTER | SCY]	= 0x00;
	ram[IO_REGISTER | SCX]  = 0x00;
	ram[IO_REGISTER | LY]   = 0x00;
	ram[IO_REGISTER | LYC]  = 0x00;
	ram[IO_REGISTER | DMA]  = 0x00;
	ram[IO_REGISTER | BGP]  = 0xFF;
	ram[IO_REGISTER | OBP0] = 0xFF;
	ram[IO_REGISTER | OBP1] = 0xFF;
	ram[IO_REGISTER | WY] = 0x00;
	ram[IO_REGISTER | WX] = 0x00;
}


void videoCreateBackgroundTexture()
{
	int width = 256, height = 256;
	for (int i = 0; i < height * width; i++) background[i] = defaultPalette[0];

	// Upload texture to graphics system
	GLint last_texture;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	glGenTextures(1, &g_backgroundTexture);
	glBindTexture(GL_TEXTURE_2D, g_backgroundTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, background);

	// Restore state
	glBindTexture(GL_TEXTURE_2D, last_texture);
}


void videoCreateTextures()
{
	videoCreateBackgroundTexture();
}

void videoUpdateBackgoundTexture()
{
	int tileMapAddr =  (ram[IO_REGISTER | LCDC] & 0x08) ? 0x9c00 : 0x9800; // BG Tile Map Display Select 
	int tileDataAddr = 0x8000;// (ram[IO_REGISTER | LCDC] & 0x10) ? 0x8000 : 0x9000; // BG & Window Tile Data Select
	for (int i = 0; i < NB_TILE; i++)
	{
		int tileIndex = 0;
		//if(ram[IO_REGISTER | LCDC] & 0x10)
			tileIndex = ram[tileMapAddr + i];
		//else
		//	tileIndex = (signed char)(ram[tileMapAddr + i]);

		int VRAMBegin = tileDataAddr + tileIndex * TILE_SIZE_IN_BYTES;
		for (int y = 0; y < 8; y++)
		{
			int ramIndex = VRAMBegin + y * 2;
			unsigned char tileData1 = ram[ramIndex];
			unsigned char tileData2 = ram[ramIndex + 1];
			for (int x = 0; x < 8; x++)
			{
				int colorIndex = (((tileData1 >> x)&1) | (((tileData2 >> x)&1) <<1))  & 0x03;

				background[(7 - x) + y * 256 + i%32 * 8 + i / 32 * 8 * 256] = currentPalette[colorIndex];
			}
		}
	}
	
	GLint last_texture;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	glBindTexture(GL_TEXTURE_2D, g_backgroundTexture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA,	GL_UNSIGNED_BYTE, background);
	glBindTexture(GL_TEXTURE_2D, last_texture);
}

void videoUpdate()
{
	if ((ram[IO_REGISTER | LCDC] & 0x80) == 0) // LCD POWER
	{
		//LCD is off
		ram[IO_REGISTER | LY] = 0;
		reg_stat &= ~0x03; // clear stateMode
		reg_stat |= 0x80;  // bit 7 always 1
		stateMode = 0;
		totalFrameCycle = 0;
		currentLineCycle = 0;
		return;
	}
	
	int	timeDiff = TC - lastTC;
	lastTC = TC;
	int newtotalFrameCycle = (totalFrameCycle + timeDiff) % (154*FULL_LINE_CYCLE);
	if (newtotalFrameCycle > (VISIBLE_LINE * FULL_LINE_CYCLE + 4))
	{
		if (totalFrameCycle < (VISIBLE_LINE * FULL_LINE_CYCLE + 4))
		{
			// trigger VBL interrupt
			trigInterrupt(IRQ_V_Blank);
		}
	}
	totalFrameCycle = newtotalFrameCycle;


	int oldLY = ram[IO_REGISTER | LY];
	int newLY = totalFrameCycle / FULL_LINE_CYCLE;
	ram[IO_REGISTER | LY] = newLY;

	if ((oldLY < ram[IO_REGISTER | LYC]) &&
		(newLY >= ram[IO_REGISTER | LYC]))
	{
		if (reg_stat & (1 << 6)) // LY=LYC Check Enable 
		{
			trigInterrupt(IRQ_LCDC);
		}
	}
	
	if(newLY == ram[IO_REGISTER | LYC])
		reg_stat |= (1 << 2); // LY=LYC Comparison Signal 
	else
		reg_stat &= ~(1 << 2); // LY=LYC Comparison Signal 


	currentLineCycle += timeDiff;
	if (currentLineCycle >= FULL_LINE_CYCLE)
	{
		currentLineCycle -= FULL_LINE_CYCLE;
	}
	int newScreenMode = 0;
	if (newLY < 144)
	{
		newScreenMode = 0;
		if (currentLineCycle > 4)   newScreenMode = 2;
		if (currentLineCycle > 80)  newScreenMode = 3;
		if (currentLineCycle > 150) newScreenMode = 0;
	}
	else if (newLY == 144)
	{
		newScreenMode = 0;
		if (currentLineCycle > 4)   newScreenMode = 1;

	}
	else // (newLY <= 153)
	{
		newScreenMode = 1;
	}

	bool ScreenModeChanged = stateMode != newScreenMode;
	stateMode = newScreenMode;
	if(ScreenModeChanged)
	{
		if (stateMode == 1)
			videoUpdateBackgoundTexture(); // temp hack

		if (((stateMode == 0) && (reg_stat & (1 << 3))) || // H-BL
			((stateMode == 2) && (reg_stat & (1 << 5))) || //OAM
			((stateMode == 1) && ((reg_stat & (1 << 4)) || (reg_stat & (1 << 5)))))		{			trigInterrupt(IRQ_LCDC);
		}
	}

	// will handle ligne 153 special LY timing later
}


void videoWrite(int registerAddr, int value)
{
	if (registerAddr == LCDC)
	{
		if (value & 0x80) // LCD POWER
		{
			ram[LY] = 0;
			lastTC = TC;
		}
	}
	if (registerAddr == BGP) // Background Palette
	{
		int index = value & 0x03;
		currentPalette[0] = defaultPalette[index];
		index = (value>>2) & 0x03;
		currentPalette[1] = defaultPalette[index];
		index = (value>>4) & 0x03;
		currentPalette[2] = defaultPalette[index];
		index = (value>>6) & 0x03;
		currentPalette[3] = defaultPalette[index];
	}
}