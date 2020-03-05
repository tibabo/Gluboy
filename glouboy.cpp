//
//  glouboy.cpp
//  imguiex-osx
//
//  Created by pinkasfeld on 30/11/2017.
//  Copyright © 2017 Joel Davis. All rights reserved.
//

#include "glouboy.h"
#include <stdio.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_memory_editor.h"

#include "io.h"
#include "video.h"
#include "timer.h"
#include "audio.h"
#include "cpu.h"
static MemoryEditor mem_edit_1;                                            // store your state somewhere

enum {BUF_SIZE = 1024};

#define KEY_F3 (292)
#define KEY_F4 (293)

unsigned char * rom = 0;
long romSize;

int breakpointPC;
int breakpointOpcode;



void Glouboy::init()
{
	if (rom != nullptr)
	{
		free(rom);
		romSize = 0;
	}

	char bundle_path[BUF_SIZE];
	//GetRessourceBundlePath(bundle_path, BUF_SIZE);
//	sprintf(bundle_path, "%s", "r-type.gb");
	sprintf(bundle_path, "%s", "zelda.gb");

	FILE * f = ImFileOpen(bundle_path, "rb");
	fseek(f, 0, SEEK_END);
	romSize = ftell(f);
	rewind(f);

	rom = (unsigned char*)malloc(romSize + 1);
	fread(rom, 1, romSize, f);
	((char*)rom)[romSize] = 0;

	videoCreateTextures();


	// init ram with rom
	assert(romSize >= 0x8000);
	memcpy(ram, rom, 0x8000);


	videoReset();
	cpuInit();
	audioInit()
	breakpointPC = -1;
	
}

static FILE * f = nullptr;
bool loggingNewData = false; 
void Glouboy::execute()
{
	TC = 0;
	if (f)
	{
		if (loggingNewData)
		{
			fprintf(f, "%04x\n", PC);
		}
		else
		{
			unsigned int otherPC;
			fscanf(f, "%04x\n", &otherPC);
			if (PC != otherPC)
			{
				int i = 0;
				i++;
			}
		}
	}

	if (handleInterrupts())
	{
		TC += 20;
	}
	else if (haltMode == false)
	{
		unsigned char instruction = ram[PC];
		opcode[instruction].funct();
	}
	else
	{
		TC += 4;
	}
	
	cpuUpdate();
	videoUpdate();
	handleJoypad();
	timerUpdate();
}

void wakeHalteMode()
{
	if (haltMode)
	{
		haltMode = false;
		TC += 4;
	}
}


void Glouboy::update()
{
	mem_edit_1.DrawWindow("Memory Editor", (unsigned char*)ram, 0x10000); // create a window and draw memory editor (if you already have a window, use DrawContents())
	videoImguiWindow();
																			  
//    unsigned char instruction = rom[PC];
	static bool hexa = true;
	ImGui::PushItemWidth(120);
	
	if (ImGui::Button("exec  F3") || ImGui::IsKeyReleased(KEY_F3))
	{
		execute();
	}

	if (ImGui::Button("run 1 frame F4") || ImGui::IsKeyReleased(KEY_F4))
	{
		int accuTC = 0;
		int target = 456 * 154;
		while (target > accuTC)
		{
			execute();
			accuTC += TC;
		}
	}

	static bool run = false;
	if (ImGui::Button("run"))
	{
		run = !run;
	}
	if (ImGui::Button("log"))
	{
		if (run == false)
		{
			if(loggingNewData)
			{
				f = ImFileOpen("log.txt", "wb+");
			}
			else
			{
				f = ImFileOpen("log.txt", "rb");
			}
			
		}
		else
		{
			fclose(f);
		}
		run = !run;
	}
	ImGui::SameLine();
	ImGui::Checkbox("write file", &loggingNewData);

	if (run)
	{
		int accuTC = 0;
		int target = 456 * 154;
		while (target > accuTC)
		{
			execute();
			accuTC += TC;
		}
	}

	if (ImGui::Button("run until"))
	{
		if (breakpointPC != -1)
		{
			do {
				execute();
			} while (PC != breakpointPC);
		}
	}
	ImGui::SameLine();
	ImGui::InputInt("breakpoint PC", &breakpointPC, 1, 0, ImGuiInputTextFlags_CharsHexadecimal);
	if (ImGui::Button("run until opcode"))
	{
		if (breakpointOpcode != -1)
		{
			int nextOp = -1;
			do {
				execute();
				nextOp = ram[PC] << 16 | ram[PC + 1] << 8 | ram[PC + 2];
			} while (breakpointOpcode != nextOp);
		}
	}
	ImGui::SameLine();
	ImGui::InputInt("breakpoint oppcode", &breakpointOpcode, 1, 0, ImGuiInputTextFlags_CharsHexadecimal);


	cpuImgui();
}
