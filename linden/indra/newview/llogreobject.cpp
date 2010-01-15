/**
 * @file llogreobject.cpp
 * @brief LLOgreObject class implementation
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#include "llviewerprecompiledheaders.h"
#include "llogreobject.h"
#include "llxmltree.h"
#include "OGRE/Ogre.h"

#include "llobjectmanager.h"
//#include "DynamicAnimationManager.h" REX avatar stuff commented out for now -Patrick Sapinski (Friday, January 08, 2010)
#include "llviewerobject.h"


// Static member declaration
//std::vector<LLDynamicAnimation> LLOgreObject::mDynamicAnimations;
std::vector<LLOgreObject *> LLOgreObject::mAnimatedObjectList;
std::vector<AnimationLod> LLOgreObject::smDefaultAnimationLods;
LLOgreObject::AnimationStateVector LLOgreObject::smAnimationStates;

extern BOOL gInRex;
extern BOOL gInPureRex;

//! Constructor: create scene node & offset translation node
LLOgreObject::LLOgreObject(LLOgreRenderer* renderer, bool useOffset) : 
	mRenderer(renderer),
	mSceneMgr(renderer->getSceneMgr()),
	mSceneNode(0),
	mOffsetNode(0),
	mEntity(0),
	mManualObject(0),
	mLight(0),
   mBillboardSet(0),
   mMeshSkeletallyAnimated(false),
	mIsVisible(true),
    mGroupingEnabled(true),
   mAnimTimeElapsed(0),
   mAnimationLodBias(1.0),
   mMaterialUpdate(false),
   mViewerObject(0),
   mCurrentAnimationRate(1.f)
{
	if (!renderer->isInitialized())
		llerrs << "Attempted to create an object before Ogre renderer is initialized" << llendl;

	mSceneNode = mSceneMgr->createSceneNode();
	mSceneMgr->getRootSceneNode()->addChild(mSceneNode);

	// Create offset node if requested (default, but can be optimized away)
	if (useOffset)
	{
		mOffsetNode = mSceneMgr->createSceneNode();
		mSceneNode->addChild(mOffsetNode);
	}

   mDefaultAABBox.setMinimum(Ogre::Vector3(-0.5f, -0.5f, -0.5f));
   mDefaultAABBox.setMaximum(Ogre::Vector3(0.5f, 0.5f, 0.5f));

   // In opensim mode, disable grouping, because region changes induce huge slowdowns (every object gets moved)
   if (!gInPureRex)
       setGroupingEnabled(false);

}

//! Destructor: remove mesh, manual object & light (if exist), destroy scene nodes
LLOgreObject::~LLOgreObject()
{
	// If Ogre has already been shut down at this point, do nothing
	if (!mRenderer->isInitialized()) return;

	removeMesh(true);
	removeManualObject();
	removeLight();
    removeBillboard();
    removeParticleSystems();

	// Destroy offset node if exists
	if (mOffsetNode)
	{
		mSceneNode->removeAndDestroyChild(mOffsetNode->getName());
		mOffsetNode = 0;
	}

	// Detach & destroy the scenenode from root
	mSceneMgr->getRootSceneNode()->removeAndDestroyChild(mSceneNode->getName());
	mSceneNode = 0;
}

Ogre::MovableObject *LLOgreObject::getMovableObject() const
{
   if (mBillboardSet)
      return mBillboardSet;

   if (mEntity)
      return mEntity;

   return 0;
}

//! Set mesh to use. Return TRUE if successful, FALSE if failed
bool LLOgreObject::setMesh(const std::string& meshName, bool castShadows, const LLVector3& offset)
{
//   if (Ogre::MeshManager::getSingleton().resourceExists(meshName) == false)
//      return false;

//   if (meshName.empty())
//      return false;

	// Possible texture etc. loading, so switch context
	mRenderer->setOgreContext();
   
   Ogre::MeshPtr mesh;

   try
   {
      mesh = Ogre::MeshManager::getSingleton().load(meshName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
      if (mMeshSkeletallyAnimated)
      {
         if (Ogre::SkeletonManager::getSingleton().getByName(mCurrentSkeleton).isNull() == false)
         {
            mesh->setSkeletonName(mCurrentSkeleton);
         }
      }
   } catch (Ogre::Exception e)
   {
      Ogre::MeshManager::getSingleton().remove(meshName);
      mRenderer->setSLContext();
      llwarns << e.getFullDescription() << llendl;
      llwarns << "Failed to load mesh: " << meshName << llendl;

      return false;
   }

    // Some extra paranoid check
    if ((mesh->hasSkeleton()) && (mesh->getSkeleton().isNull()))
    {
        // This is somewhat of a nasty thing to do, but Ogre will likely crash if we at this point have a skeletal mesh with nonexisting skeleton
        llwarns << "Resetting mesh " << meshName << " to non-skeletal" << llendl;
        mesh->setSkeletonName(Ogre::String());
    }

	// Remove old mesh if was there
	removeMesh();

	try
	{
		// Create entity with same name as the scenenode + "mesh" for simplicity 
		mEntity = mSceneMgr->createEntity(mSceneNode->getName() + "mesh", meshName);
	} catch (Ogre::Exception e)
	{
      llwarns << e.getFullDescription() << llendl;
		// Error: return to SL context 
		llwarns << "Error creating entity from mesh " << meshName << llendl;

		mRenderer->setSLContext();
		return false;
	}

	mEntity->setNormaliseNormals(true);
	mEntity->setCastShadows(castShadows);
   mEntity->setVisible(mIsVisible);
   applySkeleton(mCurrentSkeleton, mCurrentAnimation, mCurrentAnimationRate);

	// Attach entity to offset translation node if exists
	if (mOffsetNode)
	{
		setOffset(offset);
		mOffsetNode->attachObject(mEntity);
	}
	else
		mSceneNode->attachObject(mEntity);

    LLObjectManager* objMgr = LLObjectManager::getInstance();
    if (objMgr && mGroupingEnabled) objMgr->objectAdded(this);

    // Link to this OgreObject by userdata
    mEntity->setUserAny(Ogre::Any(this));

	// Return to SL context now
	mRenderer->setSLContext();
	return true;
}

bool LLOgreObject::setSkeleton(const std::string &skeletonName, const std::string &animation, F32 rate)
{
   mMeshSkeletallyAnimated = true;
   mCurrentSkeleton = skeletonName;
   mCurrentAnimation = animation;
   mCurrentAnimationRate = rate;
   return applySkeleton(mCurrentSkeleton, animation, rate);
}

bool LLOgreObject::applySkeleton(const std::string &skeletonName, const std::string &animation, F32 rate)
{
   bool success = false;
   mRenderer->setOgreContext();
   if (mEntity && mMeshSkeletallyAnimated)
   {
      mEntity->getMesh()->setSkeletonName(skeletonName);
      
      if (skeletonName.empty() == false)
      {
         if (mEntity->hasSkeleton())
         {
            Ogre::AnimationStateSet *anims = mEntity->getAllAnimationStates();
            if (anims)
            {
               enableAnimations(true);
               disableAllAnims();
               
               Ogre::AnimationState *state;
               Ogre::AnimationStateIterator iter = anims->getAnimationStateIterator();
               state = iter.getNext();
               if (animation.empty() == false)
               {
                  try
                  {
                     state = mEntity->getAnimationState(animation);
                  } catch (Ogre::ItemIdentityException) {} // use first animation
               }
               const Ogre::String &name = state->getAnimationName();
               state->setEnabled(false);
               enableAnim(name, true, 1.f, true);
               setAnimSpeed(name, rate);

               success = true;
            }
         }
      } else
      {
         // Disable animation if no skeleton
         enableAnimations(false);
         disableAllAnims();
         success = true;
      }

      if (!success)
      {
         LLViewerObject *object = getViewerObject();
         LLOgreRenderer* renderer = LLOgreRenderer::getPointer();
         renderer->addOgreUpdate(object);
      }
   } else
   {
      success = true;
   }
   mRenderer->setSLContext();

   return success;
}

//! Create a manual object with certain naming scheme
Ogre::ManualObject* LLOgreObject::createManualObject()
{
	// Possible vertexbuffer etc. operations, so switch context
	// Note that for all sanity, one should already be in Ogre context before calling this,
	// because nested switches are OK
	mRenderer->setOgreContext();

	Ogre::ManualObject* manualObject = mSceneMgr->createManualObject(mSceneNode->getName() + "manual");

	// Return to SL context now
	mRenderer->setSLContext();

	return manualObject;
}

//! Convert a manual object to mesh
std::string LLOgreObject::convertToMesh(Ogre::ManualObject* object)
{
	// Possible vertexbuffer etc. operations, so switch context
	// Note that for all sanity, one should already be in Ogre context before calling this,
	// because nested switches are OK
	mRenderer->setOgreContext();

	Ogre::String meshName = mSceneNode->getName() + "manual";

	// Destroy previous mesh if exists with same name
	Ogre::MeshManager* meshMgr = Ogre::MeshManager::getSingletonPtr();		
	if (!meshMgr->getByName(meshName).isNull())
	{
		// If old mesh is still attached, remove
		if ((mEntity) && (mEntity->getMesh()->getName() == meshName))
		{
			removeMesh();
		}

		meshMgr->remove(meshName);
	}

	// Create new mesh
	object->convertToMesh(meshName);

	// Return to SL context now
	mRenderer->setSLContext();

	return std::string(meshName);
}
//! Set manual object to use
void LLOgreObject::setManualObject(Ogre::ManualObject* manualObject, const LLVector3& offset)
{
	// Same object?
	if (manualObject == mManualObject)
	{
		if (mOffsetNode)
		{
			setOffset(offset);
		}
		return;
	}

	// Remove old (if exists)
	removeManualObject();

	mManualObject = manualObject;

	// Attach to offset translation node if exists
	if (mOffsetNode)
	{
		setOffset(offset);
		mOffsetNode->attachObject(mManualObject);
	}
	else
		mSceneNode->attachObject(mManualObject);

    mManualObject->setVisible(mIsVisible);

    // In opensim mode, cap rendering distance to same as fog end
    if (!gInPureRex)
    {
        mManualObject->setRenderingDistance(mSceneMgr->getFogEnd());
    }
}

void LLOgreObject::setMaterialName(const std::string &name, U16 materialIndex)
{
   if (mEntity)
      mEntity->getSubEntity(materialIndex)->setMaterialName(name);

   if (mBillboardSet)
   {
      try
      {
         mBillboardSet->setMaterialName(name);
      } catch (Ogre::Exception e) { }
   }
}

U16 LLOgreObject::getNumMaterials()
{
   if (mEntity)
      return mEntity->getNumSubEntities();

   if (mBillboardSet)
      return 1;

   return 0;
}

const Ogre::AxisAlignedBox &LLOgreObject::getBoundingBox()
{
   if (mBillboardSet)
      return mDefaultAABBox;

   if (mEntity)
      return (mEntity->getBoundingBox());

   //if (mBillboardSet)
   //{
   //  // Ogre::Real min = mBillboardSet->getBillboard(0)->getOwnWidth() * 0.5f;
   //  // mDefaultAABBox.setMinimum(Ogre::Vector3(-min, -min, -min));
   //  // mDefaultAABBox.setMaximum(Ogre::Vector3( min,  min,  min));
   //  //// return (&mBillboardSet->getBoundingBox());
   //}

   return mDefaultAABBox;
}


//! Begin material update. Removes object from object group for this
void LLOgreObject::beginMaterialUpdate()
{
	if (!mMaterialUpdate)
	{
		mMaterialUpdate = true;

		LLObjectManager* objMgr = LLObjectManager::getInstance();
		if (objMgr && mGroupingEnabled) objMgr->objectRemoved(this);
	}
}

//! End material update. Readds object to object group
void LLOgreObject::endMaterialUpdate()
{
	if (mMaterialUpdate)
	{
		mMaterialUpdate = false;

		LLObjectManager* objMgr = LLObjectManager::getInstance();
		if (objMgr && mGroupingEnabled) objMgr->objectAdded(this);
	}
}

//! Enable/disable light. 
void LLOgreObject::setLight(bool enable)
{
	if (enable)
	{
		if (!mLight)
		{
			mLight = mSceneMgr->createLight(mSceneNode->getName() + "light");
			mLight->setType(Ogre::Light::LT_POINT);
			mSceneNode->attachObject(mLight);	
			// Actually make it a very small black light until proper parameters are set, no shadows
			setLightParameters(LLColor3(0,0,0), 0.1f, 1.0f, false);

            mLight->setUserAny(Ogre::Any(this));
		}
		mLight->setVisible(enable);
	}
	else
	{
		removeLight();
	}
}

void LLOgreObject::setBillboard(bool enable)
{
   mRenderer->setOgreContext();
   if (enable && !mBillboardSet)
   {
      // Remove old mesh, if one was specified
      removeMesh();

      Ogre::BillboardSet *billboardSet = mSceneMgr->createBillboardSet(mSceneNode->getName() + "billboard");
      billboardSet->setDefaultDimensions(1.0f, 1.0f);
      Ogre::Billboard *billboard = billboardSet->createBillboard(Ogre::Vector3::ZERO);
      billboard->setTexcoordRect(Ogre::FloatRect(0.f, 1.f, 1.f, 0.f));     
      mSceneNode->attachObject(billboardSet);
      mSceneNode->setScale(Ogre::Vector3::UNIT_SCALE);

      billboardSet->setMaterialName("NoTexture");
      mBillboardSet = billboardSet;
      mBillboardSet->setVisible(mIsVisible);
   }

   if (!enable && mBillboardSet)
   {
      removeBillboard();
   }
   mRenderer->setSLContext();
}

bool LLOgreObject::isBillboard()
{
   return (mBillboardSet != 0);
}

//! Update light parameters 
void LLOgreObject::setLightParameters(LLColor3 color, F32 radius, F32 falloff, bool castShadows)
{
	if (mLight)
	{
		if (radius < 0.001) radius = 0.001;

		// Attenuation calculation from pipeline.cpp
		F32 x = 3.f * (1.f + falloff);
		F32 linear = x / radius; // % of brightness at radius
		// Add a constant quad term for diminishing the light more beyond radius
		F32 quad = 0.3f / (radius * radius); 

		// Use the point where linear attenuation has reduced intensity to 1/20 as max range
		// (OpenGL has no absolute light range cap like Direct3D)
		F32 maxradius = radius;
		if (linear > 0.0)
		{
			maxradius = 20 / linear;
			if (maxradius < radius) maxradius = radius;
		}

		mLight->setDiffuseColour(LLOgreRenderer::toOgreColor(color));
		mLight->setAttenuation(maxradius, 0.0f, linear, quad);
	    mLight->setCastShadows(castShadows);
	}
}
		
//! Remove mesh (if any)
void LLOgreObject::removeMesh(bool removeManualMesh)
{
	if (mEntity)
	{
		// Possible vertexbuffer etc. removal, so switch context
		mRenderer->setOgreContext();

        // Disable animations to remove this object from animable objects list
        enableAnimations(false);

        LLObjectManager* objMgr = LLObjectManager::getInstance();
        if (objMgr && mGroupingEnabled) objMgr->objectRemoved(this);

        // A precausion. In any cause should not cause trouble
        //Rex::DynamicAnimationManager::getSingleton().removeEntity(mEntity); rex avatar stuff -Patrick Sapinski (Friday, January 08, 2010)

		// Detach from offsetnode if exists, else from the main scene node
		if (mOffsetNode)
			mOffsetNode->detachObject(mEntity);
		else
			mSceneNode->detachObject(mEntity);
		mSceneMgr->destroyEntity(mEntity);
		mEntity = 0;
		
		// If a converted mesh exists for this object, remove
		if (removeManualMesh)
		{
			Ogre::String meshName = mSceneNode->getName() + "manual";
			Ogre::MeshManager* meshMgr = Ogre::MeshManager::getSingletonPtr();		
			if (!meshMgr->getByName(meshName).isNull())
			{
				meshMgr->remove(meshName);
			}
		}

		// Return to SL context now
		mRenderer->setSLContext();
	}
	
	// Clear the active animation list (contains AnimationState pointers)
	mActiveAnims.clear();
}

//! Remove manual object (if any)
void LLOgreObject::removeManualObject()
{
	if (mManualObject)
	{
		// Possible vertexbuffer etc. removal, so switch context
		mRenderer->setOgreContext();

		// Detach from offsetnode if exists, else from the main scene node
		if (mOffsetNode)
			mOffsetNode->detachObject(mManualObject);
		else
			mSceneNode->detachObject(mManualObject);
		mSceneMgr->destroyManualObject(mManualObject);
		mManualObject = 0;

		// Return to SL context now
		mRenderer->setSLContext();
	}
}

//! Remove light (if any)
void LLOgreObject::removeLight()
{
	if (mLight)
	{
		mSceneNode->detachObject(mLight);
		mSceneMgr->destroyLight(mLight);
		mLight = 0;
	}
}

void LLOgreObject::removeBillboard()
{
   if (mBillboardSet)
   {
      mSceneNode->detachObject(mBillboardSet);
      mSceneMgr->destroyBillboardSet(mBillboardSet);
      mBillboardSet = 0;
   }
}

bool LLOgreObject::addParticleSystem(const std::string& name, bool flipYCoord)
{
   Ogre::ParticleSystem* system = mRenderer->createParticleSystem(name, flipYCoord);
   if (system)
   {
      mSceneNode->attachObject(system);
      mParticleSystems.push_back(system);

      return true;
   }
   return false;
}

void LLOgreObject::resetParticleSystems()
{
    if (mParticleSystems.size())
    {
        for (unsigned i = 0; i < mParticleSystems.size(); ++i)
        {
            Ogre::ParticleSystem* system = mParticleSystems[i];
            system->clear();
        }
    }
}

void LLOgreObject::removeParticleSystems()
{
    if (mParticleSystems.size())
    {
        mRenderer->setOgreContext();

        for (unsigned i = 0; i < mParticleSystems.size(); ++i)
        {
            Ogre::ParticleSystem* system = mParticleSystems[i];
            mSceneNode->detachObject(system);
            mSceneMgr->destroyParticleSystem(system);
        }
        mParticleSystems.clear();

        mRenderer->setSLContext();
    }
}

void LLOgreObject::setGlobalAnimationLods(const AnimationLodList &lods)
{
   smDefaultAnimationLods = lods;
}

void LLOgreObject::loadAnimations(const std::string &skeleton, const StringVector &animations)
{
   llinfos << "Merging animations from skeleton " << skeleton << llendl;
   if (!mEntity || mEntity->hasSkeleton() == false)
      return;
   
   Ogre::SkeletonInstance *destSkeleton = mEntity->getSkeleton();
   if (!destSkeleton)
      return;

   Ogre::SkeletonPtr source;
   source = Ogre::SkeletonManager::getSingleton().getByName(skeleton);
   if (source.isNull())
   {
      try
      {
         source = static_cast<Ogre::SkeletonPtr>
                                    (Ogre::SkeletonManager::getSingleton().load(skeleton,
                                    Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME));
      } catch (Ogre::Exception e)
      {
         llwarns << "Failed to load skeleton " + skeleton + " for animation merge" << llendl;
         return;
      }
   }
   if (source.isNull() == false)
   {
      Ogre::Skeleton::BoneHandleMap map;
      destSkeleton->_buildMapBoneByHandle(source.getPointer(), map);

      try
      {
         destSkeleton->_mergeSkeletonAnimations(source.getPointer(), map, animations);
      } catch(Ogre::ItemIdentityException)
      {
         // Need not handle
      }
      catch (Ogre::Exception)
      {
         llwarns << "Error merging animations from " << skeleton << ", likely incompatible bone structure" << llendl;
      }

      source->unload();
      destSkeleton->_refreshAnimationState(mEntity->getAllAnimationStates());
   } else
      llwarns << "Failed to load skeleton " + skeleton + " for animation merge" << llendl;
}

//! Get animation state of the entity. Return null if entity or animation state doesn't exist
Ogre::AnimationState* LLOgreObject::getAnimState(Ogre::Entity *entity, const std::string& animName)
{
	if (!entity) return 0;

   if (entity->hasSkeleton() == false) return 0;
   if (entity->getSkeleton()->hasAnimation(animName) == false) return 0; // To fight log spam regarding Ogre::ItemIdentityException

	try
	{
		return entity->getAnimationState(animName);
	} catch (Ogre::Exception e)
	{
		return 0;
	}
}

//! Enable an exclusive animation (fades out all other animations of same priority with fadeOut parameter)
bool LLOgreObject::enableExclusiveAnim(const std::string& animName, bool looped, F32 fadeIn, F32 fadeOutOthers, bool highPriority)
{
    // Disable all other active animations
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();
	while (i != mActiveAnims.end())
	{
        const std::string& otherAnimName = i->mAnimState->getAnimationName();
        if ((otherAnimName != animName) && (i->mHighPriority == highPriority))
        {
			i->mStatus = LLOgreObjectAnim::STATUS_FADEOUT;
			i->mFadePeriod = fadeOutOthers;
        }
        ++i;
    }

    // Then enable this
    return enableAnim(animName, looped, fadeIn, highPriority);
}

//! Enable an animation. Return false if the animation doesn't exist
/*! \todo add possibility to start from middle of animation
 */
