#ifndef LLOGREMEDIAENGINE_H
#define LLOGREMEDIAENGINE_H

#include "linden_common.h"
#include "lluuid.h"
#include "lldispatcher.h"

#include <OGRE/OgreFrameListener.h>

#include <map>
#include <queue>

#define MEDIA_REFRESH_PER_MINUTE 128

class LLImageGL;
class LLMessageSystem;
class LLMutex;
class LLViewerImage;

struct apr_pool_t;

//! reX: Dispatcher for extended mediaURLs
class RexMediaUrlDispatchHandler : public LLDispatchHandler
{
public:
   //! Handle dispatch
	virtual bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& string);

   virtual LLStringUtil getKey() const { return key ; }

   const static LLStringUtil key;
};

class LLOgreMediaEngine : public Ogre::FrameListener
{
public:
    struct ImageInfoPacket
    {
        LLUUID id;
        std::string url;
        int refreshRate;
    };
    LLOgreMediaEngine();
    ~LLOgreMediaEngine();

	BOOL isAvailable() const;
	BOOL isMediaUrlSupported(const std::string& url) const;
    BOOL requiresAuthorization(const std::string& url) const;

    void addImagePacket(const ImageInfoPacket& packet);
    void authorizeMediaPlayback(const LLUUID& id);
    void unauthorizeMediaPlayback(const std::string& mediaURL);

	void assignMediaUrl(LLViewerImage* const image, const std::string& url, int refreshRate);
    void removeMediaUrl(LLViewerImage* const image);

    static LLOgreMediaEngine* getInstance();

    static void process_rex_image_info(LLMessageSystem *msg, void **);
    static void send_rex_image_info(const LLUUID& id, const std::string& mediaURL, int refreshRate);



private:
    std::string getSourceTypeFromUrl(const std::string& url) const;
    void replaceImageWithMedia(const LLUUID& id, const std::string& url);
    void processPacket(const LLOgreMediaEngine::ImageInfoPacket& packet);

    bool frameStarted(const Ogre::FrameEvent& e);
    bool frameEnded(const Ogre::FrameEvent& e);

    typedef std::map<LLUUID, std::string> IDMap;
    IDMap mUnauthorizedIDs;
    IDMap mAuthorizedIDs;

    typedef std::queue<ImageInfoPacket> PacketQueue;
    PacketQueue mPacketQueue;
    LLMutex* mQueueMutex;

    static LLOgreMediaEngine* mInstance;

    static RexMediaUrlDispatchHandler smRexMediaUrlHandler;
};	//	class LLOgreMediaEngine

#endif	//	LLOGREMEDIAENGINE_H
