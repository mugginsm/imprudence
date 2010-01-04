/** 
 * @file llogre.h
 * @brief LLOgreRenderer class header file
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#ifndef LL_LLOGRE_H
#define LL_LLOGRE_H

#include "OGRE/OgrePrerequisites.h"
#include "OGRE/OgreString.h"
#include "llmeshcache.h"
#include "lldispatcher.h"
#include "rexsky.h"

#ifdef LL_WINDOWS
// Includes for the ugly Windows OpenGL context switching hack
#define WIN32_LEAN_AND_MEAN
#include <wingdi.h>
#else
// Definition of 'Status' is an ugly hack, but necessary here
// Status is defined then undefined somewhere in the mix of
// includes, and we end up without it being defined somehow.
// Note that we immediately undefine it afterwards, to prevent
// odd bugs. This will force a compilation error if it is relied
// upon later.
#define Status int
#include <GL/glx.h>
#undef Status
#endif

#include <list>

// forward declarations
namespace Ogre
{
	class Root;
	class Camera;
	class Viewport;
	class RenderWindow;
	class SceneManager;
	class Overlay;
	class Texture;
	class GLPlugin;
	class OctreePlugin;
	class ParticleFXPlugin;
	class CgPlugin;
}

class LLMessageSystem;
class LLImageRaw;
class LLViewerImage;
class LLViewerWindow;
class LLViewerObject;
class LLOgreObject;
class LLColor3;
class LLFontGL;
class LLOgreAssetLoader;
class LLExternalTextureSource;
class LLOgreMediaEngine;
class LLOgrePostprocess;
class RexFlashAnimationManager;
struct AmbientLightOverride;
typedef boost::shared_ptr<AmbientLightOverride> AmbientLightOverridePtr;

//! Dispatcher for fog setting generic message.
class FogDispatchHandler : public LLDispatchHandler
{
public:
   //! Handle dispatch
	virtual bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& string);

   static const std::string mMessageID;
};

//! Dispatcher for water height generic message.
class WaterDispatchHandler : public LLDispatchHandler
{
public:
   //! Handle dispatch
	virtual bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& string);

   static const std::string mMessageID;
};

//! Dispatcher for mesh animation generic message.
class MeshAnimDispatchHandler : public LLDispatchHandler
{
public:
   //! Handle dispatch
	virtual bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& string);

   static const std::string mMessageID;
};

//! Dispatcher for sun override generic message.
class AmbientLightDispatchHandler : public LLDispatchHandler
{
public:
   //! Handle dispatch
	virtual bool operator()(const LLDispatcher* dispatcher, const std::string& key,
		                     const LLUUID& invoice, const sparam_t& string);
   static const std::string mMessageID;
};

//! Dispatcher for sky generic message.
class SkyDispatchHandler : public LLDispatchHandler
{
public:
   //! Handle dispatch
	virtual bool operator()(const LLDispatcher* dispatcher, const std::string& key,
		                     const LLUUID& invoice, const sparam_t& string);
   static const std::string mMessageID;
};


class LLOgreRenderer
{
public:
	//! Constructor: does no actual initialization
	LLOgreRenderer();

	//! Destructor, destroys Ogre root object & assetloader
	~LLOgreRenderer();

    //! Return ogre renderer
    static LLOgreRenderer *getPointer();

	//! Init Ogre using an existing SLViewer window
	void init(LLViewerWindow* pViewerWindow);

	//! Shutdown: destroy Ogre root, self-installed managers and plugins
	void shutdown();

	//! Are we good to go?
	bool isInitialized() { return mInitialized; }

   //! Force fog to specified parameters
   void setForceFog(Ogre::Real fogStart, Ogre::Real fogEnd, const Ogre::ColourValue &fogColour);

   //! Return current Ogre render window
   Ogre::RenderWindow *getRenderWindow() {return mWindow; }

	//! Return scene manager
	Ogre::SceneManager* getSceneMgr() { return mSceneMgr; }

	//! Return asset loader
	LLOgreAssetLoader* getAssetLoader() { return mAssetLoader; }

   //! Return viewport
   Ogre::Viewport *getViewport() { return mViewport; }

   //! Return camera
   Ogre::Camera *getCamera() { return mCamera; }

   //! Return postprocess manager
   LLOgrePostprocess* getPostprocess() { return mPostprocess; }

   RexFlashAnimationManager *getFlashAnimationManager() { return mFlashAnimationManager; }

   //! Set up ogre scene from an XML file
   /*! Sets camera, light, shadow and various other settings for the scene(s)
   */
   void setupScene(const char *filename);

    //! Create a test scene with lights, shadows, several avatars, objects...
    //void createTestScene();
	
	//! Resize renderwindow when SL viewer window has been resized
	void resizeViewerWindow(unsigned int width, unsigned int height);
	
	//! Render one Ogre frame
	void render();

    //! Precache geometry updates
    void precache();

	//! Toggle Ogre statistics overlay
	void toggleDebug();

   //! Creates new ogre particle system
   Ogre::ParticleSystem *createParticleSystem(const Ogre::String &name, bool flipYCoord = false);

	//! Queue object geometry updates
	void addOgreUpdate(LLViewerObject* object);

	//! Remove queued object geometry update (for the case the object is destroyed before next render)
	void cancelOgreUpdate(LLViewerObject* object);
	
	//! Switch to Ogre GL context (ugly hack)
	void setOgreContext();

	//! Switch to SLViewer GL context (ugly hack)
	void setSLContext();

    //! Return whether currently in Ogre GL context
    bool isInOgreContext() const { return mOgreContextCount > 0; }

	//! Convert Ogre vector to Linden vector
    /*! We convert to "imported model space" so that model imported from 3dsmax doesn't need to be rotated.
	 */
	static LLVector3 toSLVector(const Ogre::Vector3& ogreVector);
	
	//! Convert Linden vector to Ogre
	static Ogre::Vector3 toOgreVector(const LLVector3& llVector);
	
	//! Convert Linden quaternion to Ogre
	static Ogre::Quaternion toOgreQuaternion(const LLQuaternion& llQuaternion);

	//! Convert Linden color to Ogre
	static Ogre::ColourValue toOgreColor(const LLColor4& llColor);
	static Ogre::ColourValue toOgreColor(const LLColor3& llColor);
   //! Convert Ogre colour to linden color
   static LLColor4 toLLColor(const Ogre::ColourValue& ogreColour);
   static LLColor3 toLLColor3(const Ogre::ColourValue& ogreColour);

	//! Save SLViewer GL context (ugly hack)
	void saveSLContext();

    //! Return whether to use shadows on objects
    bool getUseObjectShadows();

    //! Return default legacy material name, to base legacy materials on
    const std::string& getBaseMaterialName();

	//! Set global water height
 	void setWaterHeight(F32 height);

   //! Overrides ambient colour. This is hard overriding, the setting can't be changed on Ogre side by the user anymore.
   void overrideAmbientLighting(bool setOverride, const LLVector3 &sunDirection, const LLColor4 &lightColour);

   //! update sky info
   /*!
       \param type Type of the sky, box, dome, none
       \param images List of image uuids for the sky
       \param curvature If sky type is dome, curvature of the dome
       \param tiling If sky type is dome, tiling of the dome
   */
   void updateSky(RexSkyType type, const std::vector<std::string> &images, F32 curvature, F32 tiling);

    //! End all scene overrides (called when smooth teleporting)
    void endOverrides();

    //! Set camera FOV
    void setCameraFOV(F32 field_of_view);

   static void process_rex_skybox_info(LLMessageSystem* msg, void**);

   //! Generates lightmaps to all primitives in the scene
   /*! If scene contains more lights than maxLights, closest lights are used.
       \note Global light is not included in maxLights, so light maps for it are always generated

       \param maxLights Maximum number of lights affected by a surface.
       \param sharpness Sharpness of the light maps, higher number means sharper shadows, but bigger textures and longer processing times
       \param maxTexSize Maximum light map texture size. With this parameter it's possible to limit light maps to sane sizes. 
                         Larger areas require larger texture, so with really large prims, you may get textures > 1024.
       \param debug Show debug lightmaps to help fixing any problems with light mapping
   */
   void generateLightmaps(size_t maxLights = 3, float sharpness = 20.f, int maxTexSize = 256, bool useGlobalLight = true, bool debug = false);

   //! Generate lightmap for single primitive
   void generateLightmap();
   
    //! Returns the scene's global light (sun/moon)
    const Ogre::Light* getGlobalLight() const;

    //! Pick object from screen
    /*!
        \param x horizontal screen coordinate
        \param y vertical screen coordinate
    */
    LLViewerObject* pick(float x, float y);

    //! Perform per-polygon raycast and return the nearest object that was hit or NULL if no object was hit by the ray
    /*!
        \param ray the ray
        \param maxDistance Objects farther away than this are not returned, set < 0 to return objects at any distance
        \param originEntity Entity that's origin of the ray and should be ignored
    */
    LLViewerObject* raycast(const Ogre::Ray &ray);

	enum SceneShadowingMethod
	{
		ShadowsNone,	///< No shadows whatsoever.
		ShadowsBool,	///< Use a boolean texture that represents shadowed areas. Really not recommended as the Ogre method is broken in so many ways, but supported as backward compatibility.
		ShadowsPCF		///< Use a 4-tap PCF shadow mapping method.
	};

	//! @return The used scene shadowing method.
	SceneShadowingMethod	getShadowingMethod() const { return mShadowMethod; }

	//! @return The material that is used to render terrains.
	const char *getSceneTerrainMaterialName() const;