bool LLOgreObject::enableAnim(const std::string& animName, bool looped, F32 fadeIn, bool highPriority)
{
	Ogre::AnimationState* animState = getAnimState(mEntity, animName);
	if (!animState) return false;

	animState->setLoop(looped);

	// See if we already have this animation
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();
	while (i != mActiveAnims.end())
	{
		if (i->mAnimState == animState)
		{
			i->mStatus = LLOgreObjectAnim::STATUS_FADEIN;
         i->mNumRepeats = (looped ? 0: 1);
			i->mFadePeriod = fadeIn;
            i->mHighPriority = highPriority;

			return true;
		}		
		++i;
	}

	// Start new animation from zero weight & speed factor 1, also reset time position
	animState->setTimePosition(0.0f);
	LLOgreObjectAnim newAnim;
	newAnim.mAnimState = animState;
	newAnim.mWeight = 0.0f;
	newAnim.mWeightFactor = 1.0f;
	newAnim.mSpeedFactor = 1.0f;
   newAnim.mNumRepeats = (looped ? 0: 1); // if looped, repeat 0 times (loop indefinetly) otherwise repeat one time.
	newAnim.mStatus = LLOgreObjectAnim::STATUS_FADEIN;
	newAnim.mFadePeriod = fadeIn;
    newAnim.mHighPriority = highPriority;
    newAnim.mAutoStop = false;

   // Add linked entities
   size_t n;
   for (n=0 ; n<mLinkedAnimatedEntities.size() ; ++n)
   {
      Ogre::AnimationState* animState = getAnimState(mLinkedAnimatedEntities[n], animName);
      if (animState)
      {
         animState->setLoop(looped);
         animState->setTimePosition(0.0f);
         newAnim.mLinkedAnimStates.push_back(animState);
      }
   }

	mActiveAnims.push_back(newAnim);

	return true;
}

