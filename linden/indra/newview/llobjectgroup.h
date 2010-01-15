#ifndef LL_OBJECTGROUP_H
#define LL_OBJECTGROUP_H

#include "stdtypes.h"

#include <set>
#include <OGRE/OgreVector3.h>

class LLOgreObject;

namespace Ogre
{
    class SceneManager;
    class StaticGeometry;
}

/**
 *	Groups Ogre objects and combines their vertex buffers so they
 *  can be sent in one batch to the GPU
 */
class LLObjectGroup
{
public:
    typedef std::set<LLOgreObject*> ObjectList;

    LLObjectGroup(Ogre::SceneManager* sceneManager, const Ogre::Vector3& pos);
	~LLObjectGroup();

    void addObject(LLOgreObject* object);
    bool containsObject(LLOgreObject* object);
    void removeObject(LLOgreObject* object);

    /**
     *	Builds the combined object group from the LLOgreObject's that
     *  have been added to the group with addObject
     *  @param force True if the build process should be forced even if no
     *               changes have been detected. Object transformations aren't
     *               currently tracked, so a rebuild can be forced if an object
     *               is transformed.
     */
    void build(bool force = false);

    /**
     *	Rebuilds object groups, without the need to remove and add objects again
     *  @remark No objects will be added or removed from the group, only existing
     *          objects are updated. Call build() if you want to rebuild the whole
     *          group.
     */
    void quickRebuild();

    const Ogre::Vector3& getPosition() const;

    U32 getObjectCount() const;
    ObjectList& getObjects();

    void setCastShadows(bool castShadows);
    bool getCastShadows() const;

    bool isBuilt() const;

private:
    Ogre::StaticGeometry* mStaticGeometry;
    Ogre::SceneManager* mSceneMgr;

    ObjectList mObjects;

    bool mDirty;
    bool mCastShadows;
    bool mBuilt;

    Ogre::Vector3 mPosition;

    U32 mMinObjectCount;

};	//	class LLObjectGroup


#endif	//	LL_OBJECTGROUP_H