protected:
   struct SkyImageData
   {
      //! sky type for which the image is for
      RexSkyType mType;
      //! index of the image
      int mIndex;
      F32 mCurvature;
      F32 mTiling;
   };

	//! Load local Ogre resources
	void initResources();

	//! Initialize renderer: create renderwindow, viewport, scenemanager etc.
	void initRenderer();

   //! Create sky box/plane/dome
   void createSky(bool showSky = true);

	//! Process queued object geometry updates
	void updateGeometry();

	//! Update debug stats if statistics overlay visible
	void updateDebug();

	//! Update Ogre camera from SLViewer camera
	void updateCamera();

	//! Update sun/moon/ambient lighting
	void updateGlobalLight();
	
	//! Save Ogre GL context (ugly hack)
	void saveOgreContext();

    //! Get information from mesh. Note: you must deallocate indices/vertices afterwards yourself!
    void getMeshInformation(const Ogre::MeshPtr mesh,
                                size_t &vertex_count,
                                Ogre::Vector3* &vertices,
                                size_t &index_count,
                                unsigned long* &indices,
                                const Ogre::Vector3 &position,
                                const Ogre::Quaternion &orient,
                                const Ogre::Vector3 &scale);


    //! Generic sky texture loaded event handler
    void _onSkyTextureLoaded(BOOL success, LLViewerImage* src_vi, LLImageRaw* src_raw, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata);
    //! Sky texture loaded event handler for skybox
    void _onSkyboxTextureLoaded(BOOL success, LLViewerImage* src_vi, const SkyImageData *imageData);
    //! Sky texture loaded event handler for skydome
    void _onSkyDomeTextureLoaded(BOOL success, LLViewerImage* src_vi, const SkyImageData *imageData);

    //! callback for fetching sky texture(s) from server
    static void onSkyTextureLoaded(BOOL success, LLViewerImage* src_vi, LLImageRaw* src_raw, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata);


    //! Force fog generic message handler
    static FogDispatchHandler smDispatchForceFog;

    //! Water height generic message handler
    static WaterDispatchHandler smDispatchWaterHeight;

    //! Mesh animation generic message handler
    static MeshAnimDispatchHandler smDispatchMeshAnimation;

    //! Ambient light generic message handler
    static AmbientLightDispatchHandler smDispatchAmbientLight;

    //! sky generic message handler
    static SkyDispatchHandler smDispatchSky;