//! Check whether non-looping animation has finished
/*! If looping, returns always false
 */
bool LLOgreObject::hasAnimationFinished(const std::string& animName)
{
	Ogre::AnimationState* animState = getAnimState(mEntity, animName);
	if (!animState) return false;

	// See if we find this animation in the list of active animations
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();
	while (i != mActiveAnims.end())
    {
		if (i->mAnimState == animState)
		{
            if ((!animState->getLoop()) && ((i->mSpeedFactor >= 0.f && animState->getTimePosition() >= animState->getLength()) ||
                    (i->mSpeedFactor < 0.f && animState->getTimePosition() <= 0.f)))
                return true;
            else
                return false;
        }
        ++i;
    }

    // If animation no longer in list of active animations, must have finished :)
    return true;
}

//! Check whether animation is active
bool LLOgreObject::isAnimationActive(const std::string& animName, bool checkFadeOut)
{
	Ogre::AnimationState* animState = getAnimState(mEntity, animName);
	if (!animState) return false;

	// See if we find this animation in the list of active animations
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();
	while (i != mActiveAnims.end())
    {
		if (i->mAnimState == animState)
		{
            if (checkFadeOut)
                return true;
            else 
            {
                if (i->mStatus != LLOgreObjectAnim::STATUS_FADEOUT)
                    return true;
                else
                    return false;
            }
        }
        ++i;
    }

    return false;
}



