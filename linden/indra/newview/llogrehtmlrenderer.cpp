/** 
 * @file llogrehtmlrenderer.cpp
 * @brief LLOgreHtmlRenderer class implementation
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#include "llviewerprecompiledheaders.h"

#include "llogre.h"
#include "llogrehtmlrenderer.h"

// Non-Windows platforms are case-sensitive
#include "llappviewer.h"
#include "llviewerwindow.h"
#include "llfocusmgr.h"
#include "lluictrlfactory.h"
#include "rexogrelegacymaterial.h"
#include "llwebbrowserctrl.h"
#include "llviewercontrol.h"
#include "llviewerimage.h"

#include "../llmedia/llmediaimplllmozlib.h"

LLOgreHtmlRenderer* LLOgreHtmlRenderer::sInstance = NULL;
LLOgreHtmlRenderer::RendererList LLOgreHtmlRenderer::sRenderers;
LLOgreHtmlRenderer::RendererList LLOgreHtmlRenderer::sAllRenderers;
LLFrameTimer LLOgreHtmlRenderer::sElapsedTime;
U32 LLOgreHtmlRenderer::sId = 0;


void LLOgreHtmlObserver::onNavigateComplete( const EventType& eventIn )
{
   if (mRenderer)
   {
		// This handler is called before llmozlib really redraws the html webpage client window. It has only set the mNeedsUpdate flag to true, so
		// we need to make the redraw here & now to access the proper page data.
		// Force the update of the media.
		LLMediaImplLLMozLib *media = dynamic_cast<LLMediaImplLLMozLib*>(mRenderer->getControl()->getMediaSource());
		assert(media);
		if (media)
			media->updateMedia();

      LLOgreRenderer::getPointer()->setOgreContext();
      mRenderer->_render();
      LLOgreRenderer::getPointer()->setSLContext();
   }
}

//! constructor
LLOgreHtmlRenderer::LLOgreHtmlRenderer(const std::string &name, const std::string &material, const std::string &url, float updateInterval)
: LLFloater(name, "FloaterWebTestRect", "Rex's amazing Web Browzer!")
   , mBrowserDepth(0)
   , mBrowserWidth(0)
   , mBrowserHeight(0)
   , mUpdateInterval(updateInterval)
   , mPlaneEnt(0)
   , mPlaneSN(0)
   , mExternalMaterial(false)
   , mHidden(true)
{
   llassert(LLOgreRenderer::getPointer()->isInitialized());


   LLUICtrlFactory::getInstance()->buildFloater(this, "floater_webtest.xml");

   mWebBrowser = getChild<LLWebBrowserCtrl>("web_control");
   this->setName(name);
   
   if (url.empty() == false)
      mWebBrowser->navigateTo(url);


   //if (gSavedSettings.getS32("HtmlRendererTextureWidth") == 0)
   //   gSavedSettings.setS32("HtmlRendererTextureWidth", 1024);
   //if (gSavedSettings.getS32("HtmlRendererTextureHeight") == 0)
   //   gSavedSettings.setS32("HtmlRendererTextureHeight", 1024);

   llassert(mWebBrowser != 0);

   LLWebBrowserTexture *texture = mWebBrowser->getTexture();
   texture->resize(1024, 1024);

   mBrowserWidth = texture->getBrowserWidth();
   mBrowserHeight = texture->getBrowserHeight();

   LLOgreRenderer::getPointer()->setOgreContext();
   // Create ogre material

   if (Ogre::MaterialManager::getSingleton().resourceExists(material))
   {
      mMaterial = Ogre::MaterialManager::getSingleton().getByName(material);
      createTexture(mBrowserWidth, mBrowserHeight);
      mExternalMaterial = true;
   } else
   {
      mExternalMaterial = false;
      // Create test plane to see web content
      std::string materialName = name;
      materialName.append("_material");
      mMaterial = Ogre::MaterialManager::getSingleton().create(materialName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
      mMaterial->getTechnique(0)->getPass(0)->setLightingEnabled(false);
      createTexture(mBrowserWidth, mBrowserHeight);
      
      std::string planeName = name;
      planeName.append("_plane");
      Ogre::Plane plane(Ogre::Vector3::UNIT_Y, 90);
      mPlaneMesh = Ogre::MeshManager::getSingleton().createPlane(planeName,
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, plane,
            50,50,1,1,true,1,1,1,-Ogre::Vector3::UNIT_Z);
      std::string planeEntName = name;
      planeEntName.append("_plane_ent");
      mPlaneEnt = LLOgreRenderer::getPointer()->getSceneMgr()->createEntity(planeEntName, planeName);
	   mPlaneEnt->setMaterialName(materialName);
      mPlaneSN = LLOgreRenderer::getPointer()->getSceneMgr()->getRootSceneNode()->createChildSceneNode();
      mPlaneSN->attachObject(mPlaneEnt);
      mPlaneSN->setPosition(Ogre::Vector3(-50+Ogre::Math::RangeRandom(-140, 140), 0, -80));
   }
   LLOgreRenderer::getPointer()->setSLContext();

   center();
   
   // add to list of renderers, so this gets rendered automatically
   sRenderers.push_back(this);

	// Only static renderables get an observer registered.
//   if (isStatic())
   {
      mObserver.setRenderer(this);
      mWebBrowser->addObserver(&mObserver);
   }

	sInstance = this;
}

//! destructor
LLOgreHtmlRenderer::~LLOgreHtmlRenderer()
{
	mWebBrowser->remObserver(&mObserver);

   // Remove temp client-side only stuff
   if (mPlaneSN && mPlaneEnt)
   {
      mPlaneSN->detachAllObjects(); // is this necessary?
      mPlaneSN->removeAndDestroyAllChildren();
		mPlaneSN = 0;
      LLOgreRenderer::getPointer()->getSceneMgr()->destroyEntity(mPlaneEnt);
		mPlaneEnt = 0;
      Ogre::MeshManager::getSingleton().remove(static_cast<Ogre::ResourcePtr>(mPlaneMesh));
      mPlaneMesh.setNull();
   }
   
   if (mExternalMaterial == false)
   {
      // Remove materials and textures
      Ogre::MaterialManager::getSingleton().remove(static_cast<Ogre::ResourcePtr>(mMaterial));
      mMaterial.setNull();
   }

   if (mTexture.isNull() == false)
      Ogre::TextureManager::getSingleton().remove(static_cast<Ogre::ResourcePtr>(mTexture));

   mTexture.setNull();
	mMaterial.setNull();
	mWebBrowser = 0;
   
   sInstance = NULL;

   if (hasMouseCapture())
	{
		llwarns << "Control deleted holding mouse capture.  Mouse capture removed." << llendl;
		gFocusMgr.removeMouseCaptureWithoutCallback(this);
	}

	RendererList::iterator iter = std::find(sRenderers.begin(), sRenderers.end(), this);
	if (iter != sRenderers.end())
		sRenderers.erase(iter);
}

void LLOgreHtmlRenderer::setUrl(const std::string& url)
{
   if (url.empty() == false)
      mWebBrowser->navigateTo(url);
}

void LLOgreHtmlRenderer::draw()
{
    if (!mHidden)
	    LLFloater::draw();
}

//! Render html into texture
void LLOgreHtmlRenderer::_render()
{
	assert(this);

    const unsigned char *pixels = mWebBrowser->getMediaSource()->getMediaData();

    S32 dataDepth = mWebBrowser->getMediaSource()->getMediaDepth();

    if (!dataDepth) 
    {
        llwarns << "Zero data depth" << llendl;
        return;
    }

    S32 dataWidth = mWebBrowser->getMediaSource()->getMediaDataWidth();
    S32 dataHeight = mWebBrowser->getMediaSource()->getMediaDataHeight();

    if (dataHeight < mBrowserHeight)
    {
        llwarns << "Data height less than browser height" << llendl;
        return;
    }

    if (dataWidth < mBrowserWidth)
    {
        llwarns << "Data width less than browser width" << llendl;
        return;
    }

    // Browser depth hasn't changed.  
    if (mBrowserDepth != dataDepth)
    {
        //! \todo this needs to be handled
        llwarns << "Browser depth changed" << llendl;
        // Browser depth has changed -- need to recreate texture to match.
        createTexture(mBrowserWidth, mBrowserHeight);
    }

    setTexture(pixels, dataWidth, mBrowserHeight);

    //llinfos << "Rendering w:" << dataWidth << " browser width:" << mBrowserWidth << " h:" << mBrowserHeight << llendl;
}


bool LLOgreHtmlRenderer::isStatic() const
{
   return mUpdateInterval <= 0.f;
}

void LLOgreHtmlRenderer::show()
{
   if (sId > 50000)
      return;

   sId++;
 //  if (!sInstance)
//	{
   std::string name = "webtest";
   std::string number;
   std::stringstream ss;
   ss << sId;
   ss >> number;
   name.append(number);
   sInstance = new LLOgreHtmlRenderer(name);
//	}

   sInstance->open();  /*Flawfinder: ignore*/
   sInstance->setVisible(false);
}

