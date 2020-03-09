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
int noisePeriode = 0;

void audioReloadNoiseTimer(unsigned char value)
{
	noisePeriode = 0;

	int clockDivider = value & 0x07;
	if (clockDivider)
		noisePeriode = 8 * clockDivider;
	else
		noisePeriode = 4;
	sevenStage = (value & 0x08) > 0;
	int prescaler = (value & 0xf0) >> 4;
	noisePeriode *= 2 << prescaler;
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


char sweepPeriod = 0;
char sweepTimer = 0;
int shadowFreq = 0;
int sweepShift = 0;
int way = 0;

bool sweepOverflow = false;

void updateSweep()
{
	if (sweepPeriod && sweepShift && sweepTimer && !--sweepTimer)
	{
		sweepTimer = sweepPeriod;

		int newPeriode = shadowFreq + way * (shadowFreq >> (sweepShift));
		if (newPeriode < 0)
		{
			sweepTimer = 0;
		}
		if (newPeriode > 0)
		{
			shadowFreq = newPeriode;
		}
		if (newPeriode >= 2048)
		{
			sweepTimer = 0;
			sweepOverflow = true;
			ram[IO_REGISTER | NR52] &= ~(1);
		}
	}
}

int computeChannelPeriode(int channel)
{
	int multiplier;
	if (channel == 2) multiplier = 2;
	else multiplier = 4;

	return (2048 - (ram[IO_REGISTER | register3[channel]] | ((ram[IO_REGISTER | register4[channel]] & 0x07) << 8))) * multiplier;
}

int SampleIndex = 0;

void updateAudioTimers(char clock)
{
	for (int i = 0; i < NB_GB_CHANNEL; i++)
	{
		channelTimer[i] -= clock;
		if (channelTimer[i] <= 0)
		{
			if (i == 3)
			{
				channelTimer[i] += noisePeriode;
				shiftLFSR();
			}
			else if (i == 2)
			{
				SampleIndex++;
				channelTimer[i] += computeChannelPeriode(i);
			}
			else
			{
				dutyCycleCounter[i]++;
				if ((i == 0) && sweepPeriod && sweepShift)
				{
					channelTimer[i] += (2048 - shadowFreq)*4;
				}
				else channelTimer[i] += computeChannelPeriode(i);
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
			updateSweep();
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


char sample()
{
	if ((ram[IO_REGISTER | NR30] & 0x80) == 0) return 0; // DAC POWER	

	int channel = 2;
	bool lenghtEnable = ram[IO_REGISTER | register4[channel]] & 0x40;
	if ((length[channel] && lenghtEnable) || !lenghtEnable)
	{
		ram[IO_REGISTER | NR52] |= 1 << channel;
		char data = ((ram[IO_REGISTER | Wave_Pattern_RAM + ((SampleIndex%32) >> 1)]) >> (4* (SampleIndex%2))) & 0x0f;
		switch((ram[IO_REGISTER | NR32] >> 5) & 0x3)
		{
		case 0: 
			data = 8; 
			break;
		case 2:
			data >>= 1;
			break;
		case 3:
			data >>= 2;
			break;
		}

		return data - 8;
	}
	ram[IO_REGISTER | NR52] &= ~(1 << channel);
	return 0;
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
bool optionSample  = true;
bool noiseChannel  = true;

void audioImgui()
{
	bool Open = true;
	if (ImGui::Begin("Audio", &Open, ImGuiWindowFlags_NoScrollbar))
	{
		static bool show = false;
		ImGui::Checkbox("Channel 1 Square 1", &optionSquare0);
		ImGui::Checkbox("Channel 2 Square 2", &optionSquare1);
		ImGui::Checkbox("Channel 3 Sample",   &optionSample);
		ImGui::Checkbox("Channel 4 Noise",    &noiseChannel);
	}
	ImGui::End();
}

int mixTerminal(char terminal)
{
	int output = 0;
	char channelControl = (ram[IO_REGISTER | NR50] >> (terminal * 4)) & 0x0f;

	int volume = channelControl & 0x07;
	char enable = (ram[IO_REGISTER | NR51] >> (terminal * 4)) & 0x0f;
	if ((enable & 0x01) && !sweepOverflow && optionSquare0)
	{
		output += envelopes[0].volume * wave(0);
	}

	if ((enable & 0x02) && optionSquare1)
	{
		output += envelopes[1].volume * wave(1);
	}

	if ((enable & 0x04) && optionSample)
	{
		output += sample();
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
	frameSequencerAccu += TC;
	if (frameSequencerAccu >= 8192)
	{
		frameSequencerAccu -= 8192;
		updateFrameSequencer();
	}

	updateAudioTimers(TC);


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
		buffers[CurrentBuffer][BufferPosition + 0] = left * 16;
		buffers[CurrentBuffer][BufferPosition + 1] = right * 16;
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

void audioUpdateSweepValue(unsigned char value)
{
	sweepPeriod = (value & 0x70) >> 4;
	sweepShift = value & 7;
	way = (value & 8) ? -1 : 1;
}

void updateAudioChannel(char channel, unsigned char value)
{
	ram[IO_REGISTER | register4[channel]] = value;

	if (value & 0x80)
	{
		if (channel == 0)
		{
			length[channel] = ram[IO_REGISTER | register1[channel]] & 0x3F;
			shadowFreq = ram[IO_REGISTER | register3[channel]] | ((value & 0x07) << 8);
			sweepOverflow = false;
			channelTimer[channel] = computeChannelPeriode(channel);
			sweepTimer = sweepPeriod;
			//updateSweep();
			setEnvelope(channel);
		}
		else if (channel == 1)
		{
			length[channel] = ram[IO_REGISTER | register1[channel]] & 0x3F;
			channelTimer[channel] = computeChannelPeriode(channel);
			setEnvelope(channel);
		}
		else if (channel == 2)
		{
			length[channel] = ram[IO_REGISTER | register1[channel]];
			channelTimer[channel] = computeChannelPeriode(channel);
			SampleIndex = 0;
		}
		else if (channel == 3)
		{
			length[channel] = ram[IO_REGISTER | register1[channel]] & 0x3F;
			setEnvelope(channel);
			LFSR = 0xffff;
			channelTimer[3] = noisePeriode;
		}
	}
}

