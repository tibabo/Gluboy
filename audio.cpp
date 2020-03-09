#include "al.h"
#include "alc.h"
#include "cpu.h"
#include "io.h"
#include "imgui.h"
#include "imgui_internal.h"

ALCdevice *device;

#define PI 3.141592
#include <cmath>

int CurrentBuffer = 0;
int BufferPosition = 0;
#define FREQUENCY 44100
#define NB_SAMPLE_PER_FRAME 735
#define NB_CHANNEL 2
#define NB_BUFFER 10 
#define BUFFER_SIZE (NB_SAMPLE_PER_FRAME * NB_CHANNEL * 2)
short buffers[NB_BUFFER][NB_SAMPLE_PER_FRAME * NB_CHANNEL];
ALuint source;
ALuint FBufferID[NB_BUFFER];

void audioInit()
{
	device = alcOpenDevice(nullptr);

	if (!device)
		return;

	ALCenum error;
	error = alGetError();
	if (error != AL_NO_ERROR)
	{

	}
	ALCcontext *context;

	context = alcCreateContext(device, NULL);
	if (!alcMakeContextCurrent(context))
	{
		error = alGetError();
		if (error != AL_NO_ERROR)
		{

		}
	}



	alGenBuffers(NB_BUFFER, &FBufferID[0]);
	error = alGetError();
	if (error != AL_NO_ERROR)
	{

	}

	error = alGetError();
	if (error != AL_NO_ERROR)
	{

	}
	alGenSources(1, &source);
	error = alGetError();
	if (error != AL_NO_ERROR)
	{
	}

}


void playBuffer()
{
	ALCenum error;
	ALint queuedBuffer;
	alGetSourcei(source, AL_BUFFERS_QUEUED, &queuedBuffer);
	if (queuedBuffer == NB_BUFFER)
	{
		printf("overlapping buffer\n");
		BufferPosition = 0;
		return;
	}

	alBufferData(FBufferID[CurrentBuffer], AL_FORMAT_STEREO16, &buffers[CurrentBuffer][0], BufferPosition * 2, FREQUENCY);
	alSourceQueueBuffers(source, 1, &FBufferID[CurrentBuffer]);
	error = alGetError();
	ALint source_state;
	alGetSourcei(source, AL_SOURCE_STATE, &source_state);



	if(source_state != AL_PLAYING)
	{
		printf("not playing\n");
	}
	if ((source_state != AL_PLAYING) && (queuedBuffer > 4))
		alSourcePlay(source);

	printf("add Buffer %d samples\n", BufferPosition/2);
	error = alGetError();
	CurrentBuffer++;
	BufferPosition = 0;
	if (CurrentBuffer == NB_BUFFER) CurrentBuffer = 0;
}

void checkProcessedBuffer()
{
	ALCenum error;
	ALint Processed;
	alGetSourcei(source, AL_BUFFERS_PROCESSED, &Processed);
	error = alGetError();
	ALuint ProcessedBuffers[NB_BUFFER];
	if (Processed != 0)
	{
		alSourceUnqueueBuffers(source, Processed, ProcessedBuffers);
		error = alGetError();
		ALint queuedBuffer;
		alGetSourcei(source, AL_BUFFERS_QUEUED, &queuedBuffer);
		
		printf("%d buffer in queue \n", queuedBuffer);
		
		if(queuedBuffer <= 2) playBuffer();
	}
}


#define GAMEBOY_FREQ 4194304
int audioAccu = 0;
int frameSequencerAccu = 0;
#define NB_GB_CHANNEL 4
signed int length[NB_GB_CHANNEL];
signed int channelTimer[NB_GB_CHANNEL];

char dutyCycleCounter[3] = { 0, 0, 0 };
unsigned char waveForm[4] = { 0b00000001, 0b10000001, 0b10000111, 0b01111110 };

char register1[] = { NR11, NR21, NR31, NR41 };
char register2[] = { NR12, NR22, NR32, NR42 };
char register3[] = { NR13, NR23, NR33, NR43 };
char register4[] = { NR14, NR24, NR34, NR44 };

struct Envelope
{
	int volume;
	bool up;
	int timer;
	int periode;
};

Envelope envelopes[4];
unsigned short LFSR = 0xffff;
bool sevenStage = false;

int reloadNoiseTimer()
{
	int ret = 0;

	int clockDivider = ram[IO_REGISTER | NR43] & 0x07;
	if (clockDivider)
		ret = 8 * clockDivider;
	else
		ret = 4;
	sevenStage = (ram[IO_REGISTER | NR43] & 0x08) > 0;
	int prescaler = (ram[IO_REGISTER | NR43] & 0xf0) >> 4;
	ret *= 2 << prescaler;

	return ret;
}

void shiftLFSR()
{
	LFSR = LFSR >> 1;
	if (sevenStage)
	{
		LFSR &= 0xff7f;
		LFSR |= ((LFSR & 1) ^ ((LFSR & 2) >> 1)) << 7;
	}
	else
	{
		LFSR &= 0x7fff;
		LFSR |= ((LFSR & 1) ^ ((LFSR & 2) >> 1)) << 15;
	}
}

void updateAudioTimers(char clock)
{
	for (int i = 0; i < NB_GB_CHANNEL; i++)
	{
		channelTimer[i] -= clock;
		if (channelTimer[i] <= 0)
		{
			if (i == 3)
			{
				channelTimer[i] += reloadNoiseTimer();
				shiftLFSR();
			}
			else
			{
				dutyCycleCounter[i]++;
				channelTimer[i] += (2048 - (ram[IO_REGISTER | register3[i]] | ((ram[IO_REGISTER | register4[i]] & 0x07) << 8))) * 4;
			}
		}
	}
}

