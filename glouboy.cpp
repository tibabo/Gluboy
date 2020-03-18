//
//  gluboy.cpp
//  
//
//  Created by joseph pinkasfeld 
//  
//

#include "glouboy.h"
#include <stdio.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_memory_editor.h"
#include "nfd.h"
#include "io.h"
#include "video.h"
#include "timer.h"
#include "audio.h"
#include "cpu.h"
static MemoryEditor mem_edit_1;                                            // store your state somewhere

enum {BUF_SIZE = 1024};

#define KEY_F3 (292)
#define KEY_F4 (293)


long romSize;

int breakpointPC;
int breakpointOpcode;

struct state
{
	Audio audio;
	CPU   cpu;
	Video video;
	Timer timer;
};

#define TOTAL_STATES (60 * 10)
state states[TOTAL_STATES];
int firstState = -1;
int lastState = 0;


Audio * audio = nullptr;
CPU *   cpu = nullptr;
Video * video = nullptr;
Timer * timer = nullptr;
unsigned char * rom = nullptr;

void Glouboy::init(const char * path)
{
	if (rom != nullptr)
	{
		free(rom);
		romSize = 0;
	}

	FILE * f = ImFileOpen(path, "rb");
	fseek(f, 0, SEEK_END);
	romSize = ftell(f);
	rewind(f);

	rom = (unsigned char*)malloc(romSize + 1);
	fread(rom, 1, romSize, f);
	((char*)rom)[romSize] = 0;
	fclose(f);

	if (video) delete video;
	video = new Video;
	video->createTextures();

	if (timer) delete timer;
	timer = new Timer;

	if (cpu) delete cpu;
	cpu = new CPU;

	// init ram with rom
	assert(romSize >= 0x8000);
	memset(ram, 0, 0x10000);
	memcpy(ram, rom, 0x8000);


	video->reset();
	cpu->init();

	if (audio) delete audio;
	audio = new Audio;
	audio->init();

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
			fprintf(f, "%04x\n", cpu->PC);
		}
		else
		{
			unsigned int otherPC;
			fscanf(f, "%04x\n", &otherPC);
			if (cpu->PC != otherPC)
			{
				int i = 0;
				i++;
			}
		}
	}

	if (cpu->handleInterrupts())
	{
		TC += 20;
	}
	else if (cpu->haltMode == false)
	{
		unsigned char instruction = ram[cpu->PC];
		opcode[instruction].funct();
	}
	else
	{
		TC += 4;
	}
	
	cpu->update();
	audio->update();
	video->update();
	timer->update();
}




void Glouboy::update()
{
	static bool run = false;
	static bool showMem = false;
	static bool showAudio = false;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open", nullptr)) {
				nfdchar_t *outPath = NULL;
				if (NFD_OpenDialog("gb", NULL, &outPath) == NFD_OKAY)
				{
					init(outPath);
					run = true;
				}
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("option"))
		{
			if (ImGui::MenuItem(showMem ? "Hide Memory" : "Show Memory", nullptr)) { showMem = !showMem; }
			if (ImGui::MenuItem(showAudio ? "Hide Audio" : "Show Audio", nullptr)) { showAudio = !showAudio; }
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (rom == nullptr) return;


	if (showMem)
		mem_edit_1.DrawWindow("Memory Editor", (unsigned char*)ram, 0x10000); // create a window and draw memory editor (if you already have a window, use DrawContents())

	if (showAudio)
		audio->imgui();

	video->imgui();
																			  

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


	if (ImGui::Button(run?"pause":"run"))
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
		if (handleJoypad() && (firstState != -1))
		{
			if (lastState != firstState)
			{
				lastState--;
				if (lastState < 0) lastState += TOTAL_STATES;
				*audio = states[lastState].audio;
				*cpu = states[lastState].cpu;
				*video = states[lastState].video;
				*timer = states[lastState].timer;
				video->updateTextures();
			}
		}
		else
		{
			int accuTC = 0;
			int target = 4194304 / 60;
			while (target > accuTC)
			{
				execute();
				accuTC += TC;
			}
			states[lastState++] = { *audio, *cpu, *video, *timer };
			lastState = lastState % TOTAL_STATES;
			if (firstState == -1) firstState = 0;
			if (lastState == firstState)
			{
				firstState = (lastState + 1) % TOTAL_STATES;
			}
		}
	}

	if (ImGui::Button("run until"))
	{
		if (breakpointPC != -1)
		{
			do {
				execute();
			} while (cpu->PC != breakpointPC);
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
				nextOp = ram[cpu->PC] << 16 | ram[cpu->PC + 1] << 8 | ram[cpu->PC + 2];
			} while (breakpointOpcode != nextOp);
		}
	}
	ImGui::SameLine();
	ImGui::InputInt("breakpoint oppcode", &breakpointOpcode, 1, 0, ImGuiInputTextFlags_CharsHexadecimal);


	cpu->imgui();

}
