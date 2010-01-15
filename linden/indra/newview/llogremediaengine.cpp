// ----------------------------------------------------------------------------
//  TODO:
//      - Create/remove media url textures when assigned to objects
//      - Restore old texture when media url is removed
//
// ----------------------------------------------------------------------------

#include "llviewerprecompiledheaders.h"

#include "llogremediaengine.h"
#include "llexternaltexturesource.h"
#include "llimagegl.h"
#include "rexogrelegacymaterial.h"
#include "message.h"

#include "llagent.h"
#include "llimagej2c.h"
#include "llviewerimagelist.h"

#include "llviewergenericmessage.h"
#include "lldispatcher.h"

#include <OGRE/OgreExternalTextureSourceManager.h>

extern LLAgent gAgent;

LLOgreMediaEngine* LLOgreMediaEngine::mInstance = 0;

RexMediaUrlDispatchHandler LLOgreMediaEngine::smRexMediaUrlHandler;
const std::string RexMediaUrlDispatchHandler::key = "RexMediaUrl";

bool RexMediaUrlDispatchHandler::operator ()(const LLDispatcher *dispatcher, const std::string &key, const LLUUID &invoice, const LLDispatchHandler::sparam_t &string)
{
    // 1st string: image UUID
    // 2nd string: media URL
    // 3rd string: refresh rate
    if (string.size() >= 3)
    {
        if (!LLUUID::validate(string[0]))
        {
            llwarns << "Image UUID not valid" << llendl;
            return false;
        }

        LLOgreMediaEngine::ImageInfoPacket packet;
        packet.id = LLUUID(string[0]);
        packet.url = string[1];
        packet.refreshRate = Ogre::StringConverter::parseInt(string[2]);

        llinfos << "Got RexMediaUrl (" << packet.id.asString() << ", " << packet.url << "," << packet.refreshRate << ")" << llendl;

        LLOgreMediaEngine* me = LLOgreMediaEngine::getInstance();
        if (me) 
            me->addImagePacket(packet);
    }
    else
    {
        llwarns << "Not enough parameters" << llendl;
        return false;
    }

    return true;
}

LLOgreMediaEngine* LLOgreMediaEngine::getInstance()
{
	return mInstance;
}

LLOgreMediaEngine::LLOgreMediaEngine()
    : mQueueMutex(0)
{
    mQueueMutex = new LLMutex(0);
    Ogre::Root::getSingleton().addFrameListener(this);

    mInstance = this;

    gGenericDispatcher.addHandler(smRexMediaUrlHandler.getKey(), &smRexMediaUrlHandler);
}

LLOgreMediaEngine::~LLOgreMediaEngine()
{
    Ogre::Root* root = Ogre::Root::getSingletonPtr();
    if (root)
        root->removeFrameListener(this);

    delete mQueueMutex;
    mQueueMutex = 0;

    mInstance = 0;
}

BOOL LLOgreMediaEngine::isAvailable() const
{
	return TRUE;
}

std::string LLOgreMediaEngine::getSourceTypeFromUrl(const std::string& url) const
{
	size_t delimpos = url.find_first_of(':');
	if (delimpos == std::string::npos)
		return "";

	std::string type = url.substr(0, delimpos);
	if (type.empty())
        return std::string();

	return type;
}

BOOL LLOgreMediaEngine::isMediaUrlSupported(const std::string& url) const
{
  // QT locks up the html renderer, or at least our keyboard controls...
  if (url.length() >= 4)
  {                      
      if (url.substr(url.length()-4, 4) == ".mov") return FALSE;
  }
  if (url.length() >= 3)
  {
      if (url.substr(url.length()-3, 3) == ".qt") return FALSE;
  }                    

	const Ogre::String& type = getSourceTypeFromUrl(url);
	Ogre::ExternalTextureSourceManager& mgr = Ogre::ExternalTextureSourceManager::getSingleton();
	return (mgr.getExternalTextureSource(type) != 0);
}