void updateEnvelopes()
{
	for (int i = 0; i < NB_GB_CHANNEL; i++)
	{
		if (envelopes[i].periode)
		{
			envelopes[i].timer--;
			if (envelopes[i].timer == 0)
			{
				envelopes[i].up ? envelopes[i].volume++ : envelopes[i].volume--;
				if (envelopes[i].volume > 15) envelopes[i].volume = 15;
				else if (envelopes[i].volume < 0) envelopes[i].volume = 0;

				envelopes[i].timer = envelopes[i].periode;
			}
		}
	}
}

void updateFrameSequencer()
{
	static int step = 0;
	switch (step)
	{
		case 2:
		case 6:
		case 0:
		case 4:
			for (int i = 0; i < NB_GB_CHANNEL; i++) if (length[i] > 0) length[i]--;
			break;
		case 7:
			updateEnvelopes();
			break;
	}

	step++;
	if (step >= 8) step = 0;
}


char wave(char channel)
{
	bool lenghtEnable = ram[IO_REGISTER | register4[channel]] & 0x40;
	if ((length[channel] && lenghtEnable) || !lenghtEnable)
	{
		ram[IO_REGISTER | NR52] |= 1 << channel;
		char duty = ram[IO_REGISTER | register1[channel]] >> 6;
		if ((waveForm[duty] >> (dutyCycleCounter[channel] & 0x07)) & 1)
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}
	ram[IO_REGISTER | NR52] &= ~(1 << channel);
	return 0;
}


char noise()
{
	int channel = 3;
	bool lenghtEnable = ram[IO_REGISTER | register4[channel]] & 0x40;
	if ((length[channel] && lenghtEnable) || !lenghtEnable)
	{
		ram[IO_REGISTER | NR52] |= 0x08;
		return (LFSR & 1) ? -1 : 1;
	}
	ram[IO_REGISTER | NR52] &= 0xf7;
	return 0;
}

bool optionSquare0 = true;
bool optionSquare1 = true;
bool noiseChannel  = true;

void audioImgui()
{
	bool Open = true;
	if (ImGui::Begin("Audio", &Open, ImGuiWindowFlags_NoScrollbar))
	{
		static bool show = false;
		ImGui::Checkbox("Channel Square 1", &optionSquare0);
		ImGui::Checkbox("Channel Square 2", &optionSquare1);
		ImGui::Checkbox("Channel Noise",    &noiseChannel);
	}
	ImGui::End();
}

int mixTerminal(char terminal)
{
	int output = 0;
	char channelControl = (ram[IO_REGISTER | NR50] >> (terminal * 4)) & 0x0f;

	int volume = channelControl & 0x07;
	char enable = (ram[IO_REGISTER | NR51] >> (terminal * 4)) & 0x0f;
	if ((enable & 0x01) && optionSquare0)
	{
		output += envelopes[0].volume * wave(0);
	}

	if ((enable & 0x02) && optionSquare1)
	{
		output += envelopes[1].volume * wave(1);
	}

	if ((enable & 0x08) && noiseChannel)
	{
		output += envelopes[3].volume * noise();
	}

	output *= volume + 1;

	return output;
}

void audioUpdate()
{
	updateAudioTimers(TC);

	frameSequencerAccu += TC;
	if (frameSequencerAccu >= 8192)
	{
		frameSequencerAccu -= 8192;
		updateFrameSequencer();
	}

	audioAccu += TC;
	int nbSampletoPlay = audioAccu *  FREQUENCY / GAMEBOY_FREQ;
	if (nbSampletoPlay == 0) 
		return;

	audioAccu -= nbSampletoPlay * GAMEBOY_FREQ / FREQUENCY;

	float volume = 1.0f;
	for (int i = 0; i < nbSampletoPlay; i++)
	{			 
		short left  = mixTerminal(0);
		short right = mixTerminal(1);

		// 16-bit sample: 2 bytes
		buffers[CurrentBuffer][BufferPosition + 0] = left * 63;
		buffers[CurrentBuffer][BufferPosition + 1] = right * 63;
		BufferPosition += 2;

		checkProcessedBuffer();

		if (BufferPosition == (NB_SAMPLE_PER_FRAME * NB_CHANNEL))
		{
			playBuffer();
		}

	}
}


void setEnvelope(int channel)
{
	envelopes[channel].volume = ram[IO_REGISTER + register2[channel]] >> 4;
	envelopes[channel].up     =(ram[IO_REGISTER + register2[channel]] >> 3) & 1;
	envelopes[channel].timer  = ram[IO_REGISTER + register2[channel]] & 0x07;
	envelopes[channel].periode = envelopes[channel].timer;
}

void updateAudioChannel(char channel, char value)
{
	if (value & 0x80)
	{
		if (channel == 0 || channel == 1)
		{
			length[channel] = ram[IO_REGISTER | register1[channel]] & 0x3F;
			channelTimer[channel] = ram[IO_REGISTER + register3[channel]] | ((value & 0x07) << 8);
			setEnvelope(channel);
		}
		else if (channel == 2)
		{
			length[channel] = ram[IO_REGISTER | register1[channel]];
			channelTimer[channel] = ram[IO_REGISTER | register3[channel]] | ((value & 0x07) << 8);
		}
		else if (channel == 3)
		{
			length[channel] = ram[IO_REGISTER | register1[channel]] & 0x3F;
			setEnvelope(channel);
			LFSR = 0xffff;
			channelTimer[3] = reloadNoiseTimer();
		}
	}
}

