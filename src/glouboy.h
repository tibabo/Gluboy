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


extern unsigned char * rom;
namespace Glouboy {
	void init(const char * path);
	void init(unsigned char * rom, int size, const char * path);
	void update();
	void execute();
	void close();

}

#endif /* glouboy_h */