//! Set autostop on animation
bool LLOgreObject::setAnimAutoStop(const std::string& animName, bool enable)
{
	Ogre::AnimationState* animState = getAnimState(mEntity, animName);
	if (!animState) return false;

	// See if we find this animation in the list of active animations
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();
	while (i != mActiveAnims.end())
	{
		if (i->mAnimState == animState)
		{
			i->mAutoStop = enable;
			return true;
		}
		++i;
	}
	// Animation not active
	return false;
}

//! Set autostop on animation
bool LLOgreObject::setAnimNumLoops(const std::string& animName, unsigned int nRepeats)
{
	Ogre::AnimationState* animState = getAnimState(mEntity, animName);
	if (!animState) return false;

//   if (nRepeats != 1)
//      animState->setLoop(true);

	// See if we find this animation in the list of active animations
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();
	while (i != mActiveAnims.end())
	{
		if (i->mAnimState == animState)
		{
         i->mNumRepeats = nRepeats;
			return true;
		}
		++i;
	}
	// Animation not active
	return false;
}

//! Disable an animation. Return false if the animation doesn't exist or isn't active
bool LLOgreObject::disableAnim(const std::string& animName, F32 fadeOut)
{
	Ogre::AnimationState* animState = getAnimState(mEntity, animName);
	if (!animState) return false;

	// See if we find this animation in the list of active animations
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();
	while (i != mActiveAnims.end())
	{
		if (i->mAnimState == animState)
		{
			i->mStatus = LLOgreObjectAnim::STATUS_FADEOUT;
			i->mFadePeriod = fadeOut;
			return true;
		}
		++i;
	}
	// Animation not active
	return false;
}

