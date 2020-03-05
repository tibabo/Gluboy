#include "al.h"
#include "alc.h"

ALCdevice *device;


void audioInit()
{
	device = alcOpenDevice(nullptr);

	if (!device)
		return;

	ALCenum error;
	error = alGetError();
	if (error != AL_NO_ERROR)
	{ }
		// something wrong happened
}