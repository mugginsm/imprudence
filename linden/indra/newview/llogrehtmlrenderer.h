/** 
 * @file llogrehtmlrenderer.cpp
 * @brief LLOgreHtmlRenderer class header file
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#ifndef LL_LLOGREHTMLRENDERER_H					
#define LL_LLOGREHTMLRENDERER_H

#include "llfloater.h"
#include "llframetimer.h"
#include "llmousehandler.h"
#include "llmozlib2.h"
#include "llwebbrowserctrl.h"
#include <OGRE/Ogre.h>

class LLOgreHtmlRenderer;
class LLViewerImage;

//! Observer for WebBrowser
/*!
    For static html pages, we need to know when the page is loaded so we can
    render the page to texture. We implement onNavigateComplete() to track it.

*/
class LLOgreHtmlObserver : public LLWebBrowserCtrlObserver
{
public:
   //! Create default observer
   LLOgreHtmlObserver() : mRenderer(0) {}

   //! Set html renderer for this observer
   void setRenderer(LLOgreHtmlRenderer *renderer) { mRenderer = renderer; }

   //! We have navigated to new url, lets render it
	virtual void onNavigateComplete( const EventType& eventIn );

private:
   //! Renderer that uses this observer
   LLOgreHtmlRenderer *mRenderer;
};

//! Renders webpages and html into ogre material
/*! Should be created only after ogre is initialized.
    Uses non power-of-two textures, so may be slow / not work on older 3d cards.
    We spawn a new webcontrol for each texture.

    use power-of-two textures?
    Share one webcontrol for all textures, with only one texture dynamic at any one time?
*/
class LLOgreHtmlRenderer :	public LLFloater
{
public:
   //! constructor
   LLOgreHtmlRenderer(const std::string &name, const std::string &material = std::string(), const std::string &url = std::string(), float updateInterval = -1.f);

   //! destructor
   ~LLOgreHtmlRenderer();


   //! Render html into texture. 
   /*! Should be called (automatically) for every non-static renderers.
       For static renderers, only call when texture needs to be updated.
   */
   void _render();

   //! Show control
   static void show();

   //! Set new url
   void setUrl(const std::string& url);

   //! Creates new html renderer to image from url.
	//! @param updateInterval The update interval of the page in seconds, < 0 denotes a static page (no update after loaded)
   static LLOgreHtmlRenderer *fromUrl(LLViewerImage *image, const std::string &url, float updateInterval = -1.f);

   static LLOgreHtmlRenderer* createRenderer(const std::string& materialName, const std::string &url, float updateInterval = -1.f);

   static void releaseRenderer(const std::string& materialName);
   static void releaseAll();

   //! Returns true if renderer is static (content not rendered constantly)
   bool isStatic() const;

	//! If the content is not static, returns the update interval (in seconds).
	float getRefreshInterval() const;

   //! instance
   static LLOgreHtmlRenderer *sInstance;

   //! Render all texture html renderers
   /*! Renders according to specified fps, not realtime
   */
   static void render();

    //! Draw floater. Suppressed.
    void draw();

   // Hack to support LLFocusMgr
	virtual BOOL isView() { return FALSE; }

   //! Returns the web control
   /*! Allows one to get the web browser control and use it directly.
   */
   LLWebBrowserCtrl *getControl();

	// Virtual functions inherited from LLMouseHandler
	bool _mouseDown(S32 x, S32 y, MASK mask);
	bool _mouseUp(S32 x, S32 y, MASK mask);

   static bool mouseDown(S32 x, S32 y, MASK mask);
   static bool mouseUp(S32 x, S32 y, MASK mask);
//	BOOL handleHover(S32 x, S32 y, MASK mask);
   /*
	BOOL handleScrollWheel(S32 x, S32 y, S32 clicks);
	BOOL handleDoubleClick(S32 x, S32 y, MASK mask);
	BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);
	BOOL handleRightMouseUp(S32 x, S32 y, MASK mask);
	BOOL handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect_screen);
*/
//   void setMouseCapture(BOOL b);
//	BOOL hasMouseCapture();

   void getUVFromScreen(S32 *x, S32 *y);

private:
   //! update texture with new data
   /*!
       \data New texture data
   */
   void setTexture(const unsigned char *data, S32 width, S32 height);

   //! Create new dynamic texture.
   /*! Call when texture size changes

       \param width width of the new texture
       \param height height of the new texture
   */
   void createTexture(S32 width, S32 height);

   //! Helper function for getting barycentric coordinates out of triangle and point
   Ogre::Vector3 getBarycentricCoordinates(const Ogre::Vector2 &p1, const Ogre::Vector2 &p2,
                                                      const Ogre::Vector2 &p3, const Ogre::Vector2 &p);

   //! Helper function for getting mesh information out of Ogre mesh
   void getMeshInformation(const Ogre::MeshPtr mesh, size_t &vertexCount, Ogre::Vector3* &vertices,
         size_t &indexCount, unsigned long* &indices, Ogre::Vector2* &texCoords,
         const Ogre::Vector3 &position, const Ogre::Quaternion &orientation,
         const Ogre::Vector3 &scale);

   //! ogre material where html is rendered to
   Ogre::MaterialPtr mMaterial;

   Ogre::TexturePtr mTexture;

   //! Web browser control that handles mozlib functionality
   LLWebBrowserCtrl *mWebBrowser;

   //! browser window dimensions
   S32 mBrowserDepth;
   S32 mBrowserWidth;
   S32 mBrowserHeight;

   static LLFrameTimer sElapsedTime;

	//! The update interval, if <= 0, the page is static and not periodically refreshed.
	float mUpdateInterval;

	//! How long since the last update. Not used if mUpdateInterval <= 0.
	float mUpdateCounter;

   typedef std::vector<LLOgreHtmlRenderer*> RendererList;

   //! List of all texture html renderers that need to be rendered every frame
   static RendererList sRenderers;

   //! List of all texture html renderers 
   static std::vector<LLOgreHtmlRenderer*> sAllRenderers;

   static U32 sId;

   //! Does this renderer use external material or internal (debug) material
   bool mExternalMaterial;

   // temp client-side only stuff
   Ogre::MeshPtr mPlaneMesh;
   Ogre::Entity *mPlaneEnt;
   Ogre::SceneNode *mPlaneSN;

   LLOgreHtmlObserver mObserver;

   // Should not be drawn?
   bool mHidden;
};

#endif // LL_LLOGREHTMLRENDERER_H