//! Disable all animations. 
void LLOgreObject::disableAllAnims(F32 fadeOut)
{
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();
	while (i != mActiveAnims.end())
	{
		i->mStatus = LLOgreObjectAnim::STATUS_FADEOUT;
		i->mFadePeriod = fadeOut;	
		++i;
	}
}


void LLOgreObject::setAnimToEnd(const std::string& animName)
{
   Ogre::AnimationState* animState = getAnimState(mEntity, animName);
	if (animState)
   {
      setAnimTimePosition(animName, animState->getLength());
   }
}

//! Change speedfactor of an active animation. Return false if the animation doesn't exist or isn't active
bool LLOgreObject::setAnimSpeed(const std::string& animName, F32 speedFactor)
{
	Ogre::AnimationState* animState = getAnimState(mEntity, animName);
	if (!animState) return false;

	// See if we find this animation in the list of active animations
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();
	while (i != mActiveAnims.end())
	{
		if (i->mAnimState == animState)
		{
			i->mSpeedFactor = speedFactor;
			return true;
		}
		++i;
	}
	// Animation not active
	return false;
}

//! Change weight of an active animation (default 1.0). Return false if the animation doesn't exist or isn't active
bool LLOgreObject::setAnimWeight(const std::string& animName, F32 weight)
{
	Ogre::AnimationState* animState = getAnimState(mEntity, animName);
	if (!animState) return false;

	// See if we find this animation in the list of active animations
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();
	while (i != mActiveAnims.end())
	{
		if (i->mAnimState == animState)
		{
			i->mWeightFactor = weight;
			return true;
		}
		++i;
	}
	// Animation not active
	return false;
}