LLOgreHtmlRenderer *LLOgreHtmlRenderer::fromUrl(LLViewerImage *image, const std::string &url, float updateInterval)
{
   image->getOgreMaterial()->setStatic(true);
   return createRenderer(image->getOgreMaterial()->getName(), url, updateInterval);
}

LLOgreHtmlRenderer *LLOgreHtmlRenderer::createRenderer(const std::string& materialName, const std::string &url, float updateInterval)
{
    // First try to find renderer already rendering to this material   
    for (RendererList::iterator i = sRenderers.begin(); i != sRenderers.end(); ++i)
    {
        LLOgreHtmlRenderer* renderer = *i;
        Ogre::MaterialPtr material = renderer->mMaterial;
        if (!material.isNull() && material->getName() == materialName)
        { 
            renderer->setUrl(url);
            return renderer;
        }
    }

    sId++;
    std::string name = "webtest";
    name.append(Ogre::StringConverter::toString(sId));
    sInstance = new LLOgreHtmlRenderer(name, materialName, url, updateInterval);

    sInstance->open();  /*Flawfinder: ignore*/
    sInstance->setVisible(false);

    return sInstance;
}

void LLOgreHtmlRenderer::releaseRenderer(const std::string& materialName)
{
	LLOgreHtmlRenderer *renderer = 0;

	for (RendererList::iterator i = sRenderers.begin(); i != sRenderers.end(); ++i)
		if ((*i)->mMaterial->getName() == materialName)
		{
			renderer = *i;
			break;
		}

	delete renderer;
}

