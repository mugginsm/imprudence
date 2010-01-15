/**
 * @file llogre.cpp
 * @brief LLOgreRenderer class implementation
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#include "llviewerprecompiledheaders.h"
#include "llviewerwindow.h"
#include "llviewercamera.h"
#include "llagent.h"
#include "llfasttimer.h"
#include "llglheaders.h"
#include "llviewerimagelist.h"
#include "llviewerobject.h"
#include "llvovolume.h"
#include "llxmltree.h"
#include "llsky.h"
#include "llogre.h"
#include "llogreobject.h"
#include "llogrehtmlrenderer.h"
#include "llogreassetloader.h"
#include "rexogrelegacymaterial.h"
#include "llogremediaengine.h"
#include "llobjectmanager.h"
#include "llselectmgr.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llagent.h"
#include "llappviewer.h"
#include "lllightmap.h"

#include <OGRE/Ogre.h>

//#include "AvatarGeneratorScene.h"
//#include <RexAvatar.h>
#include "llvoavatar.h"
//#include "llavatar.h"

#include "llhtmltexturesystem.h"

#include "llogrepostprocess.h"
#include "RexRttCamera.h"
#include "rexclienteffect.h"
#include "llwlparammanager.h"
#include "rexambientlightoverride.h"
#include "rexflashanimationmanager.h"
#include "rexogretexture.h"

#if LL_WINDOWS
#include <OGRE/opt/OgreGLPlugin.h>
#include <OGRE/opt/OgreParticleFXPlugin.h>
#include <OGRE/opt/OgreOctreePlugin.h>
#include <OGRE/opt/OgreCgPlugin.h>
#else
#include "llwindowsdl.h"
#endif
#include "OGRE/OgreParticleSystemRenderer.h"
#include "OGRE/OgreBillboardParticleRenderer.h"

#include "../tightvnc/tightvnc/TightVNCPluginFactory.h"

extern BOOL gInRex;
extern BOOL gInPureRex;
extern BOOL gOgreRender;

extern LLViewerObjectList gObjectList;

extern LLDispatcher gGenericDispatcher;

LLOgreRenderer gOgreRenderer;

//! Time since last frame
extern F32 gFrameIntervalSeconds;

FogDispatchHandler LLOgreRenderer::smDispatchForceFog;
WaterDispatchHandler LLOgreRenderer::smDispatchWaterHeight;
MeshAnimDispatchHandler LLOgreRenderer::smDispatchMeshAnimation;
AmbientLightDispatchHandler LLOgreRenderer::smDispatchAmbientLight;
SkyDispatchHandler LLOgreRenderer::smDispatchSky;

const std::string FogDispatchHandler::mMessageID("RexFog");
bool FogDispatchHandler::operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
      const LLDispatchHandler::sparam_t& string)
{
   if (string.size() < 5)
      return false;

   // We may get commas instead of dots in the string representation of the floats
   std::string strFogStart = string[0];
   std::string strFogEnd = string[1];
   std::string strFogc_r = string[2];
   std::string strFogc_g = string[3];
   std::string strFogc_b = string[4];
   LLStringUtil::replaceChar(strFogStart, ',', '.');
   LLStringUtil::replaceChar(strFogEnd, ',', '.');
   LLStringUtil::replaceChar(strFogc_r, ',', '.');
   LLStringUtil::replaceChar(strFogc_g, ',', '.');
   LLStringUtil::replaceChar(strFogc_b, ',', '.');

   Ogre::Real fogStart = Ogre::StringConverter::parseReal(strFogStart);
   Ogre::Real fogEnd = Ogre::StringConverter::parseReal(strFogEnd);
   Ogre::Real c_r = Ogre::StringConverter::parseReal(strFogc_r);
   Ogre::Real c_g = Ogre::StringConverter::parseReal(strFogc_g);
   Ogre::Real c_b = Ogre::StringConverter::parseReal(strFogc_b);
   Ogre::ColourValue fogColour(c_r, c_g, c_b);

   LLOgreRenderer::getPointer()->setForceFog(fogStart, fogEnd, fogColour);

   llinfos << "Server forcing fog settings: " << fogStart << " " << fogEnd << " " << Ogre::StringConverter::toString(fogColour) << llendl;

   return true;
}
const std::string WaterDispatchHandler::mMessageID("RexWaterHeight");
bool WaterDispatchHandler::operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
      const LLDispatchHandler::sparam_t& string)
{
   if (string.empty())
      return false;

   // We may get commas instead of dots in the string representation of the floats
   std::string strWaterHeight = string[0];
   LLStringUtil::replaceChar(strWaterHeight, ',', '.');

   Ogre::Real waterHeight = Ogre::StringConverter::parseReal(strWaterHeight);


   LLViewerRegion* region = gAgent.getRegion();
   if (region)
   {
      region->setWaterHeight(waterHeight);
   }
   LLOgreRenderer::getPointer()->setWaterHeight(waterHeight);

   llinfos << "Server forcing water height: " << waterHeight << llendl;

   return true;
}

const std::string MeshAnimDispatchHandler::mMessageID("RexPrimAnim");
bool MeshAnimDispatchHandler::operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
      const LLDispatchHandler::sparam_t& string)
{
   if (string.size() != 5)
      return false; // unexpected number of parameters, do not handle message

   // We may get commas instead of dots in the string representation of the floats
   LLUUID primId = LLUUID(string[0]);
   std::string animName = string[1];
   std::string strRate = string[2];
   LLStringUtil::replaceChar(strRate, ',', '.');
   float rate = Ogre::StringConverter::parseReal(strRate);
   bool looped = Ogre::StringConverter::parseBool(string[3]);
   bool stop = Ogre::StringConverter::parseBool(string[4]);

   LLViewerObject *object = gObjectList.findObject(primId);
   if (object)
   {
      LLOgreObject *ogreObject = object->getOgreObject();
      if (ogreObject)
      {
         if (stop)
         {
            ogreObject->disableAnim(animName);
         } else
         {
            ogreObject->enableAnim(animName, looped, 0.25f, true);
            ogreObject->setAnimSpeed(animName, rate);
            if (rate < 0.f)
            {
               ogreObject->setAnimToEnd(animName);
            }
            ogreObject->setAnimAutoStop(animName, looped == false);
         }
      }
   }
   return true;
}

const std::string AmbientLightDispatchHandler::mMessageID("RexAmbientL");
bool AmbientLightDispatchHandler::operator()(const LLDispatcher* dispatcher, const std::string& key,
		const LLUUID& invoice, const LLDispatchHandler::sparam_t& string)
{
   if (string.size() < 3)
      return true;

   LLVector3 sunDirection;
   LLColor4 sunColor;
   LLVector3 ambientColor;
   bool fail = false;
   fail |= !LLVector3::parseVector3(string[0].c_str(), &sunDirection);
   fail |= !LLColor4::parseColor(string[1].c_str(), &sunColor);
   fail |= !LLVector3::parseVector3(string[2].c_str(), &ambientColor);

   if (fail)
   {
      llwarns << "Failed to parse vector in generic message '" << mMessageID << "'." << llendl;
   } else
   {
      LLWLParamManager::instance()->mAnimator.setDayTime(0.0);
		LLWLParamManager::instance()->mAnimator.mIsRunning = false;
		LLWLParamManager::instance()->mAnimator.mUseLindenTime = false;
      gSky.setOverrideSun(TRUE);
      gSky.setSunDirection(sunDirection, LLVector3::zero);

      Ogre::ColourValue ogreSunColour = Ogre::ColourValue(ambientColor.mV[VX], ambientColor.mV[VY], ambientColor.mV[VZ]);
      gOgreRenderer.getSceneMgr()->setAmbientLight(gOgreRenderer.toOgreColor(sunColor));
      gOgreRenderer.overrideAmbientLighting(true, sunDirection, sunColor);
   }

   return true;
}

const std::string SkyDispatchHandler::mMessageID("RexSky");
bool SkyDispatchHandler::operator()(const LLDispatcher* dispatcher, const std::string& key,
		const LLUUID& invoice, const LLDispatchHandler::sparam_t& string)
{
   if (string.size() < 4)
      return true;

   RexSkyType type = REXSKY_NONE;
   int intType = Ogre::StringConverter::parseInt(string[0]);
   switch (intType)
   {
   case 0:
      type = REXSKY_NONE;
      break;
   case 1:
      type = REXSKY_BOX;
      break;
   case 2:
      type = REXSKY_DOME;
      break;
   }

   Ogre::StringVector images = Ogre::StringUtil::split(string[1]);
   F32 curvature = Ogre::StringConverter::parseReal(string[2]);
   F32 tiling = Ogre::StringConverter::parseReal(string[3]);
   if (LLOgreRenderer::getPointer()->isInitialized())
   {
      LLOgreRenderer::getPointer()->updateSky(type, images, curvature, tiling);
   }

   return true;
}

//! Constructor: do not create any objects yet
LLOgreRenderer::LLOgreRenderer() : 
	mRoot(0), 
	mCamera(0), 
	mViewport(0),
	mSceneMgr(0), 
	mWindow(0), 
	mDebugOverlay(0),
	mViewerWindow(0),
	mAssetLoader(0),
    mGenerateLM(false),
    mForceFog(false),
    mCurrentLMObject(0),
	mViewerWindowHandle(0),
	mObjectUpdateCount(0),
	mTextureUpdateCount(0),
	mLastFrameTime(0),
	mOgreContextCount(0),
	mOgreContextSaved(false), 
	mSLContextSaved(false), 
	mGLPlugin(0),
	mParticlePlugin(0),
	mCgPlugin(0),
	mOctreePlugin(0),
    mVNCPlugin(0),
	mWaterNode(0),
	mWaterReverseNode(0),
    mRayQuery(0),
	mInitialized(false),
    mUnderWater(false),
    mForceUnderWaterUpdate(false),
    mFlashAnimationManager(0),
    mCurrentSkyType(REXSKY_BOX),
    mCurrentSkyBoxImageCount(0),
    mSkyBoxImageCount(0),
	mShadowMethod(ShadowsNone),
    mWaterHeight(20.0f),
    mWaterMaterial("Rex/Ocean_Cg"),
    mFogStart(50.0f),
    mFogEnd(150.0f),
    mWaterFogStart(1.0f),
    mWaterFogEnd(15.0f),
    mWaterFogColor(0.2, 0.4, 0.3),
    mBackgroundColor(0.5, 0.5, 1.0),
    mSkyColor(1.0, 1.0, 1.0),
    mCameraNearClip(0.5f),
    mCameraFarClip(500.0f),
    mCameraFOV(Ogre::Degree(60.0f).valueRadians()),
    mUseObjectShadows(false),
    mForceDisableShadows(false)
{
   mSunOverride = AmbientLightOverridePtr(new AmbientLightOverride);
   mSunOverride->mOverride = false;
   
    mOriginalWaterFogStart = mWaterFogStart;
    mOriginalWaterFogEnd = mWaterFogEnd;
    mOriginalWaterFogColor = mWaterFogColor;
    mOriginalWaterHeight = mWaterHeight;
}

//! Destructor: shutdown if not done so already
LLOgreRenderer::~LLOgreRenderer()
{
	shutdown();
}

//! Shutdown: destroy Ogre root, self-installed managers and plugins
void LLOgreRenderer::shutdown()
{
    LLObjectManager* objMgr = LLObjectManager::getInstance();
    if (objMgr) delete objMgr;

	if (mAssetLoader)
	{
		delete mAssetLoader;
		mAssetLoader = 0;
	}

   if (mFlashAnimationManager)
   {
      setOgreContext();
      delete mFlashAnimationManager;
      mFlashAnimationManager = 0;
      setSLContext();
   }
/* I think this is only needed for realxtend avatars
   if (Rex::XmlManager::getSingletonPtr() != 0)
      delete (Rex::XmlManager::getSingletonPtr());

   if (Rex::DynamicAnimationManager::getSingletonPtr() != 0)
      delete (Rex::DynamicAnimationManager::getSingletonPtr());

   if (Rex::ClothManager::getSingletonPtr() != 0)
      delete (Rex::ClothManager::getSingletonPtr());
*/
   for (ExternalTextureSourceList::iterator i = mExternalTextureSources.begin(); i != mExternalTextureSources.end(); ++i)
   {
       delete *i;
   }
   mExternalTextureSources.clear();

	if (mRoot)
	{
      setOgreContext();
		delete mRoot;
		mRoot = 0;
      setSLContext();
	}