//! Change time position of an active animation. Return false if the animation doesn't exist or isn't active
bool LLOgreObject::setAnimTimePosition(const std::string& animName, F32 newPosition)
{
	Ogre::AnimationState* animState = getAnimState(mEntity, animName);
	if (!animState) return false;

	// See if we find this animation in the list of active animations
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();
	while (i != mActiveAnims.end())
	{
		if (i->mAnimState == animState)
		{
			animState->setTimePosition(newPosition);
			// Set also linked animation positions
			unsigned n;
	   		for (n=0 ; n<i->mLinkedAnimStates.size() ; ++n)
			{
				i->mLinkedAnimStates[n]->setTimePosition(newPosition);
			}			
			return true;
		}
		++i;
	}
	// Animation not active
	return false;
}




//! Update all running animations
bool LLOgreObject::_updateAnims(F32 timeStep)
{
   Ogre::Real distance = mSceneNode->getSquaredViewDepth(mRenderer->getCamera());

   S32 n;
   for (n=smDefaultAnimationLods.size()-1 ; n >= 0 ; --n)
   {
      if (distance > smDefaultAnimationLods[n].mDistanceSquared * mAnimationLodBias)
      {
         mAnimTimeElapsed += timeStep;
         if (mAnimTimeElapsed < smDefaultAnimationLods[n].updateTimeframe || smDefaultAnimationLods[n].updateTimeframe == 0)
            return false;

         timeStep = mAnimTimeElapsed;
         mAnimTimeElapsed = 0;
         break;
      }
   }

	// Loop through all animations & update them as necessary
	std::list<LLOgreObjectAnim>::iterator i = mActiveAnims.begin();	
	while (i != mActiveAnims.end())
	{
		switch(i->mStatus)
		{
		case LLOgreObjectAnim::STATUS_FADEIN:
			// If period is infinitely fast, skip to full weight & PLAY status
			if (i->mFadePeriod == 0.0f)
			{
				i->mWeight = 1.0f;
				i->mStatus = LLOgreObjectAnim::STATUS_PLAY;
			}
			else
			{
				i->mWeight += (1.0f / i->mFadePeriod) * timeStep;
				if (i->mWeight >= 1.0f)
				{
					i->mWeight = 1.0f;
					i->mStatus = LLOgreObjectAnim::STATUS_PLAY;
				}
			}
			break;

		case LLOgreObjectAnim::STATUS_PLAY:
            if (i->mAutoStop || i->mNumRepeats != 1)
            {
                if ((i->mSpeedFactor >= 0.f && i->mAnimState->getTimePosition() >= i->mAnimState->getLength()) ||
                    (i->mSpeedFactor < 0.f && i->mAnimState->getTimePosition() <= 0.f))
                {
                   if (i->mNumRepeats != 1)
                   {
                      if (i->mNumRepeats > 1)
                         i->mNumRepeats--;

                      Ogre::Real rewindPos = i->mSpeedFactor >= 0.f ? (i->mAnimState->getTimePosition() - i->mAnimState->getLength()) : i->mAnimState->getLength();
                      i->mAnimState->setTimePosition(rewindPos);
                   } else
        					 i->mStatus = LLOgreObjectAnim::STATUS_FADEOUT;
                }
            }
			break;	

		case LLOgreObjectAnim::STATUS_FADEOUT:
			// If period is infinitely fast, skip to disabled status immediately
			if (i->mFadePeriod == 0.0f)
			{
				i->mWeight = 0.0f;
				i->mStatus = LLOgreObjectAnim::STATUS_STOP;
			}
			else
			{
				i->mWeight -= (1.0f / i->mFadePeriod) * timeStep;
				if (i->mWeight <= 0.0f)
				{
					i->mWeight = 0.0f;
					i->mStatus = LLOgreObjectAnim::STATUS_STOP;
				}
			}
			break;
		}

		// Set weight & step the animation forward
		if (i->mStatus != LLOgreObjectAnim::STATUS_STOP)
		{
			i->mAnimState->setWeight((Ogre::Real)i->mWeight*i->mWeightFactor);
			i->mAnimState->addTime((Ogre::Real)(i->mSpeedFactor * timeStep));
			i->mAnimState->setEnabled(true);

            size_t n;
            for (n=0 ; n<i->mLinkedAnimStates.size() ; ++n)
            {
                i->mLinkedAnimStates[n]->copyStateFrom(*(i->mAnimState));
                // Apparently these two are needed even when copying the state
                i->mLinkedAnimStates[n]->setEnabled(true);
                i->mLinkedAnimStates[n]->addTime((Ogre::Real)(i->mSpeedFactor * timeStep));
            }
			
			++i;
		}
		else
		{
			// If stopped, disable & remove this animation from list
			i->mAnimState->setEnabled(false);
            size_t n;
            for (n=0 ; n<i->mLinkedAnimStates.size() ; ++n)
            {
                i->mLinkedAnimStates[n]->copyStateFrom(*(i->mAnimState));
                i->mLinkedAnimStates[n]->setEnabled(false);
            }
			i = mActiveAnims.erase(i);
		}
	}

    // High/low priority animation blending (only for skeletal entities)
	//! \todo blendmasks on linked animations?
	//! \todo unoptimal code, loops active animations a total of three times
    if (mEntity->hasSkeleton())
    {
        Ogre::SkeletonInstance* skel = mEntity->getSkeleton();
		if (mHighPriorityMask.size() != skel->getNumBones())
			mHighPriorityMask.resize(skel->getNumBones());
		if (mLowPriorityMask.size() != skel->getNumBones())
			mLowPriorityMask.resize(skel->getNumBones());

        for (unsigned j = 0; j < skel->getNumBones(); ++j)
        {
            mHighPriorityMask[j] = 1.0;
            mLowPriorityMask[j] = 1.0;
        }

		// Loop through all high priority animations & update the lowpriority-blendmask based on their active tracks
	    i = mActiveAnims.begin();	
	    while (i != mActiveAnims.end())
	    {
            // Create blend mask if animstate doesn't have it yet
            if (!i->mAnimState->hasBlendMask())
                i->mAnimState->createBlendMask(skel->getNumBones());

            if ((i->mHighPriority) && (i->mWeight > 0.0))
            {
				// High-priority animations get the full weight blend mask
                i->mAnimState->_setBlendMaskData(&mHighPriorityMask[0]);
                try
                {
                    Ogre::Animation* anim = skel->getAnimation(i->mAnimState->getAnimationName());
                    if (anim)
                    {
                        Ogre::Animation::NodeTrackIterator it = anim->getNodeTrackIterator();
                        while (it.hasMoreElements())
                        {
							Ogre::NodeAnimationTrack* track = it.getNext();
							unsigned id = track->getHandle();
							// For each active track, reduce corresponding bone in lowpriority-blendmask by this animation's weight
							if (id < mLowPriorityMask.size())
							{
								mLowPriorityMask[id] -= i->mWeight;
								if (mLowPriorityMask[id] < 0.0) mLowPriorityMask[id] = 0.0;
							}
                        }
                    }
                }
                catch (...) {}
            }

            ++i;
        }

		// Now set the calculated blendmask on low-priority animations
	    i = mActiveAnims.begin();	
	    while (i != mActiveAnims.end())
	    {
			if (!i->mHighPriority)
			{
                i->mAnimState->_setBlendMaskData(&mLowPriorityMask[0]);
			}

			++i;
		}
    }

    return true;
}

