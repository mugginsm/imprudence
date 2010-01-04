#include "TightVNCConnection.h"
#include "TightVNCTextureSystem.h"
#include "../llcommon/llerror.h"

#include <vncviewer.h>

#include <OGRE/OgreHardwarePixelBuffer.h>
#include <OGRE/OgreRoot.h>
#include <OGRE/OgreLogManager.h>
#include <OGRE/OgreStringConverter.h>
#include <OGRE/OgreTextureManager.h>


PBITMAPINFO CreateBitmapInfo(HBITMAP hBmp);

static const float VNC_TEXTURE_UPDATE_DELAY = 0.5;
int TightVNCConnection::mTexIDCounter = 0;

// ----------------------------------------------------------------------------

TightVNCConnection::TightVNCConnection(int id, TightVNCTextureSystem* textureSystem,
                                       const std::string& host, int port)
    : mUpdateTimer(VNC_TEXTURE_UPDATE_DELAY)
    , mScreenDirty(false)
    , mHost(host)
    , mPort(port)
    , mConn(0)
    , mTextureSystem(textureSystem)
    , mID(id)
    , mAlive(true)
    , mSafeToDelete(true)
{
    Ogre::Root::getSingleton().addFrameListener(this);
}

TightVNCConnection::~TightVNCConnection()
{
    while (!mSafeToDelete) {}

    if (mConn)
    {
        // NOTE: Connection can't be deleted here! see omni_thread::~omni_thread
        mConn->RemoveUpdateListener(this);
        mConn->KillThread();
//         delete mConn;
        mConn = 0;
    }

    mTextureSystem->connectionClosed(mID);
    mTextureSystem = 0;
}

bool TightVNCConnection::connect(VNCviewerApp* app)
{
    mConn = app->NewConnection((TCHAR*)mHost.c_str(), mPort);

    if (mConn)
    {
        mConn->AddUpdateListener(this);
        return true;
    }

    return false;
}

bool TightVNCConnection::isConnected() const
{
    return (mConn != 0);
}

bool TightVNCConnection::isAlive() const
{
    if (mAlive && mConn)
        return mConn->IsRunning();
    else
        return false;
}


void TightVNCConnection::destroy()
{
    mSafeToDelete = false;
    mAlive = false;
}

void TightVNCConnection::textureReceived()
{
    Ogre::String textureName = "TightVNCTexture" + Ogre::StringConverter::toString(mTexIDCounter++);

    Ogre::TextureManager& tmgr = Ogre::TextureManager::getSingleton();
    mTexture = tmgr.createManual(textureName,
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
        mWidth, mHeight, 0, Ogre::PF_R8G8B8, Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);

    mTextureSystem->connectionCreated(mID, textureName);
}

