/**
 * @file llogreobject.h
 * @brief LLOgreObject class header file
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#ifndef LL_LLOGREOBJECT_H
#define LL_LLOGREOBJECT_H

#include "llogre.h"
#include "llviewercontrol.h"
#include "v3color.h"
#include <list>
#include <vector>
#include <OGRE/OgreAxisAlignedBox.h>
#include <OGRE/OgreAnimationState.h>

// forward declarations
namespace Ogre
{
	class SceneNode;
	class Entity;
	class ManualObject;
	class AnimationState;
	class Light;
    class ParticleSystem;
}

class LLOgreObject;
class LLViewerObject;

//! Structure for describing an ongoing mesh animation
class LLOgreObjectAnim
{
public:
	//! Enumeration of animation states
	enum LLOgreObjectAnimStatus
	{
		STATUS_FADEIN,
		STATUS_PLAY,
		STATUS_FADEOUT,
		STATUS_STOP
	};

    //! High or low priority
    bool mHighPriority;

    //! Autostop at end (default false)
    bool mAutoStop;

	//! Time in milliseconds it takes to fade in/out an animation completely
	F32 mFadePeriod;

	//! Weight of an animation in animation blending, maximum 1.0
	F32 mWeight;

	//! Weight adjust
	F32 mWeightFactor;

	//! How an animation is sped up or slowed down, default 1.0 (original speed)
	F32 mSpeedFactor;

   //! loop animation through mNumRepeats times, or loop if zero
   unsigned long mNumRepeats;

	//! Current status of animation (see enumeration)
	LLOgreObjectAnimStatus mStatus;

	//! The corresponding Ogre mesh animation state
	Ogre::AnimationState* mAnimState;

   //! The corresponding Ogre mesh animation states.
   //! There may be multiple animation states, since they can be linked
   //! And we need to update them all at once.
   std::vector<Ogre::AnimationState*> mLinkedAnimStates;
};

//! Lod for animations
struct AnimationLod
{
   //! (Squared) distance when lod kicks in
   F32 mDistanceSquared;

   //! Time in ms when animations is updated. Set to zero to never update animation
   F32 updateTimeframe;
};

typedef std::vector<AnimationLod> AnimationLodList;

class LLOgreObject// : public LLSimpleListener
{
public:
	//! Constructor. Create a scene node, with optional origin offset (creates additional node)
	LLOgreObject(LLOgreRenderer* renderer, bool useOffset = true);
	
	//! Destructor. Destroy entity and scene node
	virtual ~LLOgreObject();
	
	//! Get scene node
	Ogre::SceneNode* getSceneNode() const { return mSceneNode; }
	
	//! Get entity (if any)
	Ogre::Entity* getEntity() const { return mEntity; }

   //! Returns movable object, or zero if one doesn't exist. This is more generic method than getEntity().
   Ogre::MovableObject *getMovableObject() const;
	
	//! Get manual object (if any)
	Ogre::ManualObject* getManualObject() const { return mManualObject; }

   //! set material name at index materialIndex. The index should be >= 0 and < getNumMaterials()
   void setMaterialName(const std::string &name, U16 materialIndex);

   //! Returns the number of materials that can be used by this object.
   U16 getNumMaterials();

   //! Returns bounding box, or zero if one doesn't exist
   const Ogre::AxisAlignedBox &getBoundingBox();

   //! Sets animation lod bias. Useful if you want a different lod for a specific object.
   void setAnimationLodBias(F32 bias) { mAnimationLodBias = bias; }

   F32 getAnimationLodBias() const { return mAnimationLodBias; }

   //! Enables or disables the use of animation lods for this object
   void enableAnimationLods(bool enable) { mAnimationLodEnabled = enable; }

   //! Are animation lods enabled
   bool getAnimationLodsEnabled() const { return mAnimationLodEnabled; }

   //! Returns the renderer used by this object
   LLOgreRenderer* getRenderer() { return mRenderer; }

   //! Sets animation lods for all objects
   static void setGlobalAnimationLods(const AnimationLodList &lods);

   //! Loads new animations from compatible skeleton.
   /*! \note The skeleton should have same hierarchy for identical bones.
             Binding pose or number of bones need not be same.
       
       \param skeleton Name of the skeleton
       \param Optional list of animations to import.
       \return True if animations were loaded succesfully, false otherwise.
   */
   void loadAnimations(const std::string &skeleton, const StringVector &animations = StringVector());

	//! Enable animation, with optional fade-in period. Returns true if success (animation exists)
	bool enableAnim(const std::string& animName, bool looped = true, F32 fadeIn = 0.0f, bool highPriority = false);
	
    //! Enable an exclusive animation (fades out all other animations with fadeOut parameter)
    bool enableExclusiveAnim(const std::string& animName, bool looped, F32 fadeIn = 0.0f, F32 fadeOutOthers = 0.0f, bool highPriority = false);

    //! Check whether non-looping animation has finished
    /*! If looping, returns always false
     */
    bool hasAnimationFinished(const std::string& animName);

    //! Check whether animation is active
    //! \param checkFadeOut if true, also fade-out (until totally faded) phase is interpreted as "active"
    bool isAnimationActive(const std::string& animName, bool checkFadeOut = true);

	//! Disable animation, with optional fade-out period. Returns true if success (animation exists)
	bool disableAnim(const std::string& animName, F32 fadeout = 0.0f);
	
    //! Disable all animations with the same fadeout period
    void disableAllAnims(F32 fadeOut = 0.0f);

    //! forwards animation to end, useful if animation is played in reverse
   void setAnimToEnd(const std::string& animName);

	//! Set relative speed of active animation. Once disabled, the speed is forgotten! Returns true if success (animation exists)
	bool setAnimSpeed(const std::string& animName, F32 speedFactor);

    //! Change weight of an active animation (default 1.0). Return false if the animation doesn't exist or isn't active
    bool setAnimWeight(const std::string& animName, F32 weight);

	//! Set time position of an active animation.
	bool setAnimTimePosition(const std::string& animName, F32 newPosition);

    //! Set autostop on animation
    bool setAnimAutoStop(const std::string& animName, bool enable);

    //! set number of times the animation is repeated
    bool setAnimNumLoops(const std::string& animName, unsigned int nRepeats);

	//! Update all active animations & their fading for this object
    /*! This doesn't need to be called manually for all objects.
        Returns true if animations were actually updated, false if skipped due to animation lod
    */
	virtual bool _updateAnims(F32 timeStep);

	//! Get animationstate of the entity if necessary to manipulate it directly 
   Ogre::AnimationState* getAnimState(Ogre::Entity *entity, const std::string& animName);

    //! Enable animation rendering for this object (if animations exist)
    /*! Adds the object to list of animable objects and prepares it to be animated.
       Should be called before any animations are used for the object and after
       mesh is set.
      
       \param enable Enable or disable animations for this object
   */
   void enableAnimations(bool enable);

    //! Update all active animations for all animated ogre objects (call once per frame)
	static void updateAnimations(F32 timeStep);

   static void addAnimationState(Ogre::AnimationState *state);
   static void removeAnimationState(Ogre::AnimationState *state);

	//! Set a mesh to be used (creates entity). Optional offset for origin translation. Returns true if success (mesh exists)
	bool setMesh(const std::string& meshName, bool castShadows = true, const LLVector3& offset = LLVector3(0,0,0));

   //! Sets skeleton of the specified name for the object. Use empty string to remove skeleton
	bool setSkeleton(const std::string &skeletonName, const std::string &animation = std::string(), F32 rate = 1.f);

   //! Applies specified skeleton to current mesh, if mesh exists
   bool applySkeleton(const std::string &skeletonName, const std::string &animation = std::string(), F32 rate = 1.f);
	
	//! Create a manual object
	/*! To be filled by caller and then set into use by setManualObject() or converted into a mesh	
	 */
	Ogre::ManualObject* createManualObject();

	//! Set a manual object to be used. Optional offset for origin translation.
	/*! Setup is up to the caller but OgreObject takes care of its destruction, attaching & detaching
	 */
	void setManualObject(Ogre::ManualObject* manualObject, const LLVector3& offset = LLVector3(0,0,0));
	
	//! Convert a manual object into a mesh for use by this OgreObject. 
	/*! If a mesh already exists, the old one will be deleted
	 *  NOTE: manual object will not be deleted automatically
	 * 
	 *  \return name of the mesh
	 */
	std::string convertToMesh(Ogre::ManualObject* manualObject);

	//! Destroy the entity
	/*! \param deleteManualMesh If true, deletes also possible mesh that has been converted from manual object
	 *!        (used by destructor)
	 */
	void removeMesh(bool deleteManualMesh = false);
	
	//! Destroy the manual object
	void removeManualObject();

	//! Destroy the light
	void removeLight();

   //! Destroy billboard
   void removeBillboard();

   //! Get associated viewerobject (if any)
   LLViewerObject* getViewerObject() const { return mViewerObject; }

   //! Set associated viewerobject
   void setViewerObject(LLViewerObject* vo);

    //! Add a particle system to object
    /*!
     * \param name Particle system template name
     * \param flipYCoord Whether billboard texture Y-coords should be flipped. Needed when using downloaded textures
     *
     * \return true if successful
     */
    bool addParticleSystem(const std::string& name, bool flipYCoord);

    //! Remove all particle systems
    void removeParticleSystems();

    //! Reset particle systems (clear emitted particles)
    void resetParticleSystems();

	//! Adjust origin translation offset
	void setOffset(const LLVector3& offset);
    void setOffset(const Ogre::Vector3 ogreOffset);

    const Ogre::Vector3 &getOffset() const;

    //! Set rendering distance (0 = infinite). Does this for all movableobjects attached
    void setRenderingDistance(F32 renderingDistance);

	//! Set visibility
	void setVisible(bool isVisible);

    //! Get visibility
    bool isVisible() const;
	
	//! Set position in 3D world
	void setPosition(const LLVector3& pos);
	
	//! Translate position
	void translate(const LLVector3& vector);
	
	//! Set orientation
    void setOrientation(const LLQuaternion& orientation);
	
	//! Set scale
	void setScale(const LLVector3& scale);

	//! Enable/disable light
	void setLight(bool enable);

   //! Makes the object into billboard.
   /*! 

       \note Resets the scale of the scenenode, as billboards are resized rather than scaled. Scaling is
             problematic as non-uniform scaling causes "wrong" behaviour.
   */
   void setBillboard(bool enable);

   //! Returns true if object is a billboard
   bool isBillboard();

	//! Set light parameters
	void setLightParameters(LLColor3 color, F32 radius, F32 falloff, bool castShadows);

	//! Signal begin of material update. Removes object from object group for this
	void beginMaterialUpdate();

	//! Signal end of material update. Re-adds object to object group
	void endMaterialUpdate();

    //! Disable/enable object grouping (on by default)
    /*! To be set before first calling setMesh() and then not changed anymore */
    void setGroupingEnabled(bool enable);

    //! Get grouping enable status
    bool getGroupingEnabled() const { return mGroupingEnabled; }

    //! Links animation states with the specified entity.
    /*! The specified entity should contain the same animations as the ogre object.
        When animations are enabled, the animationstate's state is copied to the entity's
        animation state.
    */
    void linkAnimationStates(Ogre::Entity *entity);

    //! Clears all linked animation states. Needs to be called whenever new avatar is loaded.
    void clearLinkedAnimationStates();