//! Enable animations for this object
void LLOgreObject::enableAnimations(bool enable)
{
   if (!mEntity && enable)
   {
      llwarns << "Trying to enable animations while no mesh set yet!" << llendl;
      return;
   }

   if (enable)
   {
      std::vector<LLOgreObject *>::iterator found = std::find(mAnimatedObjectList.begin(), mAnimatedObjectList.end(), this);
      if (found == mAnimatedObjectList.end())
      {
         // Object not yet in the list, add it
         mAnimatedObjectList.push_back(this);
      }

	  // Enable cumulative mode on skeletal animations
	  if (mEntity->hasSkeleton())
	  {
		  mEntity->getSkeleton()->setBlendMode(Ogre::ANIMBLEND_CUMULATIVE);
	  }

   } else
   {
      std::vector<LLOgreObject *>::iterator found = std::find(mAnimatedObjectList.begin(), mAnimatedObjectList.end(), this);
      if (found != mAnimatedObjectList.end())
      {
         // Object is in the list, remove it
         mAnimatedObjectList.erase(found);
      }
   }
}

//! Update all active animations for all animated ogre objects (call once per frame)
void LLOgreObject::updateAnimations(F32 timeStep)
{
   U32 n;
   for (n=0 ; n<mAnimatedObjectList.size() ; ++n)
   {
      mAnimatedObjectList[n]->_updateAnims(timeStep);
   }

   for (n=0 ; n<smAnimationStates.size() ; ++n)
   {
      smAnimationStates[n]->addTime(timeStep);
   }
}


//static
void LLOgreObject::addAnimationState(Ogre::AnimationState *state)
{
   AnimationStateVector::iterator iter = std::find(smAnimationStates.begin(), smAnimationStates.end(), state);
   if (iter == smAnimationStates.end())
      smAnimationStates.push_back(state);
}

//static
void LLOgreObject::removeAnimationState(Ogre::AnimationState *state)
{
   AnimationStateVector::iterator iter = std::find(smAnimationStates.begin(), smAnimationStates.end(), state);
   if (iter != smAnimationStates.end())
      smAnimationStates.erase(iter);
}
	
//! Set offset of attached object
void LLOgreObject::setOffset(const LLVector3& offset)
{
	if (mOffsetNode)
	{
        Ogre::Vector3 ogreOffset = LLOgreRenderer::toOgreVector(offset);
        if (mOffsetNode->getPosition().squaredDistance(ogreOffset) > 0.00001)
        {
            mOffsetNode->setPosition(ogreOffset);

            LLObjectManager* objMgr = LLObjectManager::getInstance();
            if (objMgr && mGroupingEnabled) objMgr->objectModified(this, LLObjectManager::MT_TRANSFORM);
        }
	}
}

//! Set offset of attached object
void LLOgreObject::setOffset(const Ogre::Vector3 ogreOffset)
{
	if (mOffsetNode)
	{
        if (mOffsetNode->getPosition().squaredDistance(ogreOffset) > 0.00001)
        {
            mOffsetNode->setPosition(ogreOffset);

            LLObjectManager* objMgr = LLObjectManager::getInstance();
            if (objMgr && mGroupingEnabled) objMgr->objectModified(this, LLObjectManager::MT_TRANSFORM);
        }
	}
}

