#include "TightVNCPluginFactory.h"
#include "TightVNCPlugin.h"

Ogre::Plugin* TightVNCPluginFactory::createPlugin()
{
    return new TightVNCPlugin;
}
