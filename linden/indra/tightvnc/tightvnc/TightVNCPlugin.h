#ifndef _TIGHTVNCPLUGINIMPL_H_
#define _TIGHTVNCPLUGINIMPL_H_

#include <OGRE/OgrePlugin.h>

#include <omnithread.h>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

class ClientConnection;
class TightVNCConnection;
class TightVNCTextureSystem;
class VNCviewerApp32;

class TightVNCPlugin
    : public Ogre::Plugin
{
public:
    TightVNCPlugin();

    /**
    *  Creates a new VNC connection asynchronously
    *  @param host VNC server host name
    *  @param port VNC server port
    *  @return Connection id
    */
    int newConnection(const Ogre::String& host, int port);

    /**
     *	@param id Connection id
     */
    void closeConnection(int id);

    /**
    *  Retrieves an existing VNC connection by host name and port
    *  @param host VNC server host name
    *  @param port VNC server port
    *  @return Connection id
    */
    int lookupConnection(const Ogre::String& host, int port);

    /**
    *  Returns the VNC texture for the given connection
    *  @param id VNC connection id
    *  @return VNC texture or a NULL TexturePtr if the texture is not yet available
    */
    Ogre::TexturePtr getTextureForConnection(const int id) const;

protected:
    const Ogre::String& getName() const;
    void install();
    void initialise();
    void shutdown();
    void uninstall();

private:
    static unsigned long __stdcall vncThreadRun(void* params);

    boost::scoped_ptr<VNCviewerApp32> mVNCApp;

    Ogre::String mName;
    TightVNCTextureSystem* mTextureSystem;

    typedef boost::shared_ptr<TightVNCConnection> ConnectionPtr;
    typedef std::map<int, ConnectionPtr> ConnectionById;
    ConnectionById mConnByID;

    int mConnIDCounter;
    mutable omni_mutex mConnMutex;

    bool mRunning;
    void* mVNCThreadHandle;

};	//	class TightVNCPlugin


#endif	//	_TIGHTVNCPLUGINIMPL_H_
