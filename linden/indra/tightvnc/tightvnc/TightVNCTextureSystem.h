#ifndef _TIGHTVNCTEXTURESYSTEM_H_
#define _TIGHTVNCTEXTURESYSTEM_H_

#include "llexternaltexturesource.h"

class TightVNCPlugin;

/**
 *  Provides an external VNC texture source for Ogre materials.
 *  Example usage of VNC textures from materials:<br>
 *  \code
    texture_unit
    {
        texture_source vnc
        {
            address vnc://192.168.0.1:5900
        }
    }
 *  \endcode
 */
class TightVNCTextureSystem : public LLExternalTextureSource
{
public:
    /**
     *  @param plugin The TightVNC plugin that owns the texture system
     */
    TightVNCTextureSystem(TightVNCPlugin* plugin);

    /**
     *	@param id Connection id
     *  @param textureName VNC texture name
     */
    void connectionCreated(int id, const Ogre::String& textureName);

    /**
     *  Reports that a VNC connection has been closed (or canceled)
     *  @param id Connection id
     */
    void connectionClosed(int id);

    void clearConnections();

    // Get/set wrappers for VNC textures source commands.
    // Can be used directly before creating a VNC texture if the material
    // doesn't have required VNC parameters set
    void setAddress(const Ogre::String& addr);
    const Ogre::String& getAddress() const { return mAddress; }

protected:
    bool initialise();
    void shutDown();

    void createDefinedTexture(const Ogre::String& material, const Ogre::String& group);
    void destroyAdvancedTexture(const Ogre::String& material, const Ogre::String& group);

    bool requiresAuthorization() const;

private:
    // ------------------------------------------------------------------------
    // Material commands to set VNC connection parameters

    struct _OgrePrivate CmdAddress : public Ogre::ParamCommand
    {
        Ogre::String doGet(const void* target) const
        {
            return ((TightVNCTextureSystem*)target)->getAddress();
        }

        void doSet(void* target, const Ogre::String& val)
        {
            ((TightVNCTextureSystem*)target)->setAddress(val);
        }
    };

    // ------------------------------------------------------------------------

    TightVNCPlugin* mPlugin;

    CmdAddress mCmdAddr;
    Ogre::String mAddress;

    typedef std::vector<Ogre::MaterialPtr> MaterialList;
    typedef std::map<int, MaterialList> IDMaterialMap;
    IDMaterialMap mMaterials;

};  //  class TightVNCTextureSystem

#endif  //  _TIGHTVNCTEXTURESYSTEM_H_
