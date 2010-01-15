#include "llviewerprecompiledheaders.h"

#include "llobjectmanager.h"
#include "llobjectgroup.h"
#include "llogreobject.h"

#include <OGRE/OgreEntity.h>
#include <OGRE/OgreRoot.h>
#include <OGRE/OgreSceneNode.h>
#include <OGRE/OgreSubEntity.h>

LLObjectManager* LLObjectManager::mInstance = 0;

LLObjectManager* LLObjectManager::getInstance()
{
    return mInstance;
}

// ----------------------------------------------------------------------------

LLObjectManager::LLObjectManager(Ogre::SceneManager* sceneMgr)
    : mSceneMgr(sceneMgr)
    , mGroupDistance(10)
    , mRebuildTimer(0)
{
    mInstance = this;

    Ogre::Root::getSingleton().addFrameListener(this);
}

LLObjectManager::~LLObjectManager()
{
    for (GroupsByMaterial::iterator i = mGroups.begin(); i != mGroups.end(); ++i)
    {
        for (GroupList::iterator j = i->second.begin(); j != i->second.end(); ++j)
        {
            delete (*j);
            (*j) = 0;
        }
    }

    Ogre::Root::getSingleton().removeFrameListener(this);

    mInstance = 0;
}

// ----------------------------------------------------------------------------

void LLObjectManager::objectAdded(LLOgreObject* object)
{
    Ogre::Entity* entity = object->getEntity();
    if (!entity || entity->hasSkeleton())
        return;

    // If object invisible, don't add now
    if (!object->isVisible()) return;

    // Use the first material name found (this may result in multiple batches per entity!)
    std::string material = entity->getSubEntity(0)->getMaterialName();

    // If entity can't be grouped, keep track of it in case it'll be modified
    if (!canEntityBeGrouped(entity))
    {
        ObjectsByMaterial::iterator i = mUngroupedObjects.find(material);
        if (i == mUngroupedObjects.end())
        {
            ObjectList objects;
            objects.insert(object);
            mUngroupedObjects.insert(std::make_pair(material, objects));
        }
        else
        {
            i->second.insert(object);
        }

        return;
    }

    // Find closest group by material
    const Ogre::Vector3 pos = object->getSceneNode()->getPosition();
    LLObjectGroup* group = lookupNearestGroup(pos, material, entity->getCastShadows());

    if (group)
    {
        // If a group was found, add the object to the group
        //llinfos << "Adding object to existing group: " << object->getSceneNode()->getName() << llendl;
        group->addObject(object);
    }
    else
    {
        //llinfos << "Creating a new group for object " << object->getSceneNode()->getName() << llendl;

        // Create new group if no existing group was found
        group = new LLObjectGroup(mSceneMgr, pos);
        group->setCastShadows(entity->getCastShadows());

        GroupsByMaterial::iterator i = mGroups.find(material);
        if (i == mGroups.end())
        {
            GroupList groups;
            groups.push_back(group);
            mGroups.insert(std::make_pair(material, groups));
        }
        else
        {
            i->second.push_back(group);
        }

        group->addObject(object);
    }

    mDirtyGroups.insert(group);
}

void LLObjectManager::objectModified(LLOgreObject* object, EModifyType type)
{
    Ogre::Entity* entity = object->getEntity();
    if (!entity || entity->hasSkeleton())
        return;

    // Visibility changed
    if (type == MT_VISIBILITY)
    {
        if (object->isVisible())
        {
            //llinfos << "Showing object" << llendl;
            objectAdded(object);
            return;
        }
        else
        {
            //llinfos << "Hiding object" << llendl;
            objectRemoved(object);
            return;
        }
    }

    std::string material = entity->getSubEntity(0)->getMaterialName();

    // Find group containing the given object
    GroupsByMaterial::iterator i = mGroups.find(material);
    if (i != mGroups.end())
    {
        for (GroupList::iterator j = i->second.begin(); j != i->second.end(); ++j)
        {
            LLObjectGroup* group = *j;

            if (group->containsObject(object))
            {
                bool removed = false;
                if (!entity->isVisible() ||!group->isBuilt())
                {
                    group->removeObject(object);
                    group->build(true);
                    entity->setVisible(true);
                    removed = true;
                }

                // Check if object has gone too far, assign to a new group if it has
                if (group->getPosition().squaredDistance(object->getSceneNode()->getPosition()) >
                    (mGroupDistance * mGroupDistance))
                {
                    if (!removed)
                    {
                        group->removeObject(object);
                        if (group->getObjectCount() != 0)
                            mDirtyGroups.insert(group);
                    }
                    objectAdded(object);
                }
                else if (removed)
                {
                    if (canEntityBeGrouped(entity))
                        group->addObject(object);

                    if (group->getObjectCount() != 0)
                        mDirtyGroups.insert(group);
                }

                
                if (group->getObjectCount() == 0)
                {
                    GroupListUnique::iterator k = mDirtyGroups.find(group);
                    if (k != mDirtyGroups.end()) mDirtyGroups.erase(k);
                    delete group;
                  
                    // Object has been added and new group possibly created; j may be invalid; re-find the old group
                    GroupList::iterator l = std::find(i->second.begin(), i->second.end(), group);
                    if (l != i->second.end())
                      i->second.erase(l);
                }

                return;
            }
        }
    }
}

