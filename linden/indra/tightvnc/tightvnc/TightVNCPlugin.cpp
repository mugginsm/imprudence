#include <winsock2.h>

#include "TightVNCPlugin.h"
#include "TightVNCConnection.h"
#include "TightVNCTextureSystem.h"

#include "llogremediaengine.h"

#include <OGRE/OgreLogManager.h>
#include <OGRE/OgreRoot.h>
#include <OGRE/OgreStringConverter.h>

#include <Exception.h>
#include <HotKeys.h>
#include <vncviewer.h>
#include <VNCviewerApp32.h>
#include <VNCHelp.h>

TightVNCPlugin::TightVNCPlugin()
    : mName("TightVNC")
    , mTextureSystem(0)
    , mConnIDCounter(0)
{
}

const Ogre::String& TightVNCPlugin::getName() const
{
    return mName;
}

void TightVNCPlugin::install()
{
}

void TightVNCPlugin::initialise()
{
    mRunning = true;
    mVNCThreadHandle = CreateThread(0, 0, &TightVNCPlugin::vncThreadRun, (void*)this, 0, 0);

    mTextureSystem = new TightVNCTextureSystem(this);
}

void TightVNCPlugin::shutdown()
{
    mRunning = false;
    WaitForSingleObject(mVNCThreadHandle, 5000);
    mConnByID.clear();
    delete mTextureSystem;
    mTextureSystem = 0;
}

void TightVNCPlugin::uninstall()
{
}

int TightVNCPlugin::lookupConnection(const Ogre::String& host, int port)
{
    Ogre::LogManager::getSingleton().logMessage("TightVNCPlugin::lookupConnection");
//     omni_mutex_lock l(m_connMutex);

    for (ConnectionById::const_iterator i = mConnByID.begin(); i != mConnByID.end(); ++i)
    {
        if (i->second->getHost() == host && i->second->getPort() == port)
            return i->first;
    }

    return -1;
}

int TightVNCPlugin::newConnection(const Ogre::String& host, const int port)
{
    omni_mutex_lock l(mConnMutex);

    Ogre::LogManager::getSingleton().logMessage("TightVNCPlugin - Creating new connection for '" +
        host + ":" + Ogre::StringConverter::toString(port) + "'");

    // Set connection host and port and map it by id
    // The connection will be established asynchronously in VNC main thread
    // so it won't block application threads
    int id = mConnIDCounter++;
    ConnectionPtr conn(new TightVNCConnection(id, mTextureSystem, host, port));
    mConnByID.insert(std::make_pair(id, conn));

    return id;
}

void TightVNCPlugin::closeConnection(int id)
{
    omni_mutex_lock l(mConnMutex);
    Ogre::LogManager::getSingleton().logMessage("TightVNCPlugin::closeConnection");
    ConnectionById::iterator i = mConnByID.find(id);
    if (i != mConnByID.end())
    {
        i->second->destroy();
    }
}

Ogre::TexturePtr TightVNCPlugin::getTextureForConnection(const int id) const
{
    //omni_mutex_lock l(m_connMutex);
    //Ogre::LogManager::getSingleton().logMessage("TightVNCPlugin - Retrieving texture for connection " +
    //    Ogre::StringConverter::toString(id));

    ConnectionById::const_iterator i = mConnByID.find(id);
    if (i != mConnByID.end())
        return i->second->getTexture();

    // Not found
    return Ogre::TexturePtr();
}

// ----------------------------------------------------------------------------
// VNC Thread

unsigned long __stdcall TightVNCPlugin::vncThreadRun(void* params)
{
    TightVNCPlugin* plugin = (TightVNCPlugin*)params;

    // Create VNC app

    plugin->mVNCApp.reset(new VNCviewerApp32((HINSTANCE)GetVNCModule(), ""));

    // Enter VNC main loop

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (plugin->mRunning)
    {
        try
        {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) &&
                !GetVNCHotkeys().TranslateAccel(&msg) &&
                !GetVNCHelp().TranslateMsg(&msg) &&
                !plugin->mVNCApp->ProcessDialogMessage(&msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                continue;
            }

            // Go through connection info's and create new connections for uninitiated connections
            // This must be done in a separate thread so Ogre main thread won't stall
            {
                omni_mutex_lock lock(plugin->mConnMutex);
                ConnectionById::iterator i = plugin->mConnByID.begin();
                while (i != plugin->mConnByID.end())
                {
                    ConnectionPtr& conn = i->second;
                    if (!conn->isConnected())
                    {
                        if (!conn->connect(plugin->mVNCApp.get()))
                        {
                            Ogre::LogManager::getSingleton().logMessage("Removing connection with id '" +
                                Ogre::StringConverter::toString(i->first) + "' (unable to establish connection)");
                            conn->destroy();
                            LLOgreMediaEngine::getInstance()->unauthorizeMediaPlayback(conn->getURL());
                            i = plugin->mConnByID.erase(i);
                            continue;
                        }
                    }
                    else if (!conn->isAlive())
                    {
                        Ogre::LogManager::getSingleton().logMessage("Removing dead connection with id '" +
                            Ogre::StringConverter::toString(i->first) + "'");
                        LLOgreMediaEngine::getInstance()->unauthorizeMediaPlayback(conn->getURL());
              //          i = plugin->mConnByID.erase(i);
                        continue;
                    }
                    ++i;
                }
            }

            Sleep(50);
        }
        catch (...)
        {
            Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "Unhandled exception in VNC main thread");
        }
    }

    WSACleanup();

    Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "Shutting down VNC main thread...");

    return 0;
}
