#include "imgui.h"
#include "video.h"
#include "glouboy.h"
#include "io.h"
#include "cpu.h"

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

struct oamBlock
{
	unsigned char y;
	unsigned char x;
	unsigned char pattern;
	struct
	{
		unsigned char dummy0 : 1;
		unsigned char dummy1 : 1;
		unsigned char dummy2 : 1;
		unsigned char dummy3 : 1;
		unsigned char palette : 1;
		unsigned char xFlip : 1;
		unsigned char yFlip : 1;
		unsigned char priority : 1;
	} flags;
};


int background[256 * 256];


GLuint g_backgroundTexture = 0;
GLuint screenTexture = 0;

unsigned int defaultPalette[4] = { 0xffd3f6ff,0xff75a8f9,0xff6f6beb,0xff583f7c};



#define reg_stat ram[IO_REGISTER | 0x41]

void Video::imgui()
{
	bool Open = true;
	if (ImGui::Begin("Video", &Open, ImGuiWindowFlags_NoScrollbar))
	{
		ImGui::Image((ImTextureID)(intptr_t)screenTexture, ImVec2(SCREEN_W*3, SCREEN_H*3));
		static bool show = false;
		ImGui::Checkbox("show full framebuffer", &show);
		if(show) ImGui::Image((ImTextureID)(intptr_t)g_backgroundTexture, ImVec2(256, 256));

	}
	ImGui::End();
}	

void Video::reset()
{
	//reset palette
	for (int i = 0; i < 4; i++)
	{
		backgroundPalette[i] = defaultPalette[i];
	}

	ram[IO_REGISTER | LCDC] = 0x91;
	ram[IO_REGISTER | 0x41] = 0x85;
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

void videoUpdateTexture(GLuint texture, int width, int height, int * data)
{
	GLint last_texture;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, last_texture);
}

GLuint  videoCreateTexture(int width, int height, int * data)
{
	for (int i = 0; i < height * width; i++) data[i] = defaultPalette[0];

	// Upload texture to graphics system
	GLint last_texture;
	GLuint newText;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	glGenTextures(1, &newText);
	glBindTexture(GL_TEXTURE_2D, newText);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	// Restore state
	glBindTexture(GL_TEXTURE_2D, last_texture);

	return newText;
}


void Video::createTextures()
{
	g_backgroundTexture = videoCreateTexture(256, 256, background);
	screenTexture = videoCreateTexture(SCREEN_W, SCREEN_H, screen);
}

void Video::paintSprites(int line)
{
	if ((ram[IO_REGISTER | LCDC] & 0x02) == 0)
		return;

	int nbLinePerSprite = 8;
	if (ram[IO_REGISTER | LCDC] & 0x04)
		nbLinePerSprite = 16;

	struct oamBlock * spriteTab = (struct oamBlock *)(ram + OAM_ADDR);

	if (line == 0)
	{
		// bubble sort sprite by x	
		for (int i = 0; i < SPRITE_INFO_COUNT; i++) spriteSortedList[i] = i;
		for (int c = 0; c < SPRITE_INFO_COUNT - 1; c++)
		{
			for (int d = 0; d < SPRITE_INFO_COUNT - c - 1; d++)
			{
				if (spriteTab[spriteSortedList[d]].x < spriteTab[spriteSortedList[d + 1]].x)
				{
					int swap = spriteSortedList[d];
					spriteSortedList[d] = spriteSortedList[d + 1];
					spriteSortedList[d + 1] = swap;
				}
			}
		}
	}

	for (int i = 0; i < SPRITE_INFO_COUNT; i++)
	{
		struct oamBlock * sprite = (struct oamBlock *)(ram + OAM_ADDR + spriteSortedList[i] * SPRITE_INFO_SIZE);
		
		int * palette = sprite->flags.palette ? spritePalette1 : spritePalette0;
		int tileDataAddr = 0x8000;

		int VRAMBegin = tileDataAddr + sprite->pattern * TILE_SIZE_IN_BYTES;
		if((sprite->y == 0) || (sprite->y >= (144 + 16)))
			continue;
		if ((line < (sprite->y - 16)) || (line >= (sprite->y - 16 + nbLinePerSprite)))
			continue;
		unsigned char y = (sprite->flags.yFlip) ? nbLinePerSprite -1 - line + sprite->y - 16 : line - sprite->y + 16;

		{
			int ramIndex = VRAMBegin + y * 2;
			unsigned char tileData1 = *(ram + ramIndex);
			unsigned char tileData2 = *(ram + ramIndex + 1);
			for (int x = 0; x < 8; x++)
			{
				int colorIndex = (((tileData1 >> x) & 1) | (((tileData2 >> x) & 1) << 1)) & 0x03;
				if (colorIndex) // transparency
				{
					unsigned char pixelX = (sprite->flags.xFlip ? x : (7 - x)) + sprite->x - 8;

					if ((pixelX >= 0) && (pixelX < SCREEN_W))
					{
						if (sprite->flags.priority)
						{
							if (screen[pixelX + line * SCREEN_W] == backgroundPalette[0])
							{
								screen[pixelX + line * SCREEN_W] = palette[colorIndex];
							}
						}
						else
						{
							screen[pixelX + line * SCREEN_W] = palette[colorIndex];
						}
					}
					
				}
			}
		}
	}
}

