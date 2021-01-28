#pragma once

struct Timer
{
	unsigned short internalDIV = 0xABCC;
	int accu = 0;
public:
	void update();
	int  divWrite();
};

extern Timer * timer;