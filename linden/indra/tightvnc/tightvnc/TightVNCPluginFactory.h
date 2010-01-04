#ifndef _TIGHTVNCPLUGIN_H_
#define _TIGHTVNCPLUGIN_H_

namespace Ogre
{
    class Plugin;
}

class TightVNCPluginFactory
{
public:
    static Ogre::Plugin* createPlugin();

};	//	class TightVNCPluginFactory


#endif	//	_TIGHTVNCPLUGIN_H_