void LLOgreHtmlRenderer::releaseAll()
{
	for (RendererList::iterator i = sRenderers.begin(); i != sRenderers.end(); ++i)
		delete *i;

	sRenderers.clear();
}

//! Render all texture html renderers
void LLOgreHtmlRenderer::render()
{
   for (RendererList::iterator i = sRenderers.begin(); i != sRenderers.end(); ++i)
   {
		LLOgreHtmlRenderer *renderer = *i;
		if (renderer->isStatic())
			continue;

		renderer->mUpdateCounter += sElapsedTime.getElapsedTimeF32();
		if (renderer->mUpdateCounter >= renderer->mUpdateInterval)
		{
			renderer->mUpdateCounter = 0.f;
			LLMediaImplLLMozLib *media = dynamic_cast<LLMediaImplLLMozLib*>(renderer->getControl()->getMediaSource());
			assert(media);
			if (media)
			{
				media->forceUpdateMedia();
				(*i)->_render();
			}
		}
   }

   sElapsedTime.reset();
}

bool LLOgreHtmlRenderer::mouseDown(S32 x, S32 y, MASK mask)
{
    for (RendererList::const_iterator i = sRenderers.begin(); i != sRenderers.end() ; ++i)
    {
       (*i)->_mouseDown(x, y, mask);
    }

    if (sRenderers.empty())
      return false;

    return true;
}