private:
	//! Ogre root object
	Ogre::Root* mRoot;

	//! Ogre OpenGL rendersystem plugin
	Ogre::GLPlugin* mGLPlugin;

	//! Ogre octree scenemanager plugin
	Ogre::OctreePlugin* mOctreePlugin;

	//! Ogre particle plugin
	Ogre::ParticleFXPlugin* mParticlePlugin;

	//! Ogre Cg manager plugin
	Ogre::CgPlugin* mCgPlugin;

    //! TightVNC viewer plugin
    Ogre::Plugin* mVNCPlugin;

	//! Camera
	Ogre::Camera* mCamera;

	//! Scene manager
	Ogre::SceneManager* mSceneMgr;

	//! Rendering window
	Ogre::RenderWindow* mWindow;

	//! Viewport
	Ogre::Viewport* mViewport;

	//! Postprocess
	LLOgrePostprocess* mPostprocess;

	//! Global light for sun & moon
	Ogre::Light* mGlobalLight;

   //! If true, use fog setting sent from server, otherwise use normal fog
   bool mForceFog;

	//! Initialized flag (indicates Ogre is ok to use for creating/moving objects etc.)
 	bool mInitialized;
	
	//! Determines what kind of shadowing method is used, PCF, modulative boolean or none.
	SceneShadowingMethod mShadowMethod;

	//! Debug overlay
	Ogre::Overlay* mDebugOverlay;	

	//! Asset loader
	LLOgreAssetLoader* mAssetLoader;

   //! flash animation manager
   RexFlashAnimationManager *mFlashAnimationManager;

	//! SLViewer window
	LLViewerWindow* mViewerWindow;

   //! override for ambient colours
   AmbientLightOverridePtr mSunOverride;

   //! If set to true, lightmaps will be generated
   bool mGenerateLM;
   S32 mCurrentLMObject;
   MeshInformationCache mMeshCache;

	//! Water plane & reverse plane scene nodes
	Ogre::SceneNode* mWaterNode;
	Ogre::SceneNode* mWaterReverseNode;

	//! OS-specific handle of SLViewer window
	void* mViewerWindowHandle;

	//! Object geometry update list
	std::list<LLViewerObject*> mObjectUpdates;

   //! list of skybox image names
   std::vector<std::string> mSkyboxImages;

    //! Water height (updated later by region)
    F32 mWaterHeight;
   //! Original water height
    F32 mOriginalWaterHeight;
    //! Water material
    std::string mWaterMaterial;

    //! Above water fog start distance
    F32 mFogStart;
    //! Above water fog end distance
    F32 mFogEnd;
    //! Below water fog start distance
    F32 mWaterFogStart;
    //! Below water fog end distance
    F32 mWaterFogEnd;

    //! Viewport background / above water fog color
    LLColor4 mBackgroundColor;
    //! Sky modulation color
    LLColor4 mSkyColor;
    //! Water fog color
    LLColor4 mWaterFogColor;

    //! Camera near clip distance
    F32 mCameraNearClip;
    //! Camera far clip distance
    F32 mCameraFarClip;

    //! Original water fog start distance (for restoring after override)
    F32 mOriginalWaterFogStart;
    //! Original water fog end distance (for restoring after override)
    F32 mOriginalWaterFogEnd;
    //! Original water fog color (for restoring after override)
    LLColor4 mOriginalWaterFogColor;

    //! Whether to use shadows on objects (in legacy material mode)
    bool mUseObjectShadows;

    //! Whether to force disable of shadows, when bad RTT support
    bool mForceDisableShadows;

    //! Base material name
    std::string mBaseMaterialName;

    //! Camera FOV
    F32 mCameraFOV;

	//! Saved Ogre and SL contexts for OpenGL context switching  
