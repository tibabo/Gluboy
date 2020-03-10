#include "al.h"
#include "alc.h"
#include "cpu.h"
#include "io.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "audio.h"

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
unsigned char waveForm[4] = { 0b00000001, 0b10000001, 0b10000111, 0b01111110 };

char register1[] = { NR11, NR21, NR31, NR41 };
char register2[] = { NR12, NR22, NR32, NR42 };
char register3[] = { NR13, NR23, NR33, NR43 };
char register4[] = { NR14, NR24, NR34, NR44 };


#define GAMEBOY_FREQ 4194304

#define NB_GB_CHANNEL 4


void Audio::init()
{
	if(!device) device = alcOpenDevice(nullptr);
	else return;

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



void NoiseChannel::reloadTimer(unsigned char value)
{
	periode = 0;

	int clockDivider = value & 0x07;
	if (clockDivider)
		periode = 8 * clockDivider;
	else
		periode = 4;
	sevenStage = (value & 0x08) > 0;
	int prescaler = (value & 0xf0) >> 4;
	periode *= 2 << prescaler;
}

void NoiseChannel::shiftLFSR()
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



void Audio::updateSweep()
{
	if (sweep.period && sweep.shift && sweep.timer && !--sweep.timer)
	{
		sweep.timer = sweep.period;

		int newPeriode = sweep.shadowFreq + sweep.way * (sweep.shadowFreq >> (sweep.shift));
		if (newPeriode < 0)
		{
			sweep.timer = 0;
		}
		if (newPeriode > 0)
		{
			sweep.shadowFreq = newPeriode;
		}
		if (newPeriode >= 2048)
		{
			sweep.timer = 0;
			sweep.overflow = true;
			ram[IO_REGISTER | NR52] &= ~(1);
		}
	}
}

int Audio::computeChannelPeriode(int i)
{
	int multiplier;
	if (i == 2) multiplier = 2;
	else multiplier = 4;

	return (2048 - (ram[IO_REGISTER | register3[i]] | ((ram[IO_REGISTER | register4[i]] & 0x07) << 8))) * multiplier;
}


void Audio::updateTimers(char clock)
{
	for (int i = 0; i < NB_GB_CHANNEL; i++)
	{
		signed int & timer = channel[i].timer;
		timer -= clock;
		if (timer <= 0)
		{
			if (i == 3)
			{
				timer += noise.periode;
				noise.shiftLFSR();
			}
			else if (i == 2)
			{
				sampleIndex++;
				timer += computeChannelPeriode(i);
			}
			else
			{
				channel[i].dutyCycleCounter++;
				if ((i == 0) && sweep.period && sweep.shift)
				{
					timer += (2048 - sweep.shadowFreq)*4;
				}
				else timer += computeChannelPeriode(i);
			}
		}
	}
}

void Audio::updateEnvelopes()
{
	for (int i = 0; i < NB_GB_CHANNEL; i++)
	{
		Envelope & env = channel[i].envelope;
		if (env.periode)
		{
			env.timer--;
			if (env.timer == 0)
			{
				env.up ? env.volume++ : env.volume--;
				if (env.volume > 15) env.volume = 15;
				else if (env.volume < 0) env.volume = 0;

				env.timer = env.periode;
			}
		}
	}
}



void Audio::updateFrameSequencer()
{
	switch (frameSequencerStep)
	{
		case 2:
		case 6:
			updateSweep();
		case 0:
		case 4:
			for (int i = 0; i < NB_GB_CHANNEL; i++) if (channel[i].length > 0) channel[i].length--;
			break;
		case 7:
			updateEnvelopes();
			break;
	}

	frameSequencerStep++;
	if (frameSequencerStep >= 8) frameSequencerStep = 0;
}


char Audio::sample()
{
	if ((ram[IO_REGISTER | NR30] & 0x80) == 0) return 0; // DAC POWER	

	int i = 2;
	bool lenghtEnable = ram[IO_REGISTER | register4[i]] & 0x40;
	if ((channel[i].length && lenghtEnable) || !lenghtEnable)
	{
		ram[IO_REGISTER | NR52] |= 1 << i;
		char data = ((ram[IO_REGISTER | Wave_Pattern_RAM + ((sampleIndex%32) >> 1)]) >> (4* (sampleIndex%2))) & 0x0f;
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
	ram[IO_REGISTER | NR52] &= ~(1 << i);
	return 0;
}

char Audio::wave(char i)
{
	bool lenghtEnable = ram[IO_REGISTER | register4[i]] & 0x40;
	if ((channel[i].length && lenghtEnable) || !lenghtEnable)
	{
		ram[IO_REGISTER | NR52] |= 1 << i;
		char duty = ram[IO_REGISTER | register1[i]] >> 6;
		if ((waveForm[duty] >> (channel[i].dutyCycleCounter & 0x07)) & 1)
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}
	ram[IO_REGISTER | NR52] &= ~(1 << i);
	return 0;
}


char Audio::noiseSample()
{
	int i = 3;
	bool lenghtEnable = ram[IO_REGISTER | register4[i]] & 0x40;
	if ((channel[i].length && lenghtEnable) || !lenghtEnable)
	{
		ram[IO_REGISTER | NR52] |= 0x08;
		return (noise.LFSR & 1) ? -1 : 1;
	}
	ram[IO_REGISTER | NR52] &= 0xf7;
	return 0;
}

bool optionSquare0 = true;
bool optionSquare1 = true;
bool optionSample  = true;
bool noiseChannel  = true;

void Audio::imgui()
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

int Audio::mixTerminal(char terminal)
{
	int output = 0;
	char channelControl = (ram[IO_REGISTER | NR50] >> (terminal * 4)) & 0x0f;

	int volume = channelControl & 0x07;
	char enable = (ram[IO_REGISTER | NR51] >> (terminal * 4)) & 0x0f;
	if ((enable & 0x01) && !sweep.overflow && optionSquare0)
	{
		output += channel[0].envelope.volume * wave(0);
	}

	if ((enable & 0x02) && optionSquare1)
	{
		output += channel[1].envelope.volume * wave(1);
	}

	if ((enable & 0x04) && optionSample)
	{
		output += sample();
	}

	if ((enable & 0x08) && noiseChannel)
	{
		output += channel[3].envelope.volume * noiseSample();
	}

	output *= volume + 1;

	return output;
}

void Audio::update()
{
	frameSequencerAccu += TC;
	if (frameSequencerAccu >= 8192)
	{
		frameSequencerAccu -= 8192;
		updateFrameSequencer();
	}

	updateTimers(TC);


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


void Audio::setEnvelope(int i)
{
	channel[i].envelope.volume = ram[IO_REGISTER + register2[i]] >> 4;
	channel[i].envelope.up     =(ram[IO_REGISTER + register2[i]] >> 3) & 1;
	channel[i].envelope.timer  = ram[IO_REGISTER + register2[i]] & 0x07;
	channel[i].envelope.periode = channel[i].envelope.timer;
}

void SweepChannel::updateValue(unsigned char value)
{
	period = (value & 0x70) >> 4;
	shift = value & 7;
	way = (value & 8) ? -1 : 1;
}

void Audio::updateChannel(char i, unsigned char value)
{
	ram[IO_REGISTER | register4[i]] = value;

	if (value & 0x80)
	{
		if (i == 0)
		{
			channel[i].length = ram[IO_REGISTER | register1[i]] & 0x3F;
			sweep.shadowFreq = ram[IO_REGISTER | register3[i]] | ((value & 0x07) << 8);
			sweep.overflow = false;
			channel[i].timer = computeChannelPeriode(i);
			sweep.timer = sweep.period;
			setEnvelope(i);
		}
		else if (i == 1)
		{
			channel[i].length = ram[IO_REGISTER | register1[i]] & 0x3F;
			channel[i].timer = computeChannelPeriode(i);
			setEnvelope(i);
		}
		else if (i == 2)
		{
			channel[i].length = ram[IO_REGISTER | register1[i]];
			channel[i].timer = computeChannelPeriode(i);
			sampleIndex = 0;
		}
		else if (i == 3)
		{
			channel[i].length = ram[IO_REGISTER | register1[i]] & 0x3F;
			setEnvelope(i);
			noise.LFSR = 0xffff;
			channel[i].timer = noise.periode;
		}
	}
}