#if LL_WINDOWS
	if (mCgPlugin)
	{
		delete mCgPlugin;
		mCgPlugin = 0;
	}
	if (mParticlePlugin)
	{
		delete mParticlePlugin;
		mParticlePlugin = 0;
	}
	if (mOctreePlugin)
	{
		delete mOctreePlugin;
		mOctreePlugin = 0;
	}
	if (mGLPlugin)
	{
		delete mGLPlugin;
		mGLPlugin = 0;
	}
#else
    mRoot->unloadPlugin("RenderSystem_GL"); // Trying this instead
//    mRoot->unloadPlugin("Plugin_OctreeSceneManager");
    mRoot->unloadPlugin("Plugin_ParticleFX");
//    mRoot->unloadPlugin("Plugin_CgProgramManager");
#endif

    if (mVNCPlugin)
    {
        delete mVNCPlugin;
        mVNCPlugin = 0;
    }

    if (mMediaEngine)
    {
        delete mMediaEngine;
        mMediaEngine = 0;
    }

	if(mPostprocess)
	{
		delete mPostprocess;
		mPostprocess = 0;
	}

	mInitialized = false;
}

//! Return pointer to ogre renderer
LLOgreRenderer *LLOgreRenderer::getPointer()
{
   return &gOgreRenderer;
}

//! Initialize Ogre, using a pre-created SL viewer window. Also create camera, viewport, scene manager & asset loader
void LLOgreRenderer::init(LLViewerWindow* pViewerWindow)
{
    if (mInitialized)
        return;

    // Save SL context
    saveSLContext();

	// Set the window to use
	if (!pViewerWindow)
		llerrs << "Null viewer window specified, aborting" << llendl;
	mViewerWindow = pViewerWindow;

	Ogre::String pluginsPath;

#if LL_WINDOWS
    pluginsPath = ""; // Can't use plugins because of static C++ runtime, no plugin file necessary
#else
	pluginsPath = mResourcePath + "plugins.cfg";
    llinfos << "Plugins path:" << pluginsPath << llendl;
#endif

	// Create Ogre root object, init resource paths
	mRoot = new Ogre::Root(pluginsPath, mResourcePath + "ogre.cfg", mResourcePath + "Ogre.log");

#if LL_WINDOWS
	// Install static plugins, required this way with static C++ runtime as used by SL viewer (otherwise Vista crash) 
	mGLPlugin = new Ogre::GLPlugin(); 
	mRoot->installPlugin(mGLPlugin);   
	mOctreePlugin = new Ogre::OctreePlugin();
	mRoot->installPlugin(mOctreePlugin);
	mParticlePlugin = new Ogre::ParticleFXPlugin();
	mRoot->installPlugin(mParticlePlugin);
	mCgPlugin = new Ogre::CgPlugin();
	mRoot->installPlugin(mCgPlugin);
#else
    mRoot->loadPlugin("RenderSystem_GL"); // Trying this instead
//    mRoot->loadPlugin("Plugin_OctreeSceneManager");
    mRoot->loadPlugin("Plugin_ParticleFX");
//    mRoot->loadPlugin("Plugin_CgProgramManager");
#endif

#if LL_WINDOWS
    mVNCPlugin = TightVNCPluginFactory::createPlugin();
    mRoot->installPlugin(mVNCPlugin);
#endif

	initResources();
	
	// We must get the OpenGL rendersystem to be compatible with SL rendering
	Ogre::RenderSystem* pRenderer = mRoot->getRenderSystemByName("OpenGL Rendering Subsystem");
	if (!pRenderer)
		llerrs << "Could not find Ogre OpenGL rendersystem, aborting" << llendl;
	mRoot->setRenderSystem(pRenderer);

	// Initialize OgreRoot, but do not create a rendering window yet
	mRoot->initialise(false); 

	// Create scenemanager, camera, renderwindow, viewport as needed
    // NOTE: creates Ogre GL context, it is active upon return
	initRenderer();

	// Load resources now
    Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

	// Create debug overlay
	mDebugOverlay = Ogre::OverlayManager::getSingleton().getByName("Core/DebugOverlay");

	// Create asset loader
	mAssetLoader = new LLOgreAssetLoader();
	// Create postprocess manager
	mPostprocess = new LLOgrePostprocess(mWindow);
    // Create mediaengine 
    mMediaEngine = new LLOgreMediaEngine();
    // Create html texture system
    mExternalTextureSources.push_back(new LLHtmlTextureSystem);

    //! add handler for 'force fog' generic message
   gGenericDispatcher.addHandler(smDispatchForceFog.mMessageID, &smDispatchForceFog);
   gGenericDispatcher.addHandler(smDispatchWaterHeight.mMessageID, &smDispatchWaterHeight);
   gGenericDispatcher.addHandler(smDispatchMeshAnimation.mMessageID, &smDispatchMeshAnimation);
   gGenericDispatcher.addHandler(smDispatchAmbientLight.mMessageID, &smDispatchAmbientLight);
   gGenericDispatcher.addHandler(smDispatchSky.mMessageID, &smDispatchSky);
   
   RexClientEffect::registerHandlers();
   
   // initialize rtt camera manager
   RexRttCameraManager::getInstance();

   mFlashAnimationManager = new RexFlashAnimationManager();

	mInitialized = true;

    // Restore SL context
    setSLContext();
}

//! Initialize Ogre resource paths & packs
void LLOgreRenderer::initResources()
{
/* more avatar stuff -Patrick Sapinski (Saturday, December 12, 2009)
   Ogre::ResourceGroupManager::getSingleton().createResourceGroup(Rex::XmlManager::XMLDATA_RESOURCE_GROUP_NAME);
   // Create Avatar data managers
   new Rex::XmlManager();
   new Rex::DynamicAnimationManager();
   new Rex::ClothManager();
*/
    // Load resource paths from config file
	Ogre::ConfigFile cf;
    cf.load(mResourcePath + "resources.cfg");

    // Go through all sections & settings in the file
	Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();

	Ogre::String secName, typeName, archName;
    while (seci.hasMoreElements())
    {
        secName = seci.peekNextKey();
        bool recursive = false;
		  Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();
		  Ogre::ConfigFile::SettingsMultiMap::iterator i;
        i = settings->find("Recursive");
        if (i != settings->end() && i->second == "True")
        {
           recursive = true;
        }
        for (i = settings->begin(); i != settings->end(); ++i)
        {
            typeName = i->first;
            archName = i->second;
            if (typeName == "Recursive")
            {
               continue;
            }
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
            // OS X does not set the working directory relative to the app,
            // In order to make things portable on OS X we need to provide
            // the loading with it's own bundle path location
            ResourceGroupManager::getSingleton().addResourceLocation(
                String(macBundlePath() + "/" + archName), typeName, secName);
#else
			Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
                archName, typeName, secName, recursive);
#endif
        }
    }
}

