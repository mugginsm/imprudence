#ifndef _TIGHTVNCUPDATER_H_
#define _TIGHTVNCUPDATER_H_

#include <VNCScreenUpdateListener.h>

#include <omnithread.h>
#include <OGRE/OgreFrameListener.h>
#include <OGRE/OgreTexture.h>
#include <boost/scoped_array.hpp>

#include <string>

class ClientConnection;
class TightVNCTextureSystem;
class VNCviewerApp;

/**
 *  A class that updates and creates VNC textures for one VNC connection.
 *  All textures will be updated in Ogre main thread.
 */
class TightVNCConnection
    : public Ogre::FrameListener
    , public VNCScreenUpdateListener
{
public:
    TightVNCConnection(int id, TightVNCTextureSystem* textureSystem,
                       const std::string& host, int port);
    ~TightVNCConnection();

    const Ogre::String& getHost() const { return mHost; }
    int getPort() const { return mPort; }

    std::string getURL() const;

    Ogre::TexturePtr getTexture() const { return mTexture; }

    bool connect(VNCviewerApp* app);
    bool isConnected() const;

    bool isAlive() const;
    void destroy();

private:
    void textureResized();
    void textureReceived();
    void release();

    // ScreenUpdateListener callback
    void screenUpdated(HDC dc, HBITMAP bitmap);

    // FrameListener callbacks
    bool frameStarted(const Ogre::FrameEvent& e);
    bool frameEnded(const Ogre::FrameEvent& e);

private:
    static int mTexIDCounter;

    ClientConnection* mConn;

    omni_mutex mUpdateMutex;
    Ogre::Real mUpdateTimer;

    bool mScreenDirty;

    Ogre::TexturePtr mTexture;
    boost::scoped_array<unsigned char> mScreen;
    long mWidth, mHeight;

    std::string mHost;
    int mPort;

    TightVNCTextureSystem* mTextureSystem;

    int mID;

    bool mAlive;
    volatile bool mSafeToDelete;

};  //  class TightVNCConnection

#endif  //  _TIGHTVNCUPDATER_H_
