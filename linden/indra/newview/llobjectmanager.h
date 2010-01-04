#ifndef LL_OBJECTMANAGER_H
#define LL_OBJECTMANAGER_H

#include "stdtypes.h"

#include <OGRE/OgreFrameListener.h>

#include <vector>
#include <map>

class LLObjectGroup;
class LLOgreObject;

namespace Ogre
{
    class Entity;
    class Material;
    class SceneManager;
    class Vector3;
}

class LLObjectManager : public Ogre::FrameListener
{
public:
    enum EModifyType
    {
        MT_TRANSFORM,
        MT_SCALE,
        MT_VISIBILITY,
        MT_UNKNOWN
    };

public:
    static LLObjectManager* getInstance();

    LLObjectManager(Ogre::SceneManager* sceneMgr);
	~LLObjectManager();

    void objectAdded(LLOgreObject* object);
    void objectModified(LLOgreObject* object, EModifyType type = MT_UNKNOWN);
    void objectRemoved(LLOgreObject* object);

    void materialModified(Ogre::Material* material);

protected:
    bool frameStarted(const Ogre::FrameEvent& e);
    bool frameEnded(const Ogre::FrameEvent& e);

private:
    LLObjectGroup* lookupNearestGroup(const Ogre::Vector3& pos, const std::string& material, bool castShadows);
    bool canEntityBeGrouped(Ogre::Entity* entity) const;
    bool canMaterialBeGrouped(Ogre::Material* material) const;

    Ogre::SceneManager* mSceneMgr;

    typedef std::vector<LLObjectGroup*> GroupList;
    typedef std::map<std::string, GroupList> GroupsByMaterial;
    GroupsByMaterial mGroups;
    
    typedef std::set<LLOgreObject*> ObjectList;
    typedef std::map<std::string, ObjectList> ObjectsByMaterial;
    ObjectsByMaterial mUngroupedObjects;

    typedef std::set<LLObjectGroup*> GroupListUnique;
    GroupListUnique mDirtyGroups;

    F32 mRebuildTimer;

    F32 mGroupDistance;

    static LLObjectManager* mInstance;

};	//	class LLObjectManager

#endif	//	LL_OBJECTMANAGER_H