BOOL LLOgreMediaEngine::requiresAuthorization(const std::string& url) const
{
    const Ogre::String& type = getSourceTypeFromUrl(url);
    Ogre::ExternalTextureSourceManager& mgr = Ogre::ExternalTextureSourceManager::getSingleton();
    LLExternalTextureSource* source = (LLExternalTextureSource*)mgr.getExternalTextureSource(type);
    if (source && source->requiresAuthorization())
        return TRUE;

    return FALSE;
}

void LLOgreMediaEngine::assignMediaUrl(LLViewerImage* const image, const std::string& url, int refreshRate)
{
    // Set refreshrate even if the url is not valid, so that it gets stored & can be edited
    image->setMediaRefreshRate(refreshRate);

	Ogre::ExternalTextureSourceManager& mgr = Ogre::ExternalTextureSourceManager::getSingleton();

  if (!isMediaUrlSupported(url))
  {
    llinfos << "Invalid image or url (" << url << ")" << llendl;
		return;
  }

	// Check if the image is valid and an external source is available
	const std::string& type = getSourceTypeFromUrl(url);
    if (type.empty() || (mgr.getExternalTextureSource(type) == 0) ||
        !image || !image->getOgreMaterial())
    {
        llinfos << "Invalid image or url (" << url << ")" << llendl;
		return;
    }

    image->getOgreMaterial()->setStatic(true);

	// Activate external texture plug in
	mgr.setCurrentPlugIn(type);

    // Pass media url to texture source
    Ogre::ExternalTextureSource* source = mgr.getCurrentPlugIn();
	source->setParameter("address", url);
    source->setParameter("refreshrate", Ogre::StringConverter::toString(refreshRate));

	// Tell the plug in to create a texture with the previous parameters to our material
	source->createDefinedTexture(image->getOgreMaterial()->getName());

    image->setMediaURL(url);

    // NOTE: this is useless for VNC, because VNC will create texture later; at this point the material still has original texture
    image->getOgreMaterial()->updateVariationsTexture();
}

void LLOgreMediaEngine::removeMediaUrl(LLViewerImage* const image)
{
    std::string url = image->getMediaURL();

    image->getOgreMaterial()->setStatic(false);

    const std::string& type = getSourceTypeFromUrl(url);
    Ogre::ExternalTextureSourceManager& mgr = Ogre::ExternalTextureSourceManager::getSingleton();
    mgr.setCurrentPlugIn(type);

    // Tell the plug in to destroy the texture that was previously created
    Ogre::ExternalTextureSource* source = mgr.getCurrentPlugIn();
    source->destroyAdvancedTexture(image->getOgreMaterial()->getName());

    image->setMediaURL(std::string());
}

void LLOgreMediaEngine::authorizeMediaPlayback(const LLUUID& id)
{
    IDMap::iterator i = mUnauthorizedIDs.find(id);
    if (i != mUnauthorizedIDs.end())
    {
        replaceImageWithMedia(i->first, i->second);
        mAuthorizedIDs.insert(*i);
        mUnauthorizedIDs.erase(i);
    }
    else
    {
        llinfos << "Can't find unauthorized media for image " << id << llendl;
    }
}

void LLOgreMediaEngine::unauthorizeMediaPlayback(const std::string& mediaURL)
{
    for (IDMap::iterator i = mAuthorizedIDs.begin(); i != mAuthorizedIDs.end(); ++i)
    {
        if (i->second == mediaURL)
        {
            mUnauthorizedIDs.insert(*i);
            mAuthorizedIDs.erase(i);
        }
    }
}

void LLOgreMediaEngine::replaceImageWithMedia(const LLUUID& id, const std::string& url)
{
    LLViewerImage* img = gImageList.getImage(id);
    if (img)
    {
        img->setMediaURL(url);
        if (isMediaUrlSupported(url))
            assignMediaUrl(img, url, img->getMediaRefreshRate());
        llinfos << "Image found, setting media url" << llendl;
    }
}