const Ogre::Vector3 &LLOgreObject::getOffset() const
{
   return mOffsetNode ? mOffsetNode->getPosition() : Ogre::Vector3::ZERO;
}

//! Set rendering distance
void LLOgreObject::setRenderingDistance(F32 renderingDistance)
{
    // In opensim mode, there is preset rendering distance, otherwise, allow setting
    if (gInPureRex)
    {
        Ogre::MovableObject* mo = getMovableObject();
        if (mo) mo->setRenderingDistance(renderingDistance);
    
        if (mManualObject) mManualObject->setRenderingDistance(renderingDistance);
    
        for (unsigned i = 0; i < mParticleSystems.size(); ++i)
            mParticleSystems[i]->setRenderingDistance(renderingDistance);
    }
}

//! Set visibility
void LLOgreObject::setVisible(bool isVisible)
{
	if (isVisible != mIsVisible)
	{
		mIsVisible = isVisible;
		mSceneNode->setVisible(isVisible);

        LLObjectManager* objMgr = LLObjectManager::getInstance();
        if (objMgr && mGroupingEnabled) objMgr->objectModified(this, LLObjectManager::MT_VISIBILITY);

        // Keep particle system(s) visible at all times
        if (!isVisible)
        {
            for (unsigned i = 0; i < mParticleSystems.size(); ++i)
                mParticleSystems[i]->setVisible(true);
        }
    }
}

//! Get visibility
bool LLOgreObject::isVisible() const
{
    return mIsVisible;
}

//! Set position of scene node
void LLOgreObject::setPosition(const LLVector3& pos)
{
    Ogre::Vector3 ogrePos = LLOgreRenderer::toOgreVector(pos);
    if (mSceneNode->getPosition().squaredDistance(ogrePos) > 0.00001)
    {
        mSceneNode->setPosition(ogrePos);

        LLObjectManager* objMgr = LLObjectManager::getInstance();
        if (objMgr && mGroupingEnabled) objMgr->objectModified(this, LLObjectManager::MT_TRANSFORM);
    }
}

//! Translate position
void LLOgreObject::translate(const LLVector3& pos)
{
	mSceneNode->translate(LLOgreRenderer::toOgreVector(pos));

    LLObjectManager* objMgr = LLObjectManager::getInstance();
    if (objMgr && mGroupingEnabled) objMgr->objectModified(this, LLObjectManager::MT_TRANSFORM);
}

//! Set orientation of object
void LLOgreObject::setOrientation(const LLQuaternion& orientation)
{
    Ogre::Quaternion ogreOrient = LLOgreRenderer::toOgreQuaternion(orientation);	

    bool dirty = !(mSceneNode->getOrientation().equals(ogreOrient, Ogre::Degree(0.1)));

    if (dirty)
    {
		mSceneNode->setOrientation(ogreOrient);

		LLObjectManager* objMgr = LLObjectManager::getInstance();
        if (objMgr && mGroupingEnabled) objMgr->objectModified(this, LLObjectManager::MT_TRANSFORM);
    }
}

//! Set scale of object
void LLOgreObject::setScale(const LLVector3& scale)
{
   if (mBillboardSet)
   {
      if (scale != mBillboardScale)
      {
         mBillboardScale = scale;

         mBillboardSet->setDefaultDimensions(scale.mV[VX], scale.mV[VY]);
         int n;
         for (n=0 ; n<mBillboardSet->getNumBillboards() ; ++n)
         {
            mBillboardSet->getBillboard(n)->setDimensions(scale.mV[VX], scale.mV[VY]);
         }

         // We need to recalculate bounds as they apparently are not calculated automatically
         Ogre::Vector3 halfsize(scale.mV[VX] * 0.5f, scale.mV[VY] * 0.5f, scale.mV[VZ] * 0.5f);
         Ogre::AxisAlignedBox box(-halfsize, halfsize);
         mBillboardSet->setBounds(box, std::max(box.getHalfSize().x, box.getHalfSize().y));

      }
   } else
   {
	   // Note: toOgreVector reverses X & Z axes (for world coordinates), but for scale this is undesirable
       // Ogre::Vector3 ogreScale = LLOgreRenderer::toOgreVector(scale);
	   Ogre::Vector3 ogreScale(scale.mV[VX], scale.mV[VY], scale.mV[VZ]);

      // Scale may change very little, or very much at a time, do a simple compare for changing at all
      if (ogreScale != mSceneNode->getScale())
      {
		   mSceneNode->setScale(ogreScale);

		   LLObjectManager* objMgr = LLObjectManager::getInstance();
           if (objMgr && mGroupingEnabled) objMgr->objectModified(this, LLObjectManager::MT_SCALE);
	   }
   }
}

//! Disable/enable object grouping 
void LLOgreObject::setGroupingEnabled(bool enable)
{
    mGroupingEnabled = enable;
}

void LLOgreObject::linkAnimationStates(Ogre::Entity *entity)
{
   if (!mEntity) return;
   if (!entity) return;

   if (entity->getSkeleton() != mEntity->getSkeleton())
      mLinkedAnimatedEntities.push_back(entity);
}

void LLOgreObject::clearLinkedAnimationStates()
{
   mLinkedAnimatedEntities.clear();
}

void LLOgreObject::setViewerObject(LLViewerObject* vo)
{
    mViewerObject = vo;
}
