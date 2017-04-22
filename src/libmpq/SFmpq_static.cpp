/* License information for this code is in license.txt */

#include "SFmpq_static.h"

extern "C" BOOL APIENTRY SFMpqMain(HINSTANCE hInstDLL, DWORD  ul_reason_for_call, LPVOID lpReserved);

struct SFMPQLIBMODULE {
	SFMPQLIBMODULE();
	~SFMPQLIBMODULE();
} SFMpqLib;


SFMPQLIBMODULE::SFMPQLIBMODULE()
{
	SFMpqMain(0,DLL_PROCESS_ATTACH,0);
}

SFMPQLIBMODULE::~SFMPQLIBMODULE()
{
	SFMpqDestroy();
	SFMpqMain(0,DLL_PROCESS_DETACH,0);
}

void LoadSFMpq()
{
}

void FreeSFMpq()
{
}

