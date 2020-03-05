//
//  glouboy.h
//  imguiex-osx
//
//  Created by pinkasfeld on 30/11/2017.
//  Copyright © 2017 Joel Davis. All rights reserved.
//

#ifndef glouboy_h
#define glouboy_h

#include <stdio.h>

extern unsigned char ram[0x10000];
extern unsigned char * rom;
extern unsigned char TC;
namespace Glouboy {
	void init();
	void update();
	void execute();

}
void jumpToVector(int vector);
void wakeHalteMode();

#endif /* glouboy_h */