#ifdef LL_WINDOWS
	HDC mOgreDC;
	HDC mSLDC;
	HGLRC mOgreGLRC;
	HGLRC mSLGLRC;
#else
    GLXContext mOgreCtx;
    GLXContext mSLCtx;
    GLXDrawable mOgreDrawable;
    GLXDrawable mSLDrawable;
    Display* mOgreDisplay;
    Display* mSLDisplay;
#endif
	bool mOgreContextSaved;
	bool mSLContextSaved;

	//! Ogre context switch counter, allows nested switches to Ogre context
	int mOgreContextCount;

    //! Use testscene with ogre.
    bool mUseTestScene;
    Ogre::AnimationState *mTestAvatarAnimState;
    Ogre::Real mTestAvatarAnimationSpeed;

	//! Path to use for resources
	Ogre::String mResourcePath;

	//! Amount of objects geometries updated last frame
    int mObjectUpdateCount;

    //! Amount of textures created last frame
	int mTextureUpdateCount;

	//! Time in milliseconds it took to render last frame
	F32 mLastFrameTime;

    //! Ray query
    Ogre::RaySceneQuery* mRayQuery;



    typedef std::vector<LLExternalTextureSource*> ExternalTextureSourceList;
    ExternalTextureSourceList mExternalTextureSources;

    LLOgreMediaEngine* mMediaEngine;

    int mSkyBoxImageCount;
    int mCurrentSkyBoxImageCount;

    bool mUnderWater;
    bool mForceUnderWaterUpdate;

    //! Current type of the sky
    RexSkyType mCurrentSkyType;

    //! Generic sky parameters common to all sky types
    RexSkyParameters mGenericSkyParameters;

    //! parameters for skydome
    RexSkydomeParameters mSkydomeParameters;

    //! List of objects that we should generate lightmaps for
    std::vector<LLOgreObject *> mLightmapObjects;
};

#endif //LL_LLOGRE_H