void TightVNCConnection::textureResized()
{
    Ogre::LogManager::getSingleton().logMessage("TightVNCConnection - Reloading VNC texture (resized)");

    Ogre::String name = mTexture->getName();
    Ogre::TextureManager& tmgr = Ogre::TextureManager::getSingleton();
//     tmgr.unload(m_pInfo->m_texture->getHandle());
    tmgr.remove(mTexture->getHandle());

    mTexture = tmgr.createManual(name,
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
        mWidth, mHeight, 0, Ogre::PF_R8G8B8, Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
}

std::string TightVNCConnection::getURL() const
{
    return "vnc://" + getHost() + ":" + Ogre::StringConverter::toString(getPort());
}


// ----------------------------------------------------------------------------
// VNC ScreenUpdateListener callback

void TightVNCConnection::screenUpdated(HDC dc, HBITMAP bitmap)
{
    omni_mutex_lock lock(mUpdateMutex);

    PBITMAPINFO info = CreateBitmapInfo(bitmap);
    if (!info)
        return;

    mScreen.reset(new BYTE[info->bmiHeader.biSizeImage]);
    if (!GetDIBits(dc, bitmap, 0, info->bmiHeader.biHeight, mScreen.get(), info, DIB_RGB_COLORS))
    {
        mScreen.reset(0);
        return;
    }

    mWidth = info->bmiHeader.biWidth;
    mHeight = info->bmiHeader.biHeight;

    mScreenDirty = true;
}

// ----------------------------------------------------------------------------
// Ogre FrameListener callbacks

bool TightVNCConnection::frameStarted(const Ogre::FrameEvent& e)
{
    omni_mutex_lock lock(mUpdateMutex);

    if (!mAlive)
        return true;

    // Allocate new texture if data is available and texture hasn't been created
    if (mScreen && mTexture.isNull())
        textureReceived();

    // If no VNC data has been received yet
    if (!mScreen || mTexture.isNull())
        return true;

    // Check for resize
    if ((mWidth != mTexture->getWidth()) || (mHeight != mTexture->getHeight()))
        textureResized();

    mUpdateTimer -= e.timeSinceLastFrame;
    if (mUpdateTimer > 0)
        return true;

    mUpdateTimer = VNC_TEXTURE_UPDATE_DELAY;

    // Update texture

    if (mScreenDirty)
    {
        Ogre::HardwarePixelBufferSharedPtr pbuf = mTexture->getBuffer();
        pbuf->lock(Ogre::HardwareBuffer::HBL_DISCARD);
        const Ogre::PixelBox& pixelBox = pbuf->getCurrentLock();
        memcpy(pixelBox.data, mScreen.get(), pixelBox.getConsecutiveSize());
        pbuf->unlock();
        mScreenDirty = false;
    }

   return true;
}

bool TightVNCConnection::frameEnded(const Ogre::FrameEvent& e)
{
    omni_mutex_lock lock(mUpdateMutex);

    if (!mAlive)
    {
        Ogre::Root::getSingletonPtr()->removeFrameListener(this);
        mSafeToDelete = true;
        return true;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Taken from MSDN

PBITMAPINFO CreateBitmapInfo(HBITMAP hBmp)
{
    BITMAP bmp; 
    PBITMAPINFO pbmi; 
    WORD    cClrBits; 

    // Retrieve the bitmap color format, width, and height. 
    if (!GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmp)) 
        return 0;

    // Convert the color format to a count of bits. 
    cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel); 
    if (cClrBits == 1) 
        cClrBits = 1; 
    else if (cClrBits <= 4) 
        cClrBits = 4; 
    else if (cClrBits <= 8) 
        cClrBits = 8; 
    else if (cClrBits <= 16) 
        cClrBits = 16; 
    else if (cClrBits <= 24) 
        cClrBits = 24; 
    else cClrBits = 32; 

    // Allocate memory for the BITMAPINFO structure. (This structure 
    // contains a BITMAPINFOHEADER structure and an array of RGBQUAD 
    // data structures.) 

     if (cClrBits != 24) 
         pbmi = (PBITMAPINFO) LocalAlloc(LPTR, 
                    sizeof(BITMAPINFOHEADER) + 
                    sizeof(RGBQUAD) * (1<< cClrBits)); 

     // There is no RGBQUAD array for the 24-bit-per-pixel format. 

     else 
         pbmi = (PBITMAPINFO) LocalAlloc(LPTR, 
                    sizeof(BITMAPINFOHEADER)); 

    // Initialize the fields in the BITMAPINFO structure. 

    pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER); 
    pbmi->bmiHeader.biWidth = bmp.bmWidth; 
    pbmi->bmiHeader.biHeight = bmp.bmHeight; 
    pbmi->bmiHeader.biPlanes = bmp.bmPlanes; 
    pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel; 
    if (cClrBits < 24) 
        pbmi->bmiHeader.biClrUsed = (1<<cClrBits); 

    // If the bitmap is not compressed, set the BI_RGB flag. 
    pbmi->bmiHeader.biCompression = BI_RGB; 

    // Compute the number of bytes in the array of color 
    // indices and store the result in biSizeImage. 
    // For Windows NT, the width must be DWORD aligned unless 
    // the bitmap is RLE compressed. This example shows this. 
    // For Windows 95/98/Me, the width must be WORD aligned unless the 
    // bitmap is RLE compressed.
    pbmi->bmiHeader.biSizeImage = ((pbmi->bmiHeader.biWidth * cClrBits +31) & ~31) /8
                                  * pbmi->bmiHeader.biHeight; 
    // Set biClrImportant to 0, indicating that all of the 
    // device colors are important. 
     pbmi->bmiHeader.biClrImportant = 0; 
     return pbmi; 
}