//! Init renderer
void LLOgreRenderer::initRenderer()
{
	// Get window handle from the LLViewerWindow
	mViewerWindowHandle = mViewerWindow->getWindow()->getPlatformWindow();

	#ifndef LL_WINDOWS
	    Display* display = get_SDL_Display();
	    int screen = DefaultScreen (display);
	    Window window = get_SDL_XWindowID();

	    // from SDL code
        XWindowAttributes a;
        XVisualInfo vi_in;
	    XVisualInfo *vi_out;
        int out_count;

        XGetWindowAttributes(display, window, &a);
        vi_in.screen = screen;
        vi_in.visualid = XVisualIDFromVisual(a.visual);
        vi_out = XGetVisualInfo(display, VisualScreenMask|VisualIDMask, &vi_in, &out_count);

	    Ogre::String x11WinHndl = Ogre::StringConverter::toString((unsigned long)display) + ":" + 
            Ogre::StringConverter::toString(screen) + ":" +
            Ogre::StringConverter::toString((unsigned long)window) + ":" + 
            Ogre::StringConverter::toString((unsigned long)vi_out);
	#endif

	// Parameter list to specify that we use an external window handle
	Ogre::NameValuePairList miscParams;
	#ifdef LL_WINDOWS
		miscParams["externalWindowHandle"] = Ogre::StringConverter::toString(((size_t)mViewerWindowHandle));
		miscParams["externalGLControl"] = "true";
		// miscParams["externalGLContext"] = Ogre::StringConverter::toString(((size_t)wglGetCurrentContext()));
	#else
		Ogre::LogManager::getSingleton().logMessage("X11 window handle & visual info: " + x11WinHndl);
		miscParams["externalWindowHandle"] = x11WinHndl;
	#endif

    // Check for lack of FBO support: Intel GMA's etc. will not have it and fall to PBuffer use, which may be fatal
    if (!gGLManager.mHasFramebufferObject)
    {
        mRoot->getRenderSystem()->setConfigOption("RTT Preferred Mode", "Copy"); // should be safer
        mForceDisableShadows = true;
    }

	// Create new render window & full size viewport
	unsigned int width = mViewerWindow->getWindowDisplayWidth();
	unsigned int height = mViewerWindow->getWindowDisplayHeight();
	mWindow = mRoot->createRenderWindow("RenderWindow", width, height, false, &miscParams);
	
	// See if window creation was successful
	if (mWindow)
	{
		// Create the main objects if they don't exist yet: SceneManager, Camera, Viewport
		if (!mSceneMgr)
			mSceneMgr = mRoot->createSceneManager(Ogre::ST_GENERIC, "SceneManager");

		if (!mCamera)
			mCamera = mSceneMgr->createCamera("Camera");
	
		mViewport = mWindow->addViewport(mCamera);
		mCamera->setAspectRatio(Ogre::Real(width) / Ogre::Real(height));

		// Init render target manually
		mRoot->getRenderSystem()->_initRenderTargets();
    
		saveOgreContext();
	}
	else
		llerrs << "Could not create Ogre render window, aborting" << llendl;
}

//! Resize viewer window
void LLOgreRenderer::resizeViewerWindow(unsigned int width, unsigned int height)
{
	if (!mInitialized) return;

	setOgreContext();

	mWindow->resize(width, height);
	mWindow->windowMovedOrResized();
	// Update camera aspect ratio
	if (mCamera)
		mCamera->setAspectRatio(Ogre::Real(width) / Ogre::Real(height));
/* more avatar stuff -Patrick Sapinski (Saturday, December 12, 2009)
   if (AvatarGeneratorScene::isVisible())
   {
      AvatarGeneratorScene::getSingleton().resizeGUIToWindow();
   }
   else */
   {
      // Disable/reenable compositors so that their RTT targets will be resized correctly
      if (mPostprocess)
      {
         Ogre::CompositorManager::getSingleton().removeCompositorChain(mViewport);
         mPostprocess->restoreOldSettings(mViewport);
      }
   }

	setSLContext();
}

void LLOgreRenderer::setForceFog(Ogre::Real fogStart, Ogre::Real fogEnd, const Ogre::ColourValue &fogColour)
{
   if (fogStart < 1.f) fogStart = 1.f;
   if (fogEnd < fogStart) fogEnd = fogStart;

   mForceFog = true;

//   mBackgroundColor = toLLColor(fogColour);
//   mFogStart = fogStart;
//   mFogEnd = fogEnd;

   mWaterFogColor = toLLColor(fogColour);
   mWaterFogStart = fogStart;
   mWaterFogEnd = fogEnd;

   mForceUnderWaterUpdate = true; // We need to update things that get updated when switching underwater/overwater

//   mCamera->setFarClipDistance(fogEnd + 10.f);
}

LLOgreRenderer::SceneShadowingMethod StringToOgreSceneShadowingMethod(const char *method)
{
	if (!method)
		return LLOgreRenderer::ShadowsNone;

	if (!stricmp(method, "pcf")) return LLOgreRenderer::ShadowsPCF;
	if (!stricmp(method, "bool")) return LLOgreRenderer::ShadowsBool;
	if (!stricmp(method, "none")) return LLOgreRenderer::ShadowsNone;

	llwarns << "WARNING! Unsupported shadowing method " << method << " encountered!. Disabling shadows." << llendl;
	return LLOgreRenderer::ShadowsNone;
}

const char *LLOgreRenderer::getSceneTerrainMaterialName() const
{
	if (mShadowMethod == ShadowsPCF)
		return "Rex/TerrainPCF";
	else
		return "Rex/TerrainBool";
}

const std::string& LLOgreRenderer::getBaseMaterialName()
{
    return mBaseMaterialName;
}

//! Set up ogre scene from an XML file
void LLOgreRenderer::setupScene(const char *filename)
{
	setOgreContext();

	// Defaults
   BOOL useObjectGrouping = FALSE;

	F32 shadowFarDist = 15;
	U32 shadowTextureSize = 512;
	F32 shadowFadeStart = 0.7;
	F32 shadowFadeEnd = 0.9;
	F32 shadowDirLightTextureOffset = 0.6;
	int shadowTextureCount = 1;
	LLColor4 shadowColor(0.6f, 0.6f, 0.6f);   
   BOOL useObjectShadows = FALSE;
	mShadowMethod = ShadowsNone;
	// This is the default material to use for shadow buffer rendering pass, overridable in script.
	// Note that we use the same single material (vertex program) for each object, so we're relying on
	// that we use Ogre software skinning. Hardware skinning would require us to do different vertex programs
	// for skinned/nonskinned geometry.
	std::string ogreShadowCasterMaterial = "rex/ShadowCaster"; 

    if (!mInitialized) return;

	// Parse scene settings from an Xml file
   LLXmlTree xml_controls;
	if (!xml_controls.parseFile(filename))
	{
		llwarns << "Unable to open Ogre scene definition file " << filename << llendl;
	}

	LLXmlTreeNode* rootp = xml_controls.getRoot();
	if (rootp)
	{
      LLXmlTreeNode* grouping = rootp->getChildByName("grouping");
    
      // Get object grouping parameters
      if (grouping)
      {
         grouping->getAttributeBOOL("enable", useObjectGrouping);
      }
      
      LLXmlTreeNode* sky_root = rootp->getChildByName("sky_generic");
      if (sky_root)
      {
         BOOL draw_first = TRUE;
         sky_root->getAttributeString("material", mGenericSkyParameters.mMaterial);
         sky_root->getAttributeF32("distance", mGenericSkyParameters.mDistance);
         sky_root->getAttributeBOOL("draw_first", draw_first);
         mGenericSkyParameters.mDrawFirst = draw_first;
         sky_root->getAttributeF32("angle", mGenericSkyParameters.mAngle);
         sky_root->getAttributeVector3("axis", mGenericSkyParameters.mAngleAxis);

         std::string type = "box";
         sky_root->getAttributeString("type", type);
         if (type == "box")
            mCurrentSkyType = REXSKY_BOX;
         else if (type == "dome")
            mCurrentSkyType = REXSKY_DOME;
         else if (type == "plane")
            mCurrentSkyType = REXSKY_PLANE;
         else if (type == "none")
            mCurrentSkyType = REXSKY_NONE;
      }
      LLXmlTreeNode* sky_dome = rootp->getChildByName("sky_dome");
      if (sky_dome)
      {
         sky_dome->getAttributeF32("curvature", mSkydomeParameters.mCurvature);
         sky_dome->getAttributeF32("tiling", mSkydomeParameters.mTiling);
         sky_dome->getAttributeS32("x_segments", mSkydomeParameters.mXSegments);
         sky_dome->getAttributeS32("y_segments", mSkydomeParameters.mYSegments);
         sky_dome->getAttributeS32("y_segments_keep", mSkydomeParameters.mYSegments_keep);
      }

		LLXmlTreeNode* waterPlane = rootp->getChildByName("ocean");

		// Get water parameters
		if (waterPlane)
		{  
			waterPlane->getAttributeF32("height", mWaterHeight);
            mOriginalWaterHeight = mWaterHeight;
            std::string waterMaterial = mWaterMaterial;
			waterPlane->getAttributeString("material", waterMaterial);
            mWaterMaterial = waterMaterial;
		} 
		else
			llwarns << "No water parameters defined" << llendl;

		// Get lighting parameters
		LLXmlTreeNode* colors = rootp->getChildByName("lighting");
		if (colors)
		{
			colors->getAttributeColor("background", mBackgroundColor);
		}
		else llwarns << "No lighting parameters defined" << llendl;

		// Get fogging parameters
		LLXmlTreeNode* fog = rootp->getChildByName("fog");
		if (fog)
		{
			fog->getAttributeF32("start", mFogStart);
			fog->getAttributeF32("end", mFogEnd);
			fog->getAttributeF32("waterstart", mWaterFogStart);
			fog->getAttributeF32("waterend", mWaterFogEnd);
			fog->getAttributeColor("watercolor", mWaterFogColor);
            mOriginalWaterFogStart = mWaterFogStart;
            mOriginalWaterFogEnd = mWaterFogEnd;
            mOriginalWaterFogColor = mWaterFogColor;
        }

		// Get camera parameters
		LLXmlTreeNode* camera = rootp->getChildByName("camera");
		if (camera)
		{
			camera->getAttributeF32("nearclip", mCameraNearClip);
			camera->getAttributeF32("farclip", mCameraFarClip);
		}

        // Get legacymaterial parameters
		LLXmlTreeNode* legacymat = rootp->getChildByName("legacymaterial");
        if (legacymat)
        {
            std::string name;
            legacymat->getAttributeString("basematerial", name);
            mBaseMaterialName = name;
        }

		// Get shadow parameters
		LLXmlTreeNode* shadow = rootp->getChildByName("shadow");
		if (shadow)
		{
			shadow->getAttributeColor("color", shadowColor);
			shadow->getAttributeF32("fardist", shadowFarDist);
			shadow->getAttributeU32("texturesize", shadowTextureSize);
			shadow->getAttributeF32("textureoffset", shadowDirLightTextureOffset);
			shadow->getAttributeS32("texturecount", shadowTextureCount);
			shadow->getAttributeF32("fadestart", shadowFadeStart);
			shadow->getAttributeF32("fadeend", shadowFadeEnd);
            shadow->getAttributeBOOL("objectshadows", useObjectShadows);		
            mUseObjectShadows = useObjectShadows;

			// Read which Ogre method we will use for shadowing.
			std::string str;
			shadow->getAttributeString("method", str);
			mShadowMethod = StringToOgreSceneShadowingMethod(str.c_str());

			// The default for shadowCaster material is "rex/ShadowCaster", but give the script ability to override if needed. This is used only in PCF mode.
			if (shadow->getAttributeString("casterMaterial", str) != FALSE)
				ogreShadowCasterMaterial = str;
		}
	}
	else llwarns << "No root defined" << llendl;

   // Set up object manager if requested
   if (useObjectGrouping)
   {
      LLObjectManager* objMgr = LLObjectManager::getInstance();
      if (!objMgr)
         objMgr = new LLObjectManager(mSceneMgr);
   }

   // Check forced disable of shadows (bad RTT support)
   if (mForceDisableShadows)
       mShadowMethod = ShadowsNone;

	if (mShadowMethod != ShadowsNone) 
	{
		mSceneMgr->setShadowColour(toOgreColor(shadowColor));
		mSceneMgr->setShadowFarDistance(shadowFarDist);

		mSceneMgr->setShadowTextureSize(shadowTextureSize);
		mSceneMgr->setShadowDirLightTextureOffset(shadowDirLightTextureOffset);
		mSceneMgr->setShadowTextureFadeStart(shadowFadeStart);
		mSceneMgr->setShadowTextureFadeEnd(shadowFadeEnd);
		mSceneMgr->setShadowTextureCount(shadowTextureCount);

		// Set shadow mode to texture rather than stencil, so object geometry has less of an impact
		if (mShadowMethod == ShadowsPCF)
		{
			mSceneMgr->setShadowTexturePixelFormat(Ogre::PF_FLOAT16_R);
			mSceneMgr->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_ADDITIVE_INTEGRATED);
			mSceneMgr->setShadowTextureCasterMaterial(ogreShadowCasterMaterial.c_str());
			mSceneMgr->setShadowTextureSelfShadow(true);
		}
		else // shadowMethod == OgreShadowsBool and we use the crappy broken modulative texturing approach.
		{	
			mSceneMgr->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_MODULATIVE);
			mSceneMgr->setShadowTextureSelfShadow(false);
		}

		// Enable shadows on the notexture material if objectshadows on
		if (useObjectShadows)
		{
			Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName("NoTexture");
			if (!mat.isNull())
				 mat->setReceiveShadows(true);
			mat = Ogre::MaterialManager::getSingleton().getByName("NoTexturefb");
			if (!mat.isNull())
				 mat->setReceiveShadows(true);
		}
	}
	else
	{
	    mSceneMgr->setShadowTechnique(Ogre::SHADOWTYPE_NONE);
	}

	// Set shadow projection mode

	// Tested a few different projection modes.. the standard one is most stable, but wastes texel space so badly that it is not that usable.
	// Also, the current z test epsilon values are configured for focused camera setup instead of the standard setup, so if you change back to the standard
	// one, you need to reconfigure the z values.

	// The LiSPSM just plain doesn't work. Causes ugly swimming and in some angles the texels are mapped horribly. The problems are probably related to the
	// single big world terrain size or the overly far viewer camera far plane distance.
