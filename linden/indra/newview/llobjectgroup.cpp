#include "llviewerprecompiledheaders.h"

#include "llobjectgroup.h"
#include "llogreobject.h"

#include <OGRE/OgreEntity.h>
#include <OGRE/OgreSceneManager.h>
#include <OGRE/OgreStaticGeometry.h>


LLObjectGroup::LLObjectGroup(Ogre::SceneManager* sceneManager, const Ogre::Vector3& pos)
    : mSceneMgr(sceneManager)
    , mDirty(false)
    , mCastShadows(false)
    , mBuilt(false)
    , mPosition(pos)
    , mMinObjectCount(2)
{
    assert(mSceneMgr != 0);

    LLUUID id;
    id.generate();
    mStaticGeometry = mSceneMgr->createStaticGeometry(id.asString());
}

LLObjectGroup::~LLObjectGroup()
{
    if (mStaticGeometry && mStaticGeometry->isVisible())
    {
        for (ObjectList::const_iterator i = mObjects.begin(); i != mObjects.end(); ++i)
        {
            LLOgreObject* object = *i;
            object->getEntity()->setVisible(object->isVisible());
        }

        mSceneMgr->destroyStaticGeometry(mStaticGeometry);
        mStaticGeometry = 0;
    }

    mSceneMgr = 0;
}

// ----------------------------------------------------------------------------

void LLObjectGroup::addObject(LLOgreObject* object)
{
    mObjects.insert(object);
    mDirty = true;
}

bool LLObjectGroup::containsObject(LLOgreObject* object)
{
    ObjectList::iterator i = mObjects.find(object);
    return (i != mObjects.end());
}

void LLObjectGroup::removeObject(LLOgreObject* object)
{
    ObjectList::iterator i = mObjects.find(object);
    if (i != mObjects.end())
    {
        // Make object visible again

        if (object->getEntity())
            object->getEntity()->setVisible(object->isVisible());

        // Remove object and mark list dirty

        mObjects.erase(i);
        mDirty = true;
    }
}

void LLObjectGroup::build(bool force)
{
    // If there have been no changes and the build isn't forced, don't rebuild geometry
    if (!force && !mDirty)
        return;

    mDirty = false;

    // Destroy old static geometry

    if (mStaticGeometry->isVisible())
    {
        if (getObjectCount() < mMinObjectCount)
        {
            for (ObjectList::const_iterator i = mObjects.begin(); i != mObjects.end(); ++i)
            {
                LLOgreObject* object = *i;
                object->getEntity()->setVisible(object->isVisible());
            }
        }
        mStaticGeometry->setVisible(false);
        mStaticGeometry->reset();
        mBuilt = false;
    }

    if (getObjectCount() < mMinObjectCount)
        return;

    // Rebuild static geometry

    for (ObjectList::const_iterator i = mObjects.begin(); i != mObjects.end(); ++i)
    {
        LLOgreObject* object = *i;
        if (object->getEntity())
        {
            Ogre::SceneNode* node = object->getSceneNode();
            mStaticGeometry->addEntity((*i)->getEntity(), node->getWorldPosition(), node->getWorldOrientation(),
                                       node->getScale());
            object->getEntity()->setVisible(false);
        }
    }

    mStaticGeometry->build();
    mStaticGeometry->setVisible(true);
    mStaticGeometry->setCastShadows(mCastShadows);
    mBuilt = true;
}

void LLObjectGroup::quickRebuild()
{
    mStaticGeometry->destroy();
    mStaticGeometry->build();
    mStaticGeometry->setVisible(true);
    mStaticGeometry->setCastShadows(mCastShadows);
}

// ----------------------------------------------------------------------------

const Ogre::Vector3& LLObjectGroup::getPosition() const
{
    return mPosition;
}

U32 LLObjectGroup::getObjectCount() const
{
    return (U32)mObjects.size();
}

LLObjectGroup::ObjectList& LLObjectGroup::getObjects()
{
    return mObjects;
}

void LLObjectGroup::setCastShadows(bool castShadows)
{
    mCastShadows = castShadows;
}

bool LLObjectGroup::getCastShadows() const
{
    return mCastShadows;
}

bool LLObjectGroup::isBuilt() const
{
    return mBuilt;
}