void Video::paintFullBackground()
{
	if ((ram[IO_REGISTER | LCDC] & 0x02) == 0)
		return;
	int tileMapAddr = (ram[IO_REGISTER | LCDC] & 0x08) ? 0x9c00 : 0x9800; // BG Tile Map Display Select 
	int tileDataAddr = (ram[IO_REGISTER | LCDC] & 0x10) ? 0x8000 : 0x9000; // BG & Window Tile Data Select
	for (int i = 0; i < NB_TILE; i++)
	{
		int tileIndex = 0;
		if (tileDataAddr == 0x8000)
			tileIndex = ram[tileMapAddr + i];
		else
			tileIndex = (signed char)(ram[tileMapAddr + i]);

		int VRAMBegin = tileDataAddr + tileIndex * TILE_SIZE_IN_BYTES;
		for (int y = 0; y < 8; y++)
		{
			int ramIndex = VRAMBegin + y * 2;
			unsigned char tileData1 = ram[ramIndex];
			unsigned char tileData2 = ram[ramIndex + 1];
			for (int x = 0; x < 8; x++)
			{
				int colorIndex = (((tileData1 >> x) & 1) | (((tileData2 >> x) & 1) << 1)) & 0x03;

				background[(7 - x) + y * 256 + i % 32 * 8 + i / 32 * 8 * 256] = backgroundPalette[colorIndex];
			}
		}
	}
}

void Video::paintBackground(unsigned char line)
{
	if ((ram[IO_REGISTER | LCDC] & 0x01) == 0)
		return;
	int tileMapAddr = (ram[IO_REGISTER | LCDC] & 0x08) ? 0x9c00 : 0x9800; // BG Tile Map Display Select 
	int tileDataAddr = (ram[IO_REGISTER | LCDC] & 0x10) ? 0x8000 : 0x9000; // BG & Window Tile Data Select
	unsigned char offsetX = ram[IO_REGISTER | SCX];
	unsigned char offsetY = ram[IO_REGISTER | SCY];

	int firstTile = (line + offsetY)%256 / 8 * NB_COL_OF_TILE;

	for (int i = firstTile; i < (firstTile + NB_COL_OF_TILE); i++)
	{
		int tileIndex;
		if (tileDataAddr == 0x8000)
			tileIndex = ram[tileMapAddr + i];
		else
			tileIndex = (signed char)(ram[tileMapAddr + i]);

		int VRAMBegin = tileDataAddr + tileIndex * TILE_SIZE_IN_BYTES;


		unsigned char y = line + offsetY - i / 32 * 8;
		unsigned char pixelY = y + i / 32 * 8;
		unsigned char screenY = pixelY - offsetY;

		int ramIndex = VRAMBegin + y * 2;
		unsigned char tileData1 = ram[ramIndex];
		unsigned char tileData2 = ram[ramIndex + 1];
		for (int x = 0; x < 8; x++)
		{
			int colorIndex = (((tileData1 >> x) & 1) | (((tileData2 >> x) & 1) << 1)) & 0x03;
			unsigned char screenX = (7 - x) + i % 32 * 8 - offsetX;

			if((screenX >=0) && (screenX < SCREEN_W))
				screen[screenX + screenY * SCREEN_W] = backgroundPalette[colorIndex];
		}
	}
}

void Video::paintWindow(unsigned int line)
{
	if ((ram[IO_REGISTER | LCDC] & 0x01) == 0)
		return;
	if ((ram[IO_REGISTER | LCDC] & 0x20) == 0)
		return;
	int tileMapAddr = (ram[IO_REGISTER | LCDC] & 0x40) ? 0x9c00 : 0x9800; // Window Tile Map Display Select 
	int tileDataAddr = (ram[IO_REGISTER | LCDC] & 0x10) ? 0x8000 : 0x9000; // BG & Window Tile Data Select

	signed int offsetX = ram[IO_REGISTER + WX] - 7;
	signed int offsetY = ram[IO_REGISTER + WY];

	if (offsetX > 159) return;
	if (line < offsetY) return;

	int firstTile = (line - offsetY) / 8 * NB_COL_OF_TILE;

	for (int i = firstTile; i < (firstTile + NB_COL_OF_TILE); i++)
	{
		int tileIndex;
		if (tileDataAddr == 0x8000)
			tileIndex = ram[tileMapAddr + i];
		else
			tileIndex = (signed char)(ram[tileMapAddr + i]);

		int VRAMBegin = tileDataAddr + tileIndex * TILE_SIZE_IN_BYTES;

		int y = line - offsetY - i / 32 * 8;
		int pixelY = y + i / 32 * 8;

		int ramIndex = VRAMBegin + y * 2;
		unsigned char tileData1 = ram[ramIndex];
		unsigned char tileData2 = ram[ramIndex + 1];
		for (int x = 0; x < 8; x++)
		{
			int colorIndex = (((tileData1 >> x) & 1) | (((tileData2 >> x) & 1) << 1)) & 0x03;
			signed int screenX = (7 - x) + i % 32 * 8 + offsetX;

			if ((screenX >= 0) && (screenX >= offsetX) && (screenX < SCREEN_W))
				screen[screenX + line * SCREEN_W] = backgroundPalette[colorIndex];
		}
	}
}

