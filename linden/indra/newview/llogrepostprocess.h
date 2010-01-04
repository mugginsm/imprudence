/** 
* @file llogrepostprocess.h
* @brief  class header file
*
* Copyright LLOgrePostprocess (c) 2007-2008 LudoCraft
*/

#ifndef LL_LLOGREPOSTPROCESS_H
#define LL_LLOGREPOSTPROCESS_H
#include "lldispatcher.h"
#include <Ogre/Ogre.h>
class HDRListener;

//! Dispatcher for water height generic message.
class PostProcessDispatchHandler : public LLDispatchHandler
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


/*enum EShaderValueType
{
	GPU_VERTEX, 
	GPU_FRAGMENT,
	MAT_AMBIENT,
	MAT_DIFFUSE,
	MAT_SPECULAR,   
	MAT_EMISSIVE
};

struct LLOgreShaderValue
{
	Ogre::String ParamName;
	Ogre::String DisplayName;
	EShaderValueType ShaderValueType;
	float MinValue;
	float MaxValue;
	size_t valueIndex;

	float getValueRange() const {return MaxValue - MinValue;};
};
typedef std::vector<LLOgreShaderValue> ShaderValueList;
typedef ShaderValueList::iterator ShaderValueListIte;*/

// We generate id's for all compositors that we will use
struct LLOgreCompositorItem
{
	LLUUID ID;
	std::string CompositorName;
	//std::vector<std::string> MaterialList;
};

struct LLOgrePostprocessItem
{
   //! The source that enabled
   enum Enabler
   {
      ENABLER_SERVER = 0,
      ENABLER_CLIENT,
      ENABLER_UNKNOWN
   };
	LLOgreCompositorItem CompositorItem;
	//std::map<std::string, ShaderValueList> MaterialValueList;
	bool EffectEnabled;
   Enabler EnabledBy; // Was the effect enabled by server or client
};

class LLOgrePostprocess 
{
public:
	// constructor
	LLOgrePostprocess(){mActiveParameters.setNull();};
	
	// constructor get all viewports in given renderwindow
	LLOgrePostprocess(Ogre::RenderWindow* mRender);
	
	// destructor
	~LLOgrePostprocess();

	// Get all param_named values in material
	//std::vector<std::string> getMaterialNamedParameters(Ogre::MaterialPtr *mat);

	// We find changeable values for a specific material in .control file
	//bool createMaterialParameterList();

	// get compositor's id
	LLUUID* getEffectUUID(const std::string name);

	// get compositor's name
	std::string* getEffectName(const LLUUID id);

	// return the namelist of all compositors that has been defined in resourcemanager. 
	std::vector<std::string> getNameListOfPostprocesses(){ return mPostprocessNameList; }
	
	// if we can find any viewport in given rendredwindow we will add it in to our list
	void loadAllViewportsInRenderWindow(Ogre::RenderWindow* mRender);

	// Set postprocess effect that we want to use in our viewport
   void setPostprocess(Ogre::Viewport* viewport, const Ogre::String name, int position = -1,
         LLOgrePostprocessItem::Enabler enabler = LLOgrePostprocessItem::ENABLER_CLIENT);

   //! Set postprocess effect that we want to use in our viewport.
   /*! Uses index rather than name of the effect to set. Less than optimal since index changes
       when effects are added or removed.

       \note Position in the compositor chain may be important if several effects are enabled.
             Some effects need to be rendered before others. Some effects are automatically 
             positioned, such as HDR which needs to be first effect in the compositor chain.

       \param viewport viewport to enable the effect for
       \param index Index of the effect to enable
       \param position position of the effect in compositor chain
   */
   void setPostprocess(Ogre::Viewport* viewport, unsigned int index, int position = -1,
         LLOgrePostprocessItem::Enabler enabler = LLOgrePostprocessItem::ENABLER_CLIENT);
	
	// We can enable/disable our postprocess effects
	void enablePostprocess(Ogre::Viewport* viewport, const Ogre::String name, bool enable = true);

	// add viewport
	void setViewport(Ogre::Viewport* viewport);

	// Remove viewport
	void removeViewport(Ogre::Viewport* viewport);
	