protected:
   typedef std::vector<Ogre::AnimationState*> AnimationStateVector;
   static AnimationStateVector smAnimationStates; // List of animation states not associated with any LLOgreObject

	//! Ogre renderer we use
	LLOgreRenderer* mRenderer;
	
	//! Scene manager 
	Ogre::SceneManager* mSceneMgr;

	//! Main scene node
	Ogre::SceneNode* mSceneNode;

	//! Offset translation scene node
	Ogre::SceneNode* mOffsetNode;

	//! Attached entity
	Ogre::Entity* mEntity;

	//! Attached manual object
	Ogre::ManualObject* mManualObject;

   //! Attached billboard set
   Ogre::BillboardSet *mBillboardSet;

   //! Default bounding box
   Ogre::AxisAlignedBox mDefaultAABBox;

	//! Attached manual object
	Ogre::Light* mLight;

	//! Animations currently running
	std::list<LLOgreObjectAnim> mActiveAnims;

	//! Bone blend mask of high-priority animations
	Ogre::AnimationState::BoneBlendMask mHighPriorityMask;

	//! Bone blend mask of low-priority animations
	Ogre::AnimationState::BoneBlendMask mLowPriorityMask;

   //! Name of the skeleton used by this object
   std::string mCurrentSkeleton;

   //! name of the current animation
   std::string mCurrentAnimation;

   //! Rate of the current animation
   F32 mCurrentAnimationRate;

   //! Is the mesh (not avatar) skeletally animated
   bool mMeshSkeletallyAnimated;

	//! Cached visibility flag
	bool mIsVisible;

	//! Object material update in progress flag
	bool mMaterialUpdate;

    //! Object grouping enable flag
    bool mGroupingEnabled;

    //! Billboard scale. Since we don't scale billboards but rather set their size, 
    //! we need to keep track of the scale with separate variable
    LLVector3 mBillboardScale;

    //! Particle systems in object
    std::vector<Ogre::ParticleSystem*> mParticleSystems;

    //! Time elapsed since animations were last updated
    F32 mAnimTimeElapsed;

    //! Is animation lod enabled.
    bool mAnimationLodEnabled;

    //! Animation lod bias. Default 1.
    F32 mAnimationLodBias;

    //! List of entites that should share animation states
    std::vector<Ogre::Entity*> mLinkedAnimatedEntities;

    //! Link to LLViewerObject (optional)
    LLViewerObject* mViewerObject;

    //! Global animation lod distances for all ogre objects
    static AnimationLodList smDefaultAnimationLods;

    //! List of ogre objects that are animated.
    static std::vector<LLOgreObject *> mAnimatedObjectList;
};

#endif // LL_LLOGREOBJECT_H