void LLObjectManager::objectRemoved(LLOgreObject* object)
{
    Ogre::Entity* entity = object->getEntity();
    if (!entity || entity->hasSkeleton())
        return;

    std::string material = entity->getSubEntity(0)->getMaterialName();

    GroupsByMaterial::iterator i = mGroups.find(material);
    if (i != mGroups.end())
    {
        for (GroupList::iterator j = i->second.begin(); j != i->second.end(); ++j)
        {
            LLObjectGroup* group = *j;
            if (group->containsObject(object))
            {
                group->removeObject(object);
                if (group->getObjectCount() == 0)
                {
                    GroupListUnique::iterator k = mDirtyGroups.find(group);
                    if (k != mDirtyGroups.end())
                        mDirtyGroups.erase(k);

                    delete group;
                    i->second.erase(j);
                }
                else
                {
                    // Rebuild group if not empty
                    mDirtyGroups.insert(group);
                }

                return;
            }
        }
    }

    ObjectsByMaterial::iterator j = mUngroupedObjects.find(material);
    if (j != mUngroupedObjects.end())
    {
        LLObjectGroup::ObjectList::iterator k = j->second.find(object);
        if (k != j->second.end()) j->second.erase(k);
    }
}

void LLObjectManager::materialModified(Ogre::Material* material)
{
    bool groupableMaterial = canMaterialBeGrouped(material);
    const std::string& name = material->getName();

	GroupsByMaterial::iterator i = mGroups.find(name);

    if (i != mGroups.end() && !groupableMaterial)
    {
		LLObjectGroup::ObjectList objects;
        // Material changed and can't be grouped anymore, destroy groups
        for (GroupList::iterator j = i->second.begin(); j != i->second.end(); ++j)
        {
            LLObjectGroup* group = *j;
            GroupListUnique::iterator k = mDirtyGroups.find(group);
            if (k != mDirtyGroups.end()) mDirtyGroups.erase(k);
            LLObjectGroup::ObjectList& groupObjects = group->getObjects();
            objects.insert(groupObjects.begin(), groupObjects.end());
            delete group;
		}
        mUngroupedObjects.insert(std::make_pair(name, objects));
        mGroups.erase(i);
    }
    else if (groupableMaterial)
    {
        // Material changed and can now be grouped, rebuild groups
        ObjectsByMaterial::iterator j = mUngroupedObjects.find(name);
        if (j != mUngroupedObjects.end())
        {
            for (LLObjectGroup::ObjectList::iterator k = j->second.begin(); k != j->second.end(); ++k)
            {
                // XXX Not the fastest way possible, refactor?
                objectAdded(*k);
            }
            mUngroupedObjects.erase(j);
        }
    }
}

// ----------------------------------------------------------------------------

LLObjectGroup* LLObjectManager::lookupNearestGroup(const Ogre::Vector3& pos, const std::string& material,
                                                   bool castShadows)
{
    //llinfos << "LLObjectManager::lookupNearestGroup (" << pos.x << "," << pos.y << "," << pos.z << ")" << llendl;
    // Find group lists by material name
    GroupsByMaterial::const_iterator i = mGroups.find(material);
    if (i == mGroups.end())
        return 0;

    // Find the group closest to pos, ignore if the closest group is further than mGroupDistance
    LLObjectGroup* group = 0;

    F32 groupDistance = mGroupDistance * mGroupDistance;
    for (GroupList::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
    {
        Ogre::Real distance = (*j)->getPosition().squaredDistance(pos);
        if ((distance < groupDistance) && ((*j)->getCastShadows() == castShadows))
        {
            group = *j;
            groupDistance = distance;
        }
    }

    return group;
}

bool LLObjectManager::canEntityBeGrouped(Ogre::Entity* entity) const
{
    if (entity->hasSkeleton())
        return false;

    unsigned int subEntityCount = entity->getNumSubEntities();
    for (unsigned int i = 0; i<subEntityCount; ++i)
    {
        Ogre::SubEntity* subEntity = entity->getSubEntity(i);
        Ogre::Material* material = subEntity->getMaterial().get();
        if (!canMaterialBeGrouped(material))
            return false;
    }

    return true;
}

bool LLObjectManager::canMaterialBeGrouped(Ogre::Material* material) const
{
    Ogre::Material::TechniqueIterator t = material->getTechniqueIterator();
    while (t.hasMoreElements())
    {
        Ogre::Technique::PassIterator p = t.getNext()->getPassIterator();
        while (p.hasMoreElements())
        {
            Ogre::Pass* pass = p.getNext();
            if (!pass->getDepthWriteEnabled())
            {
                return false;
            }
        }
    }

    return true;
}

// ----------------------------------------------------------------------------

bool LLObjectManager::frameStarted(const Ogre::FrameEvent& e)
{
    if (mRebuildTimer > 0)
    {
        mRebuildTimer -= e.timeSinceLastFrame;
    }
    else if (!mDirtyGroups.empty())
    {
        mRebuildTimer = 2.0;

        for (GroupListUnique::const_iterator i = mDirtyGroups.begin(); i != mDirtyGroups.end(); ++i)
        {
            LLObjectGroup* group = *i;
            llinfos << "Compiling group (" << group->getObjectCount() << " objects)" << llendl;
            group->build(true);
        }
        mDirtyGroups.clear();
    }

    return true;
}

bool LLObjectManager::frameEnded(const Ogre::FrameEvent& e)
{
    return true;
}