	// Remove postprocess based on name
	void removePostprocess(Ogre::Viewport* viewport ,const Ogre::String name);

   //! Remove postprocess effect based on index
   void removePostprocess(Ogre::Viewport* viewport, unsigned int index);
	
	// remove all postprocess effects that we are using in viewport
	void removeAllPostprocess(void);

   //! Remove all post process effects set by server
   void removeAllServerSetPostprocess(void);
	
	// Get all postprocess effect names that we want to use and save them in to the namelist
	void updatePostprocessNamelist(void);

	// find and return compositor info search by name
	LLOgrePostprocessItem getCompositorItem(std::string name);

	// tell has the postprocess effect been enabled in specific viewport
	// return false if effect cannot to be found
	BOOL isPostprocessEnabled(Ogre::Viewport* viewport, LLUUID id);

	// set new value to material's named_parameter. Will effect on all compositors that uses spesific material
	// Do nothing if material or parameter cannot to be found.
	// Please note! If you have already enabled postprocess effect and you have changed material value it wont take place until you
	// call notifyMaterialChangeToCompositorChain() function or reload that effect again
	void setMaterialNamedParameter(const Ogre::String materialName, const Ogre::String paramName, const float value);
	void setMaterialNamedParameter(const Ogre::String materialName, const Ogre::String paramName, const Ogre::Vector3 value);
	void setMaterialNamedParameter(const Ogre::String materialName, const Ogre::String paramName, const Ogre::Vector4 value);
	// need to check this! If material parameter is float type and you try to give it int value it will crash
	void setMaterialNamedParameter(const Ogre::String materialName, const Ogre::String paramName, const int value);
	void setMaterialNamedParameter(const Ogre::String materialName, const Ogre::String paramName, const Ogre::ColourValue value);

	// CompositorChain has copy of original material so we need to tell CompositorChain that material has been changed or change wont take a place
	// or you can reload whole compositor by removing and adding it again.
	void notifyMaterialChangeToCompositorChain(Ogre::Viewport* viewport);

	void notifyAllViewportsForMaterialChange();

	void restoreOldSettings(Ogre::Viewport* viewport);

protected:

	//try to find gpuProgramParameter if found return true else return false
	const BOOL _findMaterialParameters(const Ogre::String materialName, const Ogre::String paramName);

	// We will notify specific effect that some of it's value has been changed
	//void notifyValueChange(const LLUUID id, const Ogre::String valueName, Ogre::Real value);

private:
	// List of all compositors and their id's for each viewport
	// Note! if we want to save those changes in xml we should use other key value than viewports or camera,
	// because viewport dont have any good identifier and camera can switch between those viewports
	// so camera seems like a bad idea aswell
	std::map<Ogre::Viewport*, std::vector<LLOgrePostprocessItem>> mVpPostprocessList;

	// vertex or fragment programs named_parameters can be found in here
	Ogre::GpuProgramParametersSharedPtr mActiveParameters;

	// List of all compositors we have loaded
	std::vector<std::string> mPostprocessNameList;

	// name of compositor and it's unique id
	std::vector<LLOgrePostprocessItem> mCompostiorList;

	// HDR listener
	HDRListener* hdrListener;

   PostProcessDispatchHandler mDispatchPostProcess;
};

// from ogre Demo_Compositor sample
class HDRListener: public Ogre::CompositorInstance::Listener
{
public:
	HDRListener();
	virtual ~HDRListener();
	void notifyViewportSize(int width, int height);
	void notifyCompositor(Ogre::CompositorInstance* instance);
	virtual void notifyMaterialSetup(Ogre::uint32 pass_id, Ogre::MaterialPtr &mat);
	virtual void notifyMaterialRender(Ogre::uint32 pass_id, Ogre::MaterialPtr &mat);

protected:
	int mVpWidth, mVpHeight;
	int mBloomSize;
	// Array params - have to pack in groups of 4 since this is how Cg generates them
	// also prevents dependent texture read problems if ops don't require swizzle
	float mBloomTexWeights[15][4];
	float mBloomTexOffsetsHorz[15][4];
	float mBloomTexOffsetsVert[15][4];
};

#endif