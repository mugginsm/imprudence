#include "llviewerprecompiledheaders.h"

#include "llhtmltexturesystem.h"

#include "llogremediaengine.h"
#include "llogrehtmlrenderer.h"

#include <OGRE/OgreExternalTextureSourceManager.h>


LLHtmlTextureSystem::LLHtmlTextureSystem()
{
    mDictionaryName = "html";
    mParamDictName = "html";

    Ogre::ExternalTextureSourceManager& mgr = Ogre::ExternalTextureSourceManager::getSingleton();
    mgr.setExternalTextureSource("http", this);
    mgr.setExternalTextureSource("https", this);
    mgr.setExternalTextureSource("file", this);
}

LLHtmlTextureSystem::~LLHtmlTextureSystem()
{

}

bool LLHtmlTextureSystem::initialise()
{
    addBaseParams();

    Ogre::ParamDictionary* dict = getParamDictionary();
    dict->addParameter(Ogre::ParameterDef("address", "Web page URL", Ogre::PT_STRING), &mCmdAddr);
    dict->addParameter(Ogre::ParameterDef("refreshrate", "Web page refresh rate", Ogre::PT_STRING), &mCmdRefreshRate);

    return true;
}

void LLHtmlTextureSystem::shutDown()
{
    cleanupDictionary();
}

void LLHtmlTextureSystem::createDefinedTexture(const Ogre::String& material, const Ogre::String& group)
{
    Ogre::String addr = getParameter("address");
    if (addr.empty())
        return;

    Ogre::String rateStr = getParameter("refreshrate");
    int rate = Ogre::StringConverter::parseInt(rateStr);
    // Note: high bit of rate determines whether it is per second or per minute
    if (rate < 0) rate = 0;
    if (rate == MEDIA_REFRESH_PER_MINUTE) rate = 0;

  // To enable e.g. flash plugin in Gecko (llmozlib), see
  // https://developer.mozilla.org/en/Gecko_Plugin_API_Reference/Plug-in_Basics#How_Gecko_Finds_Plug-ins
 /// \todo Currently a html surface is created either static or dynamic, but it cannot be switched after it is created. Debug&test that this works
 ///       as well after the update period UI is implemented.
  // float updateInterval = 1.f / 30.f;
	float updateInterval = -1.f; // Passing a value < 0 means no periodical updates.
    if ((rate > 0) && (rate < MEDIA_REFRESH_PER_MINUTE))
        updateInterval = 1.f / rate;
    if (rate > MEDIA_REFRESH_PER_MINUTE)
        updateInterval = 60.f / (rate-MEDIA_REFRESH_PER_MINUTE);

	LLOgreHtmlRenderer::createRenderer(material, addr, updateInterval);
}

void LLHtmlTextureSystem::destroyAdvancedTexture(const Ogre::String& material, const Ogre::String& group)
{
    LLOgreHtmlRenderer::releaseRenderer(material);
}

bool LLHtmlTextureSystem::requiresAuthorization() const
{
    return false;
}

void LLHtmlTextureSystem::setAddress(const Ogre::String& addr)
{
    mAddress = addr;
}

const Ogre::String& LLHtmlTextureSystem::getAddress() const
{
    return mAddress;
}

void LLHtmlTextureSystem::setRefreshRate(const Ogre::String& rate)
{
    mRefreshRate = rate;
}

const Ogre::String& LLHtmlTextureSystem::getRefreshRate() const
{
    return mRefreshRate;
}