//	Ogre::ShadowCameraSetupPtr shadowCameraSetup = Ogre::ShadowCameraSetupPtr(new Ogre::LiSPSMShadowCameraSetup);

	// The focused setup seems to be the best. In some angles gives horrible shadows, and does give a bit of swimming - but generally works the best.
	Ogre::ShadowCameraSetupPtr shadowCameraSetup = Ogre::ShadowCameraSetupPtr(new Ogre::FocusedShadowCameraSetup());
	mSceneMgr->setShadowCameraSetup(shadowCameraSetup);

	// Set lighting parameters
	mViewport->setBackgroundColour(toOgreColor(mBackgroundColor));
	mGlobalLight = mSceneMgr->createLight("sunlight");
	mGlobalLight->setType(Ogre::Light::LT_DIRECTIONAL);

	// Set camera parameters
	mCamera->setFixedYawAxis(true, -Ogre::Vector3::UNIT_Z);
    mCamera->setFOVy(Ogre::Radian(mCameraFOV)); 
    mCamera->setFarClipDistance(mCameraFarClip);
    mCamera->setNearClipDistance(mCameraNearClip);

	//! \todo IM_SPLINE causes weird behaviour on test avatar
	Ogre::Animation::setDefaultInterpolationMode(Ogre::Animation::IM_LINEAR);
//    Animation::setDefaultInterpolationMode(Animation::IM_SPLINE);
//   Ogre::Animation::setDefaultRotationInterpolationMode(Ogre::Animation::RIM_LINEAR);
	Ogre::Animation::setDefaultRotationInterpolationMode(Ogre::Animation::RIM_SPHERICAL);

   // Set animation lods
   std::vector<AnimationLod> lodList;
   AnimationLod lod;
   lod.mDistanceSquared = gSavedSettings.getVector3("Animation_Lod").mV[VX];
   lod.updateTimeframe = gSavedSettings.getVector3("Animation_Lod").mV[VY];
   lodList.push_back(lod);
   LLOgreObject::setGlobalAnimationLods(lodList);

    createSky();

    // Create water plane
    Ogre::Plane plane(Ogre::Vector3::NEGATIVE_UNIT_Z, 0);
    Ogre::MeshManager::getSingleton().createPlane("WaterMesh",
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, plane,
        5000, 5000, 10, 10, true, 1, 1, 1, Ogre::Vector3::UNIT_X);
    Ogre::Entity* pPlaneEnt = mSceneMgr->createEntity( "Water", "WaterMesh" );
    pPlaneEnt->setMaterialName(mWaterMaterial);
    pPlaneEnt->setCastShadows(false);
	mWaterNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("Water");
	mWaterNode->attachObject(pPlaneEnt);

    // Create reverse water plane (for looking from inside water)
    Ogre::Plane planeRev(Ogre::Vector3::UNIT_Z, 0);
    Ogre::MeshManager::getSingleton().createPlane("WaterMeshReverse",
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, planeRev,
        5000, 5000, 10, 10, true, 1, 1, 1, Ogre::Vector3::NEGATIVE_UNIT_X);
    Ogre::Entity* pPlaneEntRev = mSceneMgr->createEntity("WaterReverse", "WaterMeshReverse");
    pPlaneEntRev->setMaterialName(mWaterMaterial);
    pPlaneEntRev->setCastShadows(false);
    mWaterReverseNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("WaterReverse");
	mWaterReverseNode->attachObject(pPlaneEntRev);

	setWaterHeight(mWaterHeight);

	updateGlobalLight(); // Get direction/colors from world

	setSLContext();
}

void LLOgreRenderer::createSky(bool showSky)
{
   Ogre::Quaternion orientation(Ogre::Degree(mGenericSkyParameters.mAngle), toOgreVector(mGenericSkyParameters.mAngleAxis));
   switch(mCurrentSkyType)
   {
   case REXSKY_BOX:
      {
         mSceneMgr->setSkyDome(false, "");
         Ogre::MaterialPtr skyMaterial = Ogre::MaterialManager::getSingleton().getByName(mGenericSkyParameters.mMaterial);
         if (skyMaterial.isNull() == false)
         {
            skyMaterial->setReceiveShadows(false);
            if (mSkyboxImages.size() == 6)
               skyMaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setCubicTextureName(&mSkyboxImages[0], false);
         }
         mSceneMgr->setSkyBox(showSky, mGenericSkyParameters.mMaterial, mGenericSkyParameters.mDistance, mGenericSkyParameters.mDrawFirst, orientation);
         break;
      }
   case REXSKY_DOME:
      {
         mSceneMgr->setSkyBox(false, "");
         mSceneMgr->setSkyDome(showSky, mGenericSkyParameters.mMaterial, mSkydomeParameters.mCurvature, mSkydomeParameters.mTiling,
               mGenericSkyParameters.mDistance, mGenericSkyParameters.mDrawFirst, orientation, mSkydomeParameters.mXSegments,
               mSkydomeParameters.mYSegments, mSkydomeParameters.mYSegments_keep);
         break;
      }
   case REXSKY_NONE:
      {
         mSceneMgr->setSkyDome(false, "");
         mSceneMgr->setSkyBox(false, "");
         break;
      }
   }
}

//! Return whether to use shadows on objects
bool LLOgreRenderer::getUseObjectShadows()
{
    return mUseObjectShadows;
}

//! Toggle statistics overlay
void LLOgreRenderer::toggleDebug()
{
	if (!mDebugOverlay) return;

	// Context switch just to be certain
	setOgreContext();

	if (mDebugOverlay->isVisible())
	{
		mDebugOverlay->hide();
	}	
	else
	{	
		mDebugOverlay->show();
	}

	setSLContext();
}

Ogre::ParticleSystem *LLOgreRenderer::createParticleSystem(const Ogre::String &name, bool flipYCoord)
{
   static unsigned int current = 0;
   current++;


   setOgreContext();
   Ogre::ParticleSystem* system = 0;
    try
    {
        system = mSceneMgr->createParticleSystem("particle" + 
             Ogre::StringConverter::toString(current), name);

        if (system)
        {
            if (flipYCoord)
            {
                // Haxor billboard orientation for downloaded textures
                Ogre::ParticleSystemRenderer* renderer = system->getRenderer();
                if ((renderer) && (renderer->getType() == "billboard"))
                {
                    Ogre::BillboardSet* set = ((Ogre::BillboardParticleRenderer*)renderer)->getBillboardSet();
                    if (set)
                    {
                        Ogre::FloatRect reverse(0.f, 1.f, 1.f, 0.f);
    
                        set->setTextureCoords(&reverse, 1);
                    }
                }
            }
        }
    }
    catch (Ogre::Exception e)
    {
        llwarns << "Unable to create particle system " << name << "." << llendl;
    }

    setSLContext();

    return system;
}

