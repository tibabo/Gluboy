#pragma once



struct Envelope
{
	int volume;
	bool up;
	int timer;
	int periode;
};

struct Channel
{
	Envelope envelope;
	signed int timer = 0;
	signed int length = 0;
	char dutyCycleCounter = 0;
};

struct NoiseChannel
{
	unsigned short LFSR = 0xffff;
	bool sevenStage = false;
	int periode = 0;
	void shiftLFSR();
	void reloadTimer(unsigned char value);
};

struct SweepChannel
{
	char period = 0;
	char timer = 0;
	int shadowFreq = 0;
	int shift = 0;
	int way = 0;
	bool overflow = false;
	void updateValue(unsigned char value);
};

struct Audio
{
	Channel channel[4];
	NoiseChannel noise;
	SweepChannel sweep;
	int audioAccu = 0;
	int frameSequencerAccu = 0;
	int frameSequencerStep = 0;
	int sampleIndex = 0;


	void updateSweep();
	int  computeChannelPeriode(int i);
	void updateTimers(char clock);
	void updateEnvelopes();
	void updateFrameSequencer();
	char wave(char channel);
	char noiseSample();
	char sample();
	int  mixTerminal(char terminal);
	void setEnvelope(int channel);
public:
	void init();
	void update();
	void updateChannel(char channel, unsigned char value);
	void imgui();
};

extern Audio * audio;
