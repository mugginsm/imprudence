/*
#include "TightVNCPlugin.h"
#include <OGRE/OgreRoot.h>
#include <windows.h>

TightVNCPlugin* g_plugin = 0;

HINSTANCE g_hinst = 0;

BOOL WINAPI DllMain(HINSTANCE hDllInstance, DWORD dwReason, LPVOID pReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
		g_hinst  = hDllInstance;
	else if (dwReason == DLL_PROCESS_DETACH)
		g_hinst = 0;
	return true;
}

extern "C" void TIGHTVNC_PLUGIN_EXPORT dllStartPlugin()
{
	g_plugin = new TightVNCPlugin();
	Ogre::Root::getSingleton().installPlugin(g_plugin);
}
extern "C" void TIGHTVNC_PLUGIN_EXPORT dllStopPlugin()
{
	Ogre::Root::getSingleton().uninstallPlugin(g_plugin);
	delete g_plugin;
	g_plugin = 0;
}
*/