//! Update stats if statistics overlay is visible
void LLOgreRenderer::updateDebug()
{
	// Get this always so that it is cleared for next frame, even if debug is invisible
	mTextureUpdateCount = RexOgreLegacyMaterial::getTextureUpdateCount();

	if (!mDebugOverlay) return;
	if (!mDebugOverlay->isVisible()) return;

	static Ogre::String currFps = "FPS: ";
	static Ogre::String avgFps = "Average FPS: ";
	static Ogre::String objectBuild = "Geometry Builds: ";
	static Ogre::String textureBuild = "Texture Builds: ";
	static Ogre::String tris = "Triangle Count: ";
	static Ogre::String batches = "Batch Count: ";
	
	try {
		Ogre::OverlayElement* guiAvg = Ogre::OverlayManager::getSingleton().getOverlayElement("Core/AverageFps");
		Ogre::OverlayElement* guiCurr = Ogre::OverlayManager::getSingleton().getOverlayElement("Core/CurrFps");
		Ogre::OverlayElement* guiObject = Ogre::OverlayManager::getSingleton().getOverlayElement("Core/WorstFps");
		Ogre::OverlayElement* guiTexture = Ogre::OverlayManager::getSingleton().getOverlayElement("Core/BestFps");

		const Ogre::RenderTarget::FrameStats& stats = mWindow->getStatistics();

		guiAvg->setCaption(avgFps + Ogre::StringConverter::toString(stats.avgFPS));
		guiCurr->setCaption(currFps + Ogre::StringConverter::toString(stats.lastFPS) + " (" + Ogre::StringConverter::toString(mLastFrameTime, 2) + " ms render)");
		guiObject->setCaption(objectBuild + Ogre::StringConverter::toString(mObjectUpdateCount));
		guiTexture->setCaption(textureBuild + Ogre::StringConverter::toString(mTextureUpdateCount));

		Ogre::OverlayElement* guiTris = Ogre::OverlayManager::getSingleton().getOverlayElement("Core/NumTris");
		guiTris->setCaption(tris + Ogre::StringConverter::toString(stats.triangleCount));

		Ogre::OverlayElement* guiBatches = Ogre::OverlayManager::getSingleton().getOverlayElement("Core/NumBatches");
		guiBatches->setCaption(batches + Ogre::StringConverter::toString(stats.batchCount));
	}
	catch(...) { /* ignore */ }
}

//! Update camera location & orientation from the SL viewer camera
void LLOgreRenderer::updateCamera()
{
	if (!mInitialized) return;
   
	LLVector3 origin = LLViewerCamera::getInstance()->getOrigin();
	LLVector3 dir = LLViewerCamera::getInstance()->getAtAxis();

   mCamera->setPosition(toOgreVector(origin));// + offset);
	mCamera->setDirection(toOgreVector(dir));

	// Set fogging (different under water)
	if (origin.mV[VZ] >= mWaterHeight)
	{
		// In stencil shadow mode, disable above-water fog (some weird stuff happens with shadow volumes at distance)
		if (mSceneMgr->getShadowTechnique() != Ogre::SHADOWTYPE_STENCIL_ADDITIVE)
		{
			mSceneMgr->setFog(Ogre::FOG_LINEAR, toOgreColor(mSkyColor * mBackgroundColor), 0.001, mFogStart, mFogEnd);
		}
		else
		{
			mSceneMgr->setFog(Ogre::FOG_NONE);
		}
		mViewport->setBackgroundColour(toOgreColor(mSkyColor * mBackgroundColor));

      if ((mUnderWater) || (mForceUnderWaterUpdate))
      {
         mUnderWater = false;
         mForceUnderWaterUpdate = false;

         try
         {
            Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName(mGenericSkyParameters.mMaterial);
            // show skybox
            if (mCurrentSkyType == REXSKY_BOX)
            {
               Ogre::SceneNode *node = mSceneMgr->getSkyBoxNode();
               Ogre::SceneNode::ObjectIterator iter = node->getAttachedObjectIterator();
               while (iter.hasMoreElements())
               {
                  Ogre::MovableObject *object = iter.getNext();
                  material = Ogre::MaterialManager::getSingleton().getByName(object->getName());
                  material->setFog(true);
               }
            } else
            {
               if (material.isNull() == false) material->setFog(true);
            }
         } catch (...) { }
   
         // In "real" reX mode, use farclipdistance as set, otherwise cap to slightly past fog end
         if (gInPureRex)
             mCamera->setFarClipDistance(mCameraFarClip);
         else
             mCamera->setFarClipDistance(mFogEnd + 10.0f);

		   try
		   {
			   Ogre::Entity* waterPlane = mSceneMgr->getEntity("Water");
			   waterPlane->setVisible(true);		
               Ogre::Entity* waterPlaneReverse = mSceneMgr->getEntity("WaterReverse");
			   waterPlaneReverse->setVisible(false);		
		   }
		   catch (...) {}
	   }
	}
	else
	{
		mSceneMgr->setFog(Ogre::FOG_LINEAR, toOgreColor(mSkyColor * mWaterFogColor), 0.001, mWaterFogStart, mWaterFogEnd);
		mViewport->setBackgroundColour(toOgreColor(mSkyColor * mWaterFogColor));

      if ((!mUnderWater) || (mForceUnderWaterUpdate))
      {
         mUnderWater = true;
         mForceUnderWaterUpdate = false;

         try
         {
            Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName(mGenericSkyParameters.mMaterial);

            // A heavy duty way of hiding the skybox when underwater
            if (mCurrentSkyType == REXSKY_BOX)
            {
               Ogre::SceneNode *node = mSceneMgr->getSkyBoxNode();
               Ogre::SceneNode::ObjectIterator iter = node->getAttachedObjectIterator();
               while (iter.hasMoreElements())
               {
                  Ogre::MovableObject *object = iter.getNext();
                  material = Ogre::MaterialManager::getSingleton().getByName(object->getName());
                  material->setFog(true, Ogre::FOG_LINEAR, toOgreColor(mSkyColor * mWaterFogColor), 0.001f, 0.1f, 0.2f);
               }
            }
            else if (material.isNull() == false)
            {
               material->setFog(true, Ogre::FOG_LINEAR, toOgreColor(mSkyColor * mWaterFogColor), 0.001f, 0.1f, 0.2f);
            }
         } catch (...) { }

         mCamera->setFarClipDistance(mWaterFogEnd + 10.f);
		   try
		   {
			   Ogre::Entity* waterPlane = mSceneMgr->getEntity("Water");
			   waterPlane->setVisible(false);		
            Ogre::Entity* waterPlaneReverse = mSceneMgr->getEntity("WaterReverse");
			   waterPlaneReverse->setVisible(true);		
         }
		   catch (...) {}
	   }
	}
}

//! Set water height
void LLOgreRenderer::setWaterHeight(F32 height)
{
	mWaterHeight = height;
	if (mWaterNode)
	{
		mWaterNode->setPosition(Ogre::Vector3(0, 0, -height));
	}
	if (mWaterReverseNode)
	{
		mWaterReverseNode->setPosition(Ogre::Vector3(0, 0, -height));
	}
}

void LLOgreRenderer::overrideAmbientLighting(bool setOverride, const LLVector3 &sunDirection, const LLColor4 &lightColour)
{
   mSunOverride->mOverride = setOverride;
   mSunOverride->mSunDirection = sunDirection;
   mSunOverride->mLightColour = lightColour;
}

void LLOgreRenderer::endOverrides()
{
    if (mSunOverride->mOverride)
    {
        LLWLParamManager::instance()->mAnimator.mIsRunning = true;
        LLWLParamManager::instance()->mAnimator.mUseLindenTime = true;
        gSky.setOverrideSun(FALSE);
        mSunOverride->mOverride = false;
    }

    if (mForceFog)
    {
        mForceFog = false;
        mWaterFogStart = mOriginalWaterFogStart;
        mWaterFogEnd = mOriginalWaterFogEnd;
        mWaterFogColor = mOriginalWaterFogColor;
    }

    setWaterHeight(mOriginalWaterHeight);

    mForceUnderWaterUpdate = true; // We need to update things that get updated when switching underwater/overwater

    //! \todo reset skybox if changed
}

//! Update global lighting from SL sky world object
void LLOgreRenderer::updateGlobalLight()
{
	if ((!mInitialized) || (!mGlobalLight)) return;

	LLViewerRegion* region = gAgent.getRegion();
	if (region)
	{
		setWaterHeight(region->getWaterHeight());
	}


   if (mSunOverride->mOverride == false)
   {
   	LLColor4 ambient = gSky.getTotalAmbientColor();
      mSceneMgr->setAmbientLight(toOgreColor(ambient) * 0.7);
   }

	LLVector3 sunDir;
	LLColor4 sunColor;

	if (gSky.getSunDirection().mV[2] >= NIGHTTIME_ELEVATION_COS)
	{
      sunDir.setVec(gSky.getSunDirection());
		sunColor.setVec(gSky.getSunDiffuseColor());
		mGlobalLight->setCastShadows(true);
	}
	else
	{
		sunDir.setVec(gSky.getMoonDirection());
		sunColor.setVec(gSky.getMoonDiffuseColor()); // No multiply 
        mGlobalLight->setCastShadows(true);
// 		mGlobalLight->setCastShadows(false); // No nighttime shadows
	}

	F32 max_color = llmax(sunColor.mV[0], sunColor.mV[1], sunColor.mV[2]);
	if (max_color > 1.f)
	{
		sunColor *= 1.f/max_color;
	}
	sunColor.clamp();
	sunDir.normVec();
   if (mSunOverride->mOverride)
   {
      sunColor = mSunOverride->mLightColour;
      sunDir = mSunOverride->mSunDirection;
   }

   mGlobalLight->setDirection(-toOgreVector(sunDir));
   mGlobalLight->setDiffuseColour(toOgreColor(sunColor));

	// Use emissive color of skybox material to control sky color
	static LLColor4 prevSkyColor;
	mSkyColor = sunColor;

	if (mSkyColor != prevSkyColor)
	{
		prevSkyColor = mSkyColor;

      if (mCurrentSkyType != REXSKY_NONE)
      {
         Ogre::MaterialPtr skyMaterial = Ogre::MaterialManager::getSingleton().getByName(mGenericSkyParameters.mMaterial);
         if (!skyMaterial.isNull())
         {
				skyMaterial->setSelfIllumination(toOgreColor(mSkyColor));
				// Need to change also skybox material clones
				try
				{
					for (int i = 0; i < 6; i++)
					{
						Ogre::String skyBoxEntityName = mSceneMgr->getName() + "SkyBoxPlane" + Ogre::StringConverter::toString(i);
						Ogre::Entity* skyBoxEntity = mSceneMgr->getEntity(skyBoxEntityName);
						Ogre::MaterialPtr skyBoxMaterial = skyBoxEntity->getSubEntity(0)->getMaterial();
						skyBoxMaterial->setSelfIllumination(toOgreColor(mSkyColor));

                        if (mUnderWater)
                        {
                            skyBoxMaterial->setFog(true, Ogre::FOG_LINEAR, toOgreColor(mSkyColor * mWaterFogColor), 0.001f, 0.1f, 0.2f);
                        }
					}
				}
				catch (...) {}
         }
      }

		// Set water shader parameters
		try
		{
			Ogre::Entity* waterPlane = mSceneMgr->getEntity("Water");
			waterPlane->getSubEntity(0)->setCustomParameter(0, Ogre::Vector4(mSkyColor[0], mSkyColor[1], mSkyColor[2], 1.0));

			Ogre::Entity* waterPlaneReverse = mSceneMgr->getEntity("WaterReverse");
			waterPlaneReverse->getSubEntity(0)->setCustomParameter(0, Ogre::Vector4(mSkyColor[0], mSkyColor[1], mSkyColor[2], 1.0));
		}
		catch (...) {}
	}
}