void LLOgreMediaEngine::processPacket(const LLOgreMediaEngine::ImageInfoPacket& packet)
{
    llinfos << "Processing " << packet.id << llendl;

    BOOL requiresAuth = requiresAuthorization(packet.url);

    LLViewerImage* img = gImageList.getImage(packet.id);
    if (img)
    {
        // Remove old media url
        if (!img->getMediaURL().empty())
        {
            removeMediaUrl(img);

            IDMap::iterator i = mAuthorizedIDs.find(packet.id);
            if (i != mAuthorizedIDs.end())
                mAuthorizedIDs.erase(i);

            i = mUnauthorizedIDs.find(packet.id);
            if (i != mUnauthorizedIDs.end())
                mUnauthorizedIDs.erase(i);
        }

        if (!packet.url.empty() && !requiresAuth)
        {
            assignMediaUrl(img, packet.url, packet.refreshRate);
        }
    }
    else
    {
        llinfos << "Couldn't find image " << packet.id << llendl;
    }

    if (requiresAuth && !packet.url.empty())
    {
        mUnauthorizedIDs.insert(std::make_pair(packet.id, packet.url));
    }
}

// ----------------------------------------------------------------------------

bool LLOgreMediaEngine::frameStarted(const Ogre::FrameEvent& e)
{
    LLMutexLock lock(mQueueMutex);
    while (!mPacketQueue.empty())
    {
        const ImageInfoPacket& packet = mPacketQueue.front();
        processPacket(packet);
        mPacketQueue.pop();
    }

    return true;
}

bool LLOgreMediaEngine::frameEnded(const Ogre::FrameEvent& e)
{
    return true;
}

// ----------------------------------------------------------------------------
// Network message handlers

void LLOgreMediaEngine::process_rex_image_info(LLMessageSystem* msg, void**)
{
    ImageInfoPacket packet;
    msg->getUUIDFast(_PREHASH_ImageInfo, _PREHASH_ImageID, packet.id);

    char mediaURL[128];
    msg->getStringFast(_PREHASH_ImageInfo, _PREHASH_MediaURL, 128, mediaURL);

    packet.url = mediaURL;
    packet.refreshRate = 0;

    llinfos << "Got RexImageInfo (" << packet.id.asString() << ", " << packet.url << ")" << llendl;

    LLOgreMediaEngine* me = getInstance();
    if (me)
        me->addImagePacket(packet);
}

void LLOgreMediaEngine::send_rex_image_info(const LLUUID& id, const std::string& mediaURL, int refreshRate)
{
    llinfos << id.asString() << ", " << mediaURL << ", " << refreshRate << llendl;

    std::vector<std::string> strings;
    strings.push_back(id.asString());
    strings.push_back(mediaURL);
    strings.push_back(Ogre::StringConverter::toString(refreshRate));
    send_generic_message("RexMediaUrl", strings);

    //LLOgreMediaEngine* me = getInstance();
    //if (me)
    //{
    //    LLViewerImage* img = gImageList.getImage(id);
    //    if (img)
    //    {
    //        gMessageSystem->newMessageFast(_PREHASH_RexImageInfo);
    //        gMessageSystem->nextBlockFast(_PREHASH_ImageInfo);
    //        gMessageSystem->addUUIDFast(_PREHASH_ImageID, id);
    //        gMessageSystem->addStringFast(_PREHASH_MediaURL, mediaURL);
    //        gMessageSystem->sendReliable(gAgent.getRegionHost());
    //        llinfos << "Sent to " << gAgent.getRegionHost().getHostName() << llendl;
    //    }
    //    else
    //    {
    //        llinfos << "Can't find image with id " << id.asString() << llendl;
    //    }
    //}
}

void LLOgreMediaEngine::addImagePacket(const ImageInfoPacket& packet)
{
    LLMutexLock lock(mQueueMutex);
    mPacketQueue.push(packet);
}