bool LLOgreHtmlRenderer::mouseUp(S32 x, S32 y, MASK mask)
{
    for (RendererList::const_iterator i = sRenderers.begin(); i != sRenderers.end(); ++i)
    {
        (*i)->_mouseUp(x, y, mask);
    }

    if (sRenderers.empty())
      return false;

    return true;
}

void LLOgreHtmlRenderer::setTexture(const unsigned char *data, S32 width, S32 height)
{
   llassert(mMaterial.isNull() == false);
   llassert(mTexture.isNull() == false);

   Ogre::HardwarePixelBufferSharedPtr pixelBuffer = mTexture->getBuffer();

   // Lock the pixel buffer. We can use HBL_DISCARD because we don't need to save/read the data back
   pixelBuffer->lock(Ogre::HardwareBuffer::HBL_DISCARD);
   const Ogre::PixelBox &pixelBox = pixelBuffer->getCurrentLock();
   U8* pDest = static_cast<U8*>(pixelBox.data);

   S32 texWidth = gSavedSettings.getS32("HtmlRendererTextureWidth");
   S32 texHeight = gSavedSettings.getS32("HtmlRendererTextureHeight");

   if (isStatic() && (texWidth != width || texHeight != height))
   {
      Ogre::PixelBox src(Ogre::Box(0, 0, width, height), pixelBox.format, (void*)data);
//      Ogre::PixelBox dst(Ogre::Box(0, 0, texWidth, texHeight), pixelBox.format, 0);
      Ogre::Image::scale(src, pixelBox);
   } else
   {
      memcpy(pDest, data, pixelBuffer->getSizeInBytes());
   }

   //llwarns << "pb size: " << pixelBuffer->getSizeInBytes() << "  tex width: " << mTexture->getWidth() << " th: " << mTexture->getHeight() << " bw: " << width << " bh " << height << llendl;

   // Unlock the pixel buffer
   pixelBuffer->unlock();
}