//! Update geometry for viewer objects
void LLOgreRenderer::updateGeometry()
{
	if (!mInitialized) return;

	LLFastTimer t(LLFastTimer::FTM_REBUILD);

	mObjectUpdateCount = 0;

	std::list<LLViewerObject*>::iterator i = mObjectUpdates.begin();

	const F32 max_update_time = 0.010f; // 10 ms max. update time
	LLTimer update_timer;

	while (i != mObjectUpdates.end())
	{
		(*i)->updateOgreGeometry();

		i = mObjectUpdates.erase(i);
		++mObjectUpdateCount;

		if (update_timer.getElapsedTimeF32() >= max_update_time) break;
	}
}

//! Queue object for geometry updating
void LLOgreRenderer::addOgreUpdate(LLViewerObject* object)
{
	mObjectUpdates.push_back(object);
}

//! Remove queued object geometry update (for the case the object is destroyed before next render)
void LLOgreRenderer::cancelOgreUpdate(LLViewerObject* object)
{
	std::list<LLViewerObject*>::iterator i = mObjectUpdates.begin();

	while (i != mObjectUpdates.end())
	{
		if ((*i) == object)
		{
			i = mObjectUpdates.erase(i);
			return;
		}
		else ++i;
	}
}

//! Precache update of objects
void LLOgreRenderer::precache()
{
	if (!mInitialized) return;
    if (!gInRex) return;

	if (mOgreContextCount != 0)
        llwarns << "In Ogre context before updating: shouldn't be!" << llendl;

	setOgreContext();
    updateGeometry();
	setSLContext();
}

//! Render one frame
void LLOgreRenderer::render()
{
	if (!mInitialized) return;

	if (mOgreContextCount != 0)
		llwarns << "In Ogre context before rendering: shouldn't be!" << llendl;

	setOgreContext();

	updateGlobalLight();
	updateCamera();
	updateGeometry();
	updateDebug();
   if (mGenerateLM)
   {
      generateLightmap();
   }

   mFlashAnimationManager->update();
   RexClientEffect::update();

	LLFastTimer t(LLFastTimer::FTM_RENDER_GEOMETRY);

	// Render and measure time it took	
	LLTimer render_timer;
    LLOgreHtmlRenderer::render();


    // Cap to 1/4 second so particle effects don't go crazy while in modal dialog
    Ogre::FrameEvent evt;
    evt.timeSinceLastFrame = gFrameIntervalSeconds;
    if (evt.timeSinceLastFrame > 0.25) evt.timeSinceLastFrame = 0.25;
    evt.timeSinceLastEvent = evt.timeSinceLastFrame;

	mRoot->_fireFrameStarted(evt);
    #ifdef LL_WINDOWS
	    mRoot->_updateAllRenderTargets();
    #else
        // In non-win32 systems we don't have externalGLControl, so we shouldn't swap
        //! \todo does this update RTT targets needed by main window?
	mWindow->update(false);
    #endif
	mRoot->_fireFrameEnded();

	mLastFrameTime = render_timer.getElapsedTimeF32() * 1000.0f; 

	setSLContext();
}

//! Convert LL vector to Ogre
Ogre::Vector3 LLOgreRenderer::toOgreVector(const LLVector3& llVector)
{
   return Ogre::Vector3(-llVector.mV[VX], llVector.mV[VY], -llVector.mV[VZ]);
}

//! Convert Ogre vector to LL
LLVector3 LLOgreRenderer::toSLVector(const Ogre::Vector3& ogreVector)
{
   return LLVector3(-ogreVector.x, ogreVector.y, -ogreVector.z);
}

//! Convert LL quaternion to Ogre
Ogre::Quaternion LLOgreRenderer::toOgreQuaternion(const LLQuaternion& llQuaternion)
{
	return Ogre::Quaternion(llQuaternion.mQ[VS], -llQuaternion.mQ[VX], llQuaternion.mQ[VY], -llQuaternion.mQ[VZ]);
}

//! Convert LL 4-component color to Ogre
Ogre::ColourValue LLOgreRenderer::toOgreColor(const LLColor4& llColor)
{
	return Ogre::ColourValue(llColor.mV[0], llColor.mV[1], llColor.mV[2], llColor.mV[3]);
}

//! Convert LL 3-component color to Ogre
Ogre::ColourValue LLOgreRenderer::toOgreColor(const LLColor3& llColor)
{
	return Ogre::ColourValue(llColor.mV[0], llColor.mV[1], llColor.mV[2]);
}

//! Convert Ogre colour to LL 4-component color
LLColor4 LLOgreRenderer::toLLColor(const Ogre::ColourValue& ogreColour)
{
   return LLColor4(ogreColour.r, ogreColour.g, ogreColour.b, ogreColour.a);
}

//! Convert Ogre colour to LL 4-component color
LLColor3 LLOgreRenderer::toLLColor3(const Ogre::ColourValue& ogreColour)
{
   return LLColor3(ogreColour.r, ogreColour.g, ogreColour.b);
}

//! Save Ogre's OpenGL context for later recall
void LLOgreRenderer::saveOgreContext()
{
#ifdef LL_WINDOWS
	mOgreDC = wglGetCurrentDC();
	mOgreGLRC = wglGetCurrentContext();
#else
    mOgreDrawable = glXGetCurrentDrawable();
    mOgreDisplay = glXGetCurrentDisplay();
    mOgreCtx = glXGetCurrentContext();
    Ogre::LogManager::getSingleton().logMessage(
        "Saved Ogre context: " +
        Ogre::StringConverter::toString((unsigned long)mOgreCtx) +
        " display: " + Ogre::StringConverter::toString((unsigned long)mOgreDisplay) + 
        " drawable: " + Ogre::StringConverter::toString((unsigned long)mOgreDrawable));
#endif

	mOgreContextSaved = true;
	// Reset the context switch counter (we are in Ogre context when saving)
	mOgreContextCount = 1;
}

//! Save SL's OpenGL context for later recall
void LLOgreRenderer::saveSLContext()
{
#ifdef LL_WINDOWS
	mSLDC = wglGetCurrentDC();
	mSLGLRC = wglGetCurrentContext();
#else
    mSLCtx = glXGetCurrentContext();
    mSLDrawable = glXGetCurrentDrawable();
    mSLDisplay = glXGetCurrentDisplay();
    Ogre::LogManager::getSingleton().logMessage( 
        "Saved SL context: " +
        Ogre::StringConverter::toString((unsigned long)mSLCtx) +
        " display: " + Ogre::StringConverter::toString((unsigned long)mSLDisplay) + 
        " drawable: " + Ogre::StringConverter::toString((unsigned long)mSLDrawable));
#endif

	mSLContextSaved = true;
	// Reset the context switch counter
	mOgreContextCount = 0;
}

//! Switch to Ogre OpenGL context
void LLOgreRenderer::setOgreContext()
{
	if (mOgreContextSaved)
	{
		// Check for nested switching: no need to set again
		if (mOgreContextCount == 0)
		{
#ifdef LL_WINDOWS
			wglMakeCurrent(mOgreDC, mOgreGLRC);
#else
			glXMakeCurrent(mOgreDisplay, mOgreDrawable, mOgreCtx);
#endif
		}
		++mOgreContextCount;
	}
}

//! Switch to SL OpenGL context
void LLOgreRenderer::setSLContext()
{
	if (mSLContextSaved)
	{		
		if (mOgreContextCount == 0)
		{
			llwarns << "Switch to SL OpenGL context when already in SL context" << llendl;
			return;
		}

		// Return to SL context only when nested switch count is 0 
		--mOgreContextCount;
		if (mOgreContextCount == 0)
		{
#ifdef LL_WINDOWS
            wglMakeCurrent(mSLDC, mSLGLRC);
#else
            glXMakeCurrent(mSLDisplay, mSLDrawable, mSLCtx);
#endif

            // Try to fix Nvidia VBO weirdness: always unbind after mucking around in Ogre context
	        LLVertexBuffer::unbind();
		}
	}
}

void LLOgreRenderer::updateSky(RexSkyType type, const std::vector<std::string> &images, F32 curvature, F32 tiling)
{
   if (type == REXSKY_NONE)
   {
      mCurrentSkyType = type;
      createSky(false);
   } else
   {
      std::map<std::string, int> indexMap;
      std::map<std::string, int>::const_iterator suffixIter;
      indexMap["_fr"] = 0;
      indexMap["_bk"] = 1;
      indexMap["_lf"] = 2;
      indexMap["_rt"] = 3;
      indexMap["_up"] = 4;
      indexMap["_dn"] = 5;

      mCurrentSkyBoxImageCount = 0;
      mSkyBoxImageCount = 0;
      mSkyboxImages.clear();
      mSkyboxImages.reserve(6);

      size_t max = std::min(images.size(), (size_t)6);
      size_t n;
      for (n=0 ; n<max ; ++n)
      {
         std::string imageStr = images[n];
         int index = (int)n;
         if (imageStr.size() > 3)
         {
            std::string suffix = imageStr.substr(imageStr.size() - 3);
            suffixIter = indexMap.find(suffix);
            if (suffixIter != indexMap.end())
            {
               index = suffixIter->second;
               imageStr = imageStr.substr(0, imageStr.size() - 3);
            }
         }
         LLUUID imageId(imageStr);

         if (imageId.notNull())
         {
            mSkyBoxImageCount++;

            LLViewerImage *image = gImageList.getImage(imageId, FALSE, TRUE, 0, 0, gAgent.getRegionHost());
            SkyImageData *imageData = new SkyImageData;
            imageData->mIndex = index;
            imageData->mType = type;
            imageData->mCurvature = curvature;
            imageData->mTiling = tiling;
            image->setLoadedCallback(onSkyTextureLoaded, 0, FALSE, FALSE, (void*)imageData);
         }
      }
   }
}

