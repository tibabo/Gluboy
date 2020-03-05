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

#define imguiRegister(a) {int charRegister = a; if(ImGui::InputInt(#a,&charRegister, 0, 0, ImGuiInputTextFlags_CharsHexadecimal)){ a = charRegister;} }

namespace Glouboy {
	void init();
	void update();
	void execute();

}
void jumpToVector(int vector);
void wakeHalteMode();

#endif /* glouboy_h */