void Video::renderScreen()
{
	int y = 0;
	int x = 0;
	unsigned char offsetX = ram[IO_REGISTER | SCX];
	unsigned char offsetY = ram[IO_REGISTER | SCY];
	while (y < SCREEN_H)
	{
		int x = 0;
		unsigned char backgroundY = y + offsetY;
		while (x < SCREEN_W)
		{
			unsigned char backgroundX = x + offsetX;
			screen[x + y * SCREEN_W] = background[backgroundX + backgroundY * 256];
			x++;
		}
		y++;
	}
}

void Video::updateTextures()
{
	videoUpdateTexture(g_backgroundTexture, 256, 256, background);
	videoUpdateTexture(screenTexture, SCREEN_W, SCREEN_H, screen);
}

void Video::update()
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
	
	int newtotalFrameCycle = (totalFrameCycle + TC) % (154*FULL_LINE_CYCLE);
	if (newtotalFrameCycle >= (VISIBLE_LINE * FULL_LINE_CYCLE + 4))
	{
		if (totalFrameCycle < (VISIBLE_LINE * FULL_LINE_CYCLE + 4))
		{
			// trigger VBL interrupt
			cpu->trigInterrupt(IRQ_V_Blank);
		}
	}
	totalFrameCycle = newtotalFrameCycle;


	int oldLY = ram[IO_REGISTER | LY];
	int newLY = totalFrameCycle / FULL_LINE_CYCLE;
	ram[IO_REGISTER | LY] = newLY;

	if ((oldLY != ram[IO_REGISTER | LYC]) &&
		(newLY == ram[IO_REGISTER | LYC]))
	{
		if (reg_stat & (1 << 6)) // LY=LYC Check Enable 
		{
			cpu->trigInterrupt(IRQ_LCDC);
		}
	}
	
	if(newLY == ram[IO_REGISTER | LYC])
		reg_stat |= (1 << 2); // LY=LYC Comparison Signal 
	else
		reg_stat &= ~(1 << 2); // LY=LYC Comparison Signal 


	currentLineCycle += TC;
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
		if (currentLineCycle > 250) newScreenMode = 0;
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

  	reg_stat &= 0b11111100;
  	reg_stat |= stateMode;

	if(ScreenModeChanged)
	{
		if (stateMode == 1)
		{
			paintFullBackground();
			updateTextures(); 
			if (reg_stat & (1 << 4))
			{
				cpu->trigInterrupt(IRQ_LCDC);
			}

			if (reg_stat & (1 << 5))
			{
				cpu->trigInterrupt(IRQ_LCDC);
			}
		}

		if (stateMode == 0)
		{
			if(reg_stat & (1 << 3))
				cpu->trigInterrupt(IRQ_LCDC);
		}

		if (stateMode == 2)
		{
			if (reg_stat & (1 << 5))
			{
				cpu->trigInterrupt(IRQ_LCDC);
			}
		}
		if (stateMode == 3)
		{
			paintBackground(newLY);
			paintWindow(newLY);
			paintSprites(newLY);
		}
	}

	// will handle ligne 153 special LY timing later
}


void writePalette(int *palette, int value)
{
	int index = value & 0x03;
	palette[0] = defaultPalette[index];
	index = (value >> 2) & 0x03;
	palette[1] = defaultPalette[index];
	index = (value >> 4) & 0x03;
	palette[2] = defaultPalette[index];
	index = (value >> 6) & 0x03;
	palette[3] = defaultPalette[index];
}

int Video::write(int registerAddr, int value)
{
	if (registerAddr == 0x41) // STAT
	{
		value &= 0b11111000;
		value |= reg_stat & 0b00000111;
	}
	else if (registerAddr == LCDC) // STAT
	{
		int i = 0;
		i++;
	}
	else if (registerAddr == BGP)
	{
		writePalette(backgroundPalette, value);
	}
	else if (registerAddr == OBP0)
	{
		writePalette(spritePalette0, value);
	}
	else if (registerAddr == OBP1)
	{
		writePalette(spritePalette1, value);
	}

	return value;
}