void LLOgreRenderer::process_rex_skybox_info(LLMessageSystem* msg, void**)
{
    llinfos << "Got RexSkyboxInfo" << llendl;

    const char* imageNames[6] = {"FrontTextureID", "BackTextureID", "LeftTextureID",
                                 "RightTextureID", "TopTextureID", "BottomTextureID"};

    LLOgreRenderer::getPointer()->mCurrentSkyBoxImageCount = 0;
    LLOgreRenderer::getPointer()->mSkyboxImages.clear();
    LLOgreRenderer::getPointer()->mCurrentSkyType = REXSKY_BOX;

    LLViewerImage* imgs[6];
    for (int i = 0; i < 6; ++i)
    {
        LLUUID imageID;
        msg->getUUID("Textures", imageNames[i], imageID);

        llinfos << "Fetching image " << imageID << llendl;
        imgs[i] = gImageList.getImage(imageID, FALSE, TRUE, 0, 0, gAgent.getRegionHost());
        SkyImageData *imageData = new SkyImageData;
        imageData->mIndex = i;
        imageData->mType = REXSKY_BOX;
        imgs[i]->setLoadedCallback(onSkyTextureLoaded, 0, FALSE, FALSE, imageData);
    }
}

// static
void LLOgreRenderer::onSkyTextureLoaded(BOOL success, LLViewerImage* src_vi, LLImageRaw* src_raw, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata)
{
   LLOgreRenderer *renderer = LLOgreRenderer::getPointer();
   if (renderer->isInitialized())
   {
      renderer->_onSkyTextureLoaded(success, src_vi, src_raw, aux_src, discard_level, final, userdata);
   }
}

void LLOgreRenderer::_onSkyTextureLoaded(BOOL success, LLViewerImage* src_vi, LLImageRaw* src_raw, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata)
{
   SkyImageData *imageData = (SkyImageData *)userdata;
   mCurrentSkyType = imageData->mType;
   switch (mCurrentSkyType)
   {
   case REXSKY_BOX:
      _onSkyboxTextureLoaded(success, src_vi, imageData);
      break;
   case REXSKY_DOME:
      _onSkyDomeTextureLoaded(success, src_vi,  imageData);
      break;
   }

   delete imageData;
}

void LLOgreRenderer::_onSkyboxTextureLoaded(BOOL success, LLViewerImage* src_vi, const SkyImageData *imageData)
{
   // the skybox textures are upside down, because there is no easy way to flip cubemap textures vertically.
   // (normal texture is easy by using Ogre::TextureUnitState::setTextureScale(1, -1)).
   if (success == TRUE && src_vi->isMissingAsset() == false)
   {
      mSkyboxImages.resize(6);
      mSkyboxImages[imageData->mIndex] = src_vi->getOgreTexture()->getName();

      mCurrentSkyBoxImageCount++;
      if (mCurrentSkyBoxImageCount == mSkyBoxImageCount)
      {
         LLOgreRenderer::getPointer()->setOgreContext();
         Ogre::MaterialPtr skyMaterial = Ogre::MaterialManager::getSingleton().getByName(mGenericSkyParameters.mMaterial);
         if (!skyMaterial.isNull())
         {
            skyMaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setCubicTextureName(&mSkyboxImages[0], false);
//            skyMaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureScale(1, -1);
            createSky();
         }

         // lets create scaled down cubemap texture for water reflections.
         // We apparently need to create a separate cubemap texture for the environment map as
         // there's no way to use the existing ones. It's also good idea to scale the cubemap down since
         // it doesn't need to be very accurate.

         // For ease, we create scaled down versions of the skybox textures and then copy the new textures into
         // cubemap faces and afterwards remove the new scaled down textures.
         std::vector<Ogre::TexturePtr> scaledTextures;
         scaledTextures.reserve(mSkyBoxImageCount);
         size_t n;
         for (n=0 ; n<mSkyboxImages.size() ; ++n)
         {
            Ogre::String defaultTextureName = mSkyboxImages[n];
            if (defaultTextureName.empty() == false)
            {
               Ogre::TexturePtr defaultTexture = Ogre::TextureManager::getSingleton().getByName(defaultTextureName);
               if (defaultTexture.isNull() == false)
               {
                  Ogre::uint width = std::max(defaultTexture->getWidth() / 2, (Ogre::uint)2);
                  Ogre::uint height = std::max(defaultTexture->getHeight() / 2, (Ogre::uint)2);

                  Ogre::TexturePtr newTexture = Ogre::TextureManager::getSingleton().createManual("scaled" + Ogre::StringConverter::toString(n),
                     Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D, width, height, Ogre::MIP_UNLIMITED, defaultTexture->getBuffer()->getFormat());
                  defaultTexture->copyToTexture(newTexture);
                  scaledTextures.push_back(newTexture);
               }
            }
         }

         Ogre::TexturePtr cubeTex = (Ogre::TexturePtr)Ogre::TextureManager::getSingleton().getByName("RexSkyboxCubic");
         if (cubeTex.isNull())
         {
            Ogre::TexturePtr tex = scaledTextures[0];
             cubeTex = Ogre::TextureManager::getSingleton().createManual("RexSkyboxCubic",
                 Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                 Ogre::TEX_TYPE_CUBE_MAP, tex->getWidth(), tex->getHeight(), tex->getDepth(),
                 tex->getNumMipmaps(), tex->getBuffer()->getFormat());
         }
         const int side[] = {3, 2, 4, 5, 0, 1};
         for (int i = 0; i < mSkyBoxImageCount; ++i)
         {
            size_t index = side[i];
            if (index < scaledTextures.size())
            {
               Ogre::TexturePtr face = scaledTextures[index];
               Ogre::HardwarePixelBufferSharedPtr buffer = cubeTex->getBuffer(i);
               buffer->blit(face->getBuffer());
            }
         }

         for (n=0 ; n<scaledTextures.size() ; ++n)
         {
            Ogre::TextureManager::getSingleton().remove(static_cast<Ogre::ResourcePtr>(scaledTextures[n]));
         }

         // Update water reflection
         Ogre::MaterialPtr waterMaterial = Ogre::MaterialManager::getSingleton().getByName(mWaterMaterial);
         if (!waterMaterial.isNull())
         {
             llinfos << "Updating water material..." << llendl;
             Ogre::TextureUnitState* state = waterMaterial->getTechnique(0)->getPass(0)->getTextureUnitState(1);
             state->setCubicTextureName("RexSkyboxCubic", true);
             state->setTextureCoordSet(1);
//             state->setTextureScale(1, -1); // not works for cubemaps
         }
         LLOgreRenderer::getPointer()->setSLContext();
      }
   }
}


void LLOgreRenderer::_onSkyDomeTextureLoaded(BOOL success, LLViewerImage* src_vi, const SkyImageData *imageData)
{
   if (success == TRUE && src_vi->isMissingAsset() == false)
   {
      mSkydomeParameters.mCurvature = imageData->mCurvature;
      mSkydomeParameters.mTiling = imageData->mTiling;

      const Ogre::String &texture = src_vi->getOgreTexture()->getName();
      Ogre::MaterialPtr skyMaterial = Ogre::MaterialManager::getSingleton().getByName(mGenericSkyParameters.mMaterial);
      if (!skyMaterial.isNull())
      {
          skyMaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(texture);
          skyMaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureAddressingMode(Ogre::TextureUnitState::TAM_WRAP);
          createSky();
      }
   }
}


void LLOgreRenderer::generateLightmaps(size_t maxLights, float sharpness, int maxTexSize, bool useGlobalLight, bool debug)
{
   if (mGenerateLM == false)
   {
      mGenerateLM = true;
      mCurrentLMObject = 0;
      mLightmapObjects.clear();

      LLLightMap::setDefaultMaxLights(maxLights);
      LLLightMap::setDefaultPixelsPerUnit(sharpness);
      LLLightMap::setDefaultTexSize(maxTexSize);
//      LLLightMap::setDefaultTexSize(1024);
      LLLightMap::setDefaultUseGlobalLight(useGlobalLight);
//      LLLightMap::setDefaultUseGlobalLight(false);
      LLLightMap::setDefaultDebug(debug);


      bool somethingSelected = LLSelectMgr::getInstance()->getSelection()->isEmpty() == false;
      if (somethingSelected)
         mLightmapObjects.reserve(gObjectList.getNumObjects() + 2);

      mLightmapObjects.push_back(0); // To skip first few objects when generating lightmaps, otherwise the first object might not get lighted properly
      mLightmapObjects.push_back(0);

      S32 n;
      for (n=0 ; n<gObjectList.getNumObjects() ; ++n)
      {
         LLViewerObject *object = gObjectList.getObject(n);
         if (!somethingSelected || object->isSelected())
         {
            LLOgreObject *ogreObject = object->getOgreObject();
            if (ogreObject)
            {
               Ogre::Entity *entity = ogreObject->getEntity();
               if (entity)
               {
                  const Ogre::Any& any = entity->getUserAny();
                  if (!any.isEmpty())
                  {
                     LLOgreObject* obj = Ogre::any_cast<LLOgreObject*>(any);
                     if (obj)
                     {
                        LLViewerObject* vo = obj->getViewerObject();
                        if (vo)
                        {
                        static_cast<LLVOVolume*>(object)->setUseClonedMaterial(true);
                        object->markOgreUpdate();
                        
                        mLightmapObjects.push_back(ogreObject);
                     }
                  }
               }
            }
         }
      }
      }

      if (mLightmapObjects.empty())
         mGenerateLM = false; // a bit of defensive programming, should really never happen
   }
}

void LLOgreRenderer::generateLightmap()
{
   if (mObjectUpdates.empty() == false)
      return;

   LLOgreObject *ogreObject = mLightmapObjects[mCurrentLMObject];
   if (ogreObject)
   {
      Ogre::Entity *entity = ogreObject->getEntity();
      LLEntityLightMap lightmap(entity, &mMeshCache);
   }

   mCurrentLMObject++;
   if (mCurrentLMObject == mLightmapObjects.size())
   {
      mGenerateLM = false;
      mMeshCache.clear();
      mLightmapObjects.clear();
   }
}

const Ogre::Light* LLOgreRenderer::getGlobalLight() const
{
   return mGlobalLight;
}

