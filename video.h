#pragma once

#define OAM_ADDR 0xFE00

#define SPRITE_INFO_SIZE 4
#define SPRITE_INFO_COUNT 40
#define NB_LINE_OF_TILE 32
#define NB_COL_OF_TILE 32
#define FULL_LINE_CYCLE 456
#define VISIBLE_LINE 144
#define TILE_SIZE_IN_BYTES 16
#define NB_TILE (NB_LINE_OF_TILE * NB_COL_OF_TILE)
#define SCREEN_W 160 
#define SCREEN_H 144

struct Video
{
	int screen[SCREEN_W * SCREEN_H];
	int totalFrameCycle = 0;
	int currentLineCycle = 0;
	int stateMode = 0;

	int backgroundPalette[4];
	int spritePalette0[4];
	int spritePalette1[4];
	char spriteSortedList[SPRITE_INFO_COUNT];

	void paintSprites(int line);
	void paintFullBackground();
	void paintBackground(unsigned char line);
	void paintWindow(unsigned int line);
	void renderScreen();
	void updateTextures();
public :
	void reset();
	int  write(int registerAddr, int value);
	void update();
	void createTextures();
	void imgui();
};

extern Video * video;