//! Create new dynamic texture
void LLOgreHtmlRenderer::createTexture(S32 width, S32 height)
{
   llassert(mMaterial.isNull() == false);
   mMaterial->getTechnique(0)->getPass(0)->removeAllTextureUnitStates();

   if (mTexture.isNull() == false)
      Ogre::TextureManager::getSingleton().remove(static_cast<Ogre::ResourcePtr>(mTexture));

   mTexture.setNull();


 //  // calculate new dimensions for texture
 //  F32 scale_ratio = 1.f;
	//if (width > MAX_DIMENSION)
	//	scale_ratio = (F32)MAX_DIMENSION / (F32)width;

	//if (height > MAX_DIMENSION)
	//	scale_ratio = llmin(scale_ratio, (F32)MAX_DIMENSION / (F32)height);

	//mBrowserWidth = llround(scale_ratio * (F32)width);
	//mBrowserHeight = llround(scale_ratio * (F32)height);

 //  LLMozLib::getInstance()->setSize( mEmbeddedBrowserWindowId, mBrowserWidth, mBrowserHeight );

   S32 browser_depth = mWebBrowser->getMediaSource()->getMediaDepth();

   Ogre::PixelFormat ogreFormat = Ogre::PF_B8G8R8A8;
   if (browser_depth == 2)
      ogreFormat = Ogre::PF_B8G8R8;

   std::string textureName = this->getName();
   textureName.append("_texture");

   // create dynamic texture
   //// For some reason, the actual size is +1 to what we get from llwebbrowserctrl texture

   if (isStatic())
   {
      width = gSavedSettings.getS32("HtmlRendererTextureWidth");
      height = gSavedSettings.getS32("HtmlRendererTextureHeight");
   }
   mTexture = Ogre::TextureManager::getSingleton().createManual(textureName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
      Ogre::TEX_TYPE_2D, width, height, 0, ogreFormat, Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);

   // Create default empty content
   static const char nullData[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
   Ogre::Box src(0,0, 2, 2);
   Ogre::PixelBox pixelBox(src, ogreFormat, (void*)nullData);
   mTexture->getBuffer()->blitFromMemory(pixelBox);

   mMaterial->getTechnique(0)->getPass(0)->createTextureUnitState(textureName);

   mBrowserDepth = browser_depth;
}

//! Returns the web control
/*! Allows one to get the web browser control and use it directly.
*/
LLWebBrowserCtrl *LLOgreHtmlRenderer::getControl()
{
   return mWebBrowser;
}

bool LLOgreHtmlRenderer::_mouseDown(S32 x, S32 y, MASK mask)
{
//   convertInputCoordinates(x, y);
   S32 x2 = x, y2 = y;
   getUVFromScreen(&x2, &y2);
   llinfos << "Click screen coords: " << x << "  " << y << llendl;
//   x2 *= mBrowserWidth;
//   y2 *= mBrowserHeight;

//   llinfos << "Browser world coords: " << x2 << "  " << y2 << llendl;
//	LLMozLib::getInstance()->mouseDown(mEmbeddedBrowserWindowId, x2, y2);
//   LLMozLib::getInstance()->mouseDown(mEmbeddedBrowserWindowId, x, y);

   return LLFloater::handleMouseDown(x, y, mask);
//   return true;
}

bool LLOgreHtmlRenderer::_mouseUp(S32 x, S32 y, MASK mask)
{
//   convertInputCoordinates(x, y);
   S32 x2 = x, y2 = y;
   getUVFromScreen(&x2,&y2);
   llinfos << "Click screen coords: " << x << "  " << y << llendl;
//   x2 *= mBrowserWidth;
//   y2 *= mBrowserHeight;
//	LLMozLib::getInstance()->mouseUp( mEmbeddedBrowserWindowId, x2, y2 );
//   LLMozLib::getInstance()->mouseUp( mEmbeddedBrowserWindowId, x, y );

//	gViewerWindow->setMouseCapture( NULL );
   return LLFloater::handleMouseUp(x, y, mask);
//   return true;
}
/*
BOOL LLOgreHtmlRenderer::handleHover(S32 x, S32 y, MASK mask)
{
   S32 x2 = x, y2 = y;
   getUVFromScreen(&x2,&y2);
	LLMozLib::getInstance()->mouseMove( mEmbeddedBrowserWindowId, x2, y2 );

	return LLFloater::handleHover(x, y, mask);
}
*/
/*
BOOL LLOgreHtmlRenderer::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	return LLFloater::handleScrollWheel(x, y, clicks);
}
BOOL LLOgreHtmlRenderer::handleDoubleClick(S32 x,S32 y,MASK mask)
{
	return LLFloater::handleDoubleClick(x, y, mask);
}

BOOL LLOgreHtmlRenderer::handleRightMouseDown(S32 x,S32 y,MASK mask)
{
	return LLFloater::handleRightMouseDown(x, y, mask);
}
BOOL LLOgreHtmlRenderer::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
	return LLFloater::handleRightMouseUp(x, y, mask);
}

BOOL LLOgreHtmlRenderer::handleToolTip(S32 x, S32 y, LLString& msg, LLRect* sticky_rect_screen)
{
	return LLFloater::handleToolTip(x, y, msg, sticky_rect_screen);
}

void LLOgreHtmlRenderer::setMouseCapture(BOOL b)
{
	if (b)
		gViewerWindow->setMouseCapture(this);
	else if (hasMouseCapture())
		gViewerWindow->setMouseCapture(NULL);
}


BOOL LLOgreHtmlRenderer::hasMouseCapture()
{
	return (gFocusMgr.getMouseCapture() == this);
}
*/

void LLOgreHtmlRenderer::getUVFromScreen(S32 *x, S32 *y)
{
   S32 sx, sy;
   localPointToScreen(*x, *y, &sx, &sy);
   Ogre::Real normalizedX = sx / (Ogre::Real)gViewerWindow->getWindowDisplayWidth();
   Ogre::Real normalizedY = sy / (Ogre::Real)gViewerWindow->getWindowDisplayHeight();

   Ogre::Ray fromCamera = LLOgreRenderer::getPointer()->getCamera()->getCameraToViewportRay(normalizedX, normalizedY);
   Ogre::RaySceneQuery *sceneQuery = LLOgreRenderer::getPointer()->getSceneMgr()->createRayQuery(fromCamera);
   sceneQuery->setSortByDistance(true);

   llinfos << "Finding triangle: " << normalizedX << "  " << normalizedY << llendl;
   if (sceneQuery->execute().size() == 0)
   {
      LLOgreRenderer::getPointer()->getSceneMgr()->destroyQuery(sceneQuery);
      return; // we hit nothing
   }

   // Do simple bounding box test to find the right entity
   Ogre::Entity *entity = NULL;
   Ogre::RaySceneQueryResult &queryResult = sceneQuery->getLastResults();
   size_t n;
   for (n=0 ; n<queryResult.size() ; ++n)
   {
      // Only check entitys
      if (queryResult[n].movable != 0)
      {
         llinfos << "Hit something: " << queryResult[n].movable->getMovableType() << llendl;
          if (queryResult[n].movable->getMovableType().compare("Entity") == 0)
      {
         llinfos << "Hit something" << llendl;
         entity = static_cast<Ogre::Entity*>(queryResult[n].movable);
         if (entity == mPlaneEnt)
         {
            llinfos << "Found the right Ogre entity" << llendl;
            break;
         }
         entity = NULL;
      }
      }
   }
   LLOgreRenderer::getPointer()->getSceneMgr()->destroyQuery(sceneQuery);
   
   if (entity)
   {
      size_t vertexCount;
      size_t indexCount;
      Ogre::Vector3 *vertices;
      Ogre::Vector2 *texCoords;
      unsigned long *indices;

      getMeshInformation(entity->getMesh(), vertexCount, vertices, indexCount, indices, texCoords,
                  entity->getParentNode()->getWorldPosition(),
                  entity->getParentNode()->getWorldOrientation(),
                  entity->getParentNode()->getScale());
      size_t n;
      Ogre::Real shortestDistance = LLOgreRenderer::getPointer()->getCamera()->getFarClipDistance();
      U32 foundIndex = -1;
      for (n=0 ; n<indexCount ; n += 3)
      {
         std::pair<bool, Ogre::Real> hit = Ogre::Math::intersects(fromCamera, vertices[indices[n]], 
                                       vertices[indices[n+1]], vertices[indices[n+2]], true, false);
         if (hit.first)
         {
            if (hit.second < shortestDistance)
            {
               shortestDistance = hit.second;
               foundIndex = n;
            }
         }
      }


      if (foundIndex != -1)
      {
         Ogre::Vector3 point = fromCamera.getPoint(shortestDistance);
         
         // Ok we got a triangle and a point on that triangle, now we need the u,v coords of the point
         // None of this is very optimised, as we assume it doen't need to be
         
         // found polygon
         llinfos << "Found polygon, index: " << foundIndex << llendl;
         // Get triangle
         Ogre::Vector3 t1 = vertices[indices[foundIndex]];
         Ogre::Vector3 t2 = vertices[indices[foundIndex+1]];
         Ogre::Vector3 t3 = vertices[indices[foundIndex+2]];
         
         // Project triangle to 2D space, so calculating barycentric coords is easier/faster
         //
         // Find triangle's normal and it's most significant axis
         Ogre::Vector3 normal = (t1-t2).crossProduct(t3-t2);
   
         int axis = 0;
         (Ogre::Math::Abs(normal.x) < Ogre::Math::Abs(normal.y)) ? axis = 1 : axis = 0;
         (axis && Ogre::Math::Abs(normal.y) < Ogre::Math::Abs(normal.z)) ? axis = 2 : axis = 1;

         // get project plane axis
//         int projectAxis1 = (axis + 1) % 3;
//         int projectAxis2 = (axis + 2) % 3;
         
         // Project triangle and point to 2d plane with axis (projectAxis1, projectAxis2)
         Ogre::Vector2 p1(t1.x, t1.y);
         Ogre::Vector2 p2(t2.x, t2.y);
         Ogre::Vector2 p3(t3.x, t3.y);
         Ogre::Vector2 p(point.x, point.y);
         if (axis == 0)
         {
            p1 = Ogre::Vector2(t1.y, t1.z);
            p2 = Ogre::Vector2(t2.y, t2.z);
            p3 = Ogre::Vector2(t3.y, t3.z);
            p = Ogre::Vector2(point.y, point.z);
         } else if (axis == 1)
         {
            p1 = Ogre::Vector2(t1.x, t1.z);
            p2 = Ogre::Vector2(t2.x, t2.z);
            p3 = Ogre::Vector2(t3.x, t3.z);
            p = Ogre::Vector2(point.x, point.z);
         }

         Ogre::Vector3 bary = getBarycentricCoordinates(p1, p2, p3, p);
         llinfos << "bary: " << bary.x << "  " << bary.y << "  " << bary.z << llendl;
         Ogre::Vector2 t = texCoords[indices[foundIndex]] * bary.x + texCoords[indices[foundIndex+1]] * bary.y + texCoords[indices[foundIndex+2]] * bary.z;

         llinfos << "U,v: " << t.x << "  " << t.y << llendl;
         *x = t.x * mBrowserWidth;
//         *y = (1.0 - t.y) * mBrowserHeight;
         *y = t.y * mBrowserHeight;
         llinfos << "Browser coords: " << *x << "  " << *y << llendl;
      }
      delete [] vertices;
      delete [] indices;
      delete [] texCoords;
   }
}

Ogre::Vector3 LLOgreHtmlRenderer::getBarycentricCoordinates(const Ogre::Vector2 &p1, const Ogre::Vector2 &p2,
                                                      const Ogre::Vector2 &p3, const Ogre::Vector2 &p)
{
   Ogre::Vector3 bary;
   Ogre::Real denom = -p1.x * p3.y - p2.x * p1.y + p2.x * p3.y + p1.y * p3.x + p2.y * p1.x - p2.y * p3.x;
   if (Ogre::Math::Abs(denom) > 1e-6)
   {
      bary.x = (p2.x * p3.y - p2.y * p3.x - p.x * p3.y + p3.x * p.y - p2.x * p.y + p2.y * p.x) / denom;
      bary.y = -(-p1.x * p.y + p1.x * p3.y + p1.y * p.x - p.x * p3.y + p3.x * p.y - p1.y * p3.x) / denom;
   }
   bary.z = 1 - bary.x - bary.y;

   return bary;
}

void LLOgreHtmlRenderer::getMeshInformation(const Ogre::MeshPtr mesh, size_t &vertexCount, Ogre::Vector3* &vertices,
                                            size_t &indexCount, unsigned long* &indices, Ogre::Vector2* &texCoords, 
                                            const Ogre::Vector3 &position, const Ogre::Quaternion &orientation,
                                            const Ogre::Vector3 &scale)
{
   llassert(mesh.isNull() == false);

   bool sharedAdded = false;

   vertexCount = indexCount = 0;
   texCoords = NULL;

   // get vertex and index counts
   unsigned short n;
   for (n=0 ; n<mesh->getNumSubMeshes() ; ++n)
   {
      Ogre::SubMesh *subMesh = mesh->getSubMesh(n);

      if (subMesh->useSharedVertices)
      {
         if (!sharedAdded)
         {
            vertexCount += mesh->sharedVertexData->vertexCount;
            sharedAdded = true;
         }
      } else
      {
         vertexCount += subMesh->vertexData->vertexCount;
      }

      indexCount += subMesh->indexData->indexCount;
   }

   vertices = new Ogre::Vector3[vertexCount];
   indices = new unsigned long[indexCount];
   texCoords = new Ogre::Vector2[vertexCount];

   sharedAdded = false;
   size_t sharedOffset = 0;
   size_t currentOffset = 0;
   size_t nextOffset = 0;
   size_t indexOffset = 0;
   for (n=0 ; n<mesh->getNumSubMeshes() ; ++n)
   {
      Ogre::SubMesh *subMesh = mesh->getSubMesh(n);
      Ogre::VertexData* vertexData = subMesh->useSharedVertices ? mesh->sharedVertexData : subMesh->vertexData;

      if (subMesh->useSharedVertices == false || (subMesh->useSharedVertices && sharedAdded == false))
      {
         if (subMesh->useSharedVertices)
         {
            sharedAdded = true;
            sharedOffset = currentOffset;
         }
         const Ogre::VertexElement *posElem = vertexData->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
         const Ogre::VertexElement *texElem = vertexData->vertexDeclaration->findElementBySemantic(Ogre::VES_TEXTURE_COORDINATES);
         
         Ogre::HardwareVertexBufferSharedPtr vertexBuffer = vertexData->vertexBufferBinding->getBuffer(posElem->getSource());

         U8 *vertex = static_cast<U8*>(vertexBuffer->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));
         float *pReal;
         size_t k;
         for (k=0 ; k<vertexData->vertexCount ; ++k, vertex += vertexBuffer->getVertexSize())
         {
            posElem->baseVertexPointerToElement(vertex, &pReal);
            Ogre::Vector3 pt(pReal[0], pReal[1], pReal[2]);
            vertices[currentOffset + k] = (orientation * (pt * scale)) + position;
            
            texElem->baseVertexPointerToElement(vertex, &pReal);  
            texCoords[currentOffset + k] = Ogre::Vector2(pReal[0], pReal[1]);
         }
         vertexBuffer->unlock();
         nextOffset += vertexData->vertexCount;
      }
   
      Ogre::IndexData *indexData = subMesh->indexData;
      size_t triCount = indexData->indexCount / 3;
      Ogre::HardwareIndexBufferSharedPtr indexBuffer = indexData->indexBuffer;

      bool use32bitindexes = (indexBuffer->getType() == Ogre::HardwareIndexBuffer::IT_32BIT);

      unsigned long* pLongInd = static_cast<unsigned long*>(indexBuffer->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));
      unsigned short* pShortInd = reinterpret_cast<unsigned short*>(pLongInd);

      size_t offset = subMesh->useSharedVertices ? sharedOffset : currentOffset;
      if (use32bitindexes)
      {
         size_t k;
         for (k=0 ; k<triCount*3 ; ++k)
         {
            indices[indexOffset++] = pLongInd[k] + static_cast<unsigned long>(offset);
         }
      } else
      {
         size_t k;
         for (k=0 ; k<triCount*3 ; ++k)
         {
            indices[indexOffset++] = static_cast<unsigned long>(pShortInd[k]) + static_cast<unsigned long>(offset);
         }
      }

      indexBuffer->unlock();
      currentOffset = nextOffset;
   }
}