// Per-poly picking code from Ogre wiki
// http://www.ogre3d.org/wiki/index.php/Raycasting_to_the_polygon_level

LLViewerObject* LLOgreRenderer::pick(float x, float y)
{
    // create the ray to test
    if (!mCamera) return NULL;
    Ogre::Ray ray = mCamera->getCameraToViewportRay(x, y);

    return raycast(ray);
}

LLViewerObject* LLOgreRenderer::raycast(const Ogre::Ray &ray)
{
	if (!mInitialized) return NULL;
    if (!gOgreRender) return NULL;

    Ogre::Vector3 result;
    LLViewerObject* closestObject = NULL;

    // Create ray query on first time
    if (!mRayQuery)
        mRayQuery = mSceneMgr->createRayQuery(Ogre::Ray());
    if (!mRayQuery) return NULL;

    mRayQuery->setSortByDistance(true); 

    // check we are initialised
    if (mRayQuery != NULL)
    {
        // set ray for the query
        mRayQuery->setRay(ray);

        // execute the query, returns a vector of hits
        if (mRayQuery->execute().size() <= 0)
        {
            // raycast did not hit an objects bounding box
            return NULL;
        }
    }
    else
    {
        return NULL;
    }   

    // at this point we have raycast to a series of different objects bounding boxes.
    // we need to test these different objects to see which is the first polygon hit.
    // there are some minor optimizations (distance based) that mean we wont have to
    // check all of the objects most of the time, but the worst case scenario is that
    // we need to test every triangle of every object.
    setOgreContext(); // Need access to Ogre vertexbuffers

    // Pre-pass to get the best priority available, so we can break early
    Ogre::RaySceneQueryResult &query_result = mRayQuery->getLastResults();
    int best_priority = -1000000;
    for (size_t qr_idx = 0; qr_idx < query_result.size(); qr_idx++)
    {
        // only check this result if its a hit against an entity
        if ((query_result[qr_idx].movable != NULL) &&
            (query_result[qr_idx].movable->getMovableType().compare("Entity") == 0))
        {
            // get the entity to check
            Ogre::Entity *pentity = static_cast<Ogre::Entity*>(query_result[qr_idx].movable);

            // See that the entity associates to a viewerobject
            const Ogre::Any& any = pentity->getUserAny();
            if (any.isEmpty()) continue;
            LLOgreObject* obj = Ogre::any_cast<LLOgreObject*>(any);
            if (!obj) continue;
            LLViewerObject* vo = obj->getViewerObject();
            if (!vo) continue;
            if (vo->getPCode() != LL_PCODE_VOLUME) continue;

            LLVOVolume* volobjp = (LLVOVolume*)vo;
            int current_priority = volobjp->getRexSelectionPriority();
            if (current_priority > best_priority)
                best_priority = current_priority;
        }
    }

    // Now do the real pass
    Ogre::Real closest_distance = -1.0f;
    int closest_priority = -1000000;
    Ogre::Vector3 closest_result;
    for (size_t qr_idx = 0; qr_idx < query_result.size(); qr_idx++)
    {
        // stop checking if we have found a raycast hit that is closer
        // than all remaining entities, and the priority found is best possible
        if ((closest_distance >= 0.0f) &&
            (closest_distance < query_result[qr_idx].distance) && (closest_priority >= best_priority))
        {
             break;
        }
       
        // only check this result if its a hit against an entity
        if ((query_result[qr_idx].movable != NULL) &&
            (query_result[qr_idx].movable->getMovableType().compare("Entity") == 0))
        {
            // get the entity to check
            Ogre::Entity *pentity = static_cast<Ogre::Entity*>(query_result[qr_idx].movable);

            // See that the entity associates to a viewerobject
            const Ogre::Any& any = pentity->getUserAny();
            if (any.isEmpty()) continue;
            LLOgreObject* obj = Ogre::any_cast<LLOgreObject*>(any);
            if (!obj) continue;
            LLViewerObject* vo = obj->getViewerObject();
            if (!vo) continue;
            if (vo->getPCode() != LL_PCODE_VOLUME) continue;

            LLVOVolume* volobjp = (LLVOVolume*)vo;
            int current_priority = volobjp->getRexSelectionPriority();
            if (current_priority < closest_priority)
                continue;

            // mesh data to retrieve
            size_t vertex_count;
            size_t index_count;
            Ogre::Vector3 *vertices = 0;
            unsigned long *indices = 0;

            // get the mesh information
            getMeshInformation(pentity->getMesh(), vertex_count, vertices, index_count, indices,
                              pentity->getParentNode()->getWorldPosition(),
                              pentity->getParentNode()->getWorldOrientation(),
                              pentity->getParentNode()->_getDerivedScale());

            bool new_closest_found = false;
            // Paranoid check
            if ((vertices) && (indices))
            {
                // test for hitting individual triangles on the mesh
                for (int i = 0; i < static_cast<int>(index_count); i += 3)
                {
                    // check for a hit against this triangle
                    std::pair<bool, Ogre::Real> hit = Ogre::Math::intersects(ray, vertices[indices[i]],
                        vertices[indices[i+1]], vertices[indices[i+2]], true, false);

                    // if it was a hit check if its the closest
                    if (hit.first)
                    {
                        if ((closest_distance < 0.0f) ||
                            (hit.second < closest_distance) || (current_priority > closest_priority))
                        {
                            if (current_priority >= closest_priority)
                            {
                                // this is the closest/best so far, save it off
                                closest_distance = hit.second;
                                closest_priority = current_priority;
                                new_closest_found = true;
                            }
                        }
                    }
                }
            }

            // free the verticies and indicies memory
            delete[] vertices;
            delete[] indices;

            // if we found a new closest raycast for this object, update the
            // closest_result before moving on to the next object.
            if (new_closest_found)
            {
                closest_result = ray.getPoint(closest_distance);
                closestObject = vo;
            }
        }
    }

    setSLContext();

    // return the result
    if (closest_distance >= 0.0f)
    {
        // raycast success
        result = closest_result;

        return closestObject;
    }
    else
    {
        // raycast failed
        return NULL;
    } 
}

// Get the mesh information for the given mesh.
// Code found in Wiki: www.ogre3d.org/wiki/index.php/RetrieveVertexData
void LLOgreRenderer::getMeshInformation(const Ogre::MeshPtr mesh,
                                size_t &vertex_count,
                                Ogre::Vector3* &vertices,
                                size_t &index_count,
                                unsigned long* &indices,
                                const Ogre::Vector3 &position,
                                const Ogre::Quaternion &orient,
                                const Ogre::Vector3 &scale)
{
    bool added_shared = false;
    size_t current_offset = 0;
    size_t shared_offset = 0;
    size_t next_offset = 0;
    size_t index_offset = 0;

    vertex_count = index_count = 0;

    // Calculate how many vertices and indices we're going to need
    for (unsigned short i = 0; i < mesh->getNumSubMeshes(); ++i)
    {
        Ogre::SubMesh* submesh = mesh->getSubMesh( i );

        // We only need to add the shared vertices once
        if(submesh->useSharedVertices)
        {
            if( !added_shared )
            {
                vertex_count += mesh->sharedVertexData->vertexCount;
                added_shared = true;
            }
        }
        else
        {
            vertex_count += submesh->vertexData->vertexCount;
        }

        // Add the indices
        index_count += submesh->indexData->indexCount;
    }


    // Allocate space for the vertices and indices
    vertices = new Ogre::Vector3[vertex_count];
    indices = new unsigned long[index_count];

    added_shared = false;

    // Run through the submeshes again, adding the data into the arrays
    for ( unsigned short i = 0; i < mesh->getNumSubMeshes(); ++i)
    {
        Ogre::SubMesh* submesh = mesh->getSubMesh(i);

        Ogre::VertexData* vertex_data = submesh->useSharedVertices ? mesh->sharedVertexData : submesh->vertexData;

        if((!submesh->useSharedVertices)||(submesh->useSharedVertices && !added_shared))
        {
            if(submesh->useSharedVertices)
            {
                added_shared = true;
                shared_offset = current_offset;
            }

            const Ogre::VertexElement* posElem =
                vertex_data->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);

            Ogre::HardwareVertexBufferSharedPtr vbuf =
                vertex_data->vertexBufferBinding->getBuffer(posElem->getSource());

            unsigned char* vertex =
                static_cast<unsigned char*>(vbuf->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));

            // There is _no_ baseVertexPointerToElement() which takes an Ogre::Real or a double
            //  as second argument. So make it float, to avoid trouble when Ogre::Real will
            //  be comiled/typedefed as double:
            //      Ogre::Real* pReal;
            float* pReal;

            for( size_t j = 0; j < vertex_data->vertexCount; ++j, vertex += vbuf->getVertexSize())
            {
                posElem->baseVertexPointerToElement(vertex, &pReal);

                Ogre::Vector3 pt(pReal[0], pReal[1], pReal[2]);

                vertices[current_offset + j] = (orient * (pt * scale)) + position;
            }

            vbuf->unlock();
            next_offset += vertex_data->vertexCount;
        }


        Ogre::IndexData* index_data = submesh->indexData;
        size_t numTris = index_data->indexCount / 3;
        Ogre::HardwareIndexBufferSharedPtr ibuf = index_data->indexBuffer;

        bool use32bitindexes = (ibuf->getType() == Ogre::HardwareIndexBuffer::IT_32BIT);

        unsigned long*  pLong = static_cast<unsigned long*>(ibuf->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));
        unsigned short* pShort = reinterpret_cast<unsigned short*>(pLong);


        size_t offset = (submesh->useSharedVertices)? shared_offset : current_offset;

        if ( use32bitindexes )
        {
            for ( size_t k = 0; k < numTris*3; ++k)
            {
                indices[index_offset++] = pLong[k] + static_cast<unsigned long>(offset);
            }
        }
        else
        {
            for ( size_t k = 0; k < numTris*3; ++k)
            {
                indices[index_offset++] = static_cast<unsigned long>(pShort[k]) +
                    static_cast<unsigned long>(offset);
            }
        }

        ibuf->unlock();

        current_offset = next_offset;
    }
} 

void LLOgreRenderer::setCameraFOV(F32 field_of_view)
{
    mCameraFOV = field_of_view;

    if (mCamera)
    {
        mCamera->setFOVy(Ogre::Radian(field_of_view));
    }
}
