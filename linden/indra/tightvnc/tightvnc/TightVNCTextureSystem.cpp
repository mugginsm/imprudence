#include "TightVNCTextureSystem.h"
#include "TightVNCPlugin.h"

#include <OGRE/OgreExternalTextureSourceManager.h>
#include <OGRE/OgreLogManager.h>
#include <OGRE/OgreMaterialManager.h>
#include <OGRE/OgrePass.h>
#include <OGRE/OgreRoot.h>
#include <OGRE/OgreTechnique.h>
#include <OGRE/OgreTextureManager.h>
#include <OGRE/OgreStringConverter.h>



TightVNCTextureSystem::TightVNCTextureSystem(TightVNCPlugin *plugin)
    : mPlugin(plugin)
{
    mDictionaryName = "vnc";
    mParamDictName = "vnc";
    Ogre::ExternalTextureSourceManager::getSingleton().setExternalTextureSource("vnc", this);
}

void TightVNCTextureSystem::connectionCreated(int id, const Ogre::String& textureName)
{
    IDMaterialMap::iterator i = mMaterials.find(id);
    if (i != mMaterials.end())
    {        
        MaterialList& materials = i->second;
        for (MaterialList::iterator j = materials.begin(); j != materials.end(); ++j)
        {
            Ogre::MaterialPtr matPtr = *j;
            if (!matPtr.isNull())
            {
                Ogre::LogManager::getSingleton().logMessage(
                    "TightVNCTextureSystem - Assigning VNC texture '" + textureName +
                    "' to material");

                // A texture has been created for the connection, assign it to the material

                Ogre::Technique* tech = matPtr->getTechnique(0);
                Ogre::Pass* pass = tech->getPass(0);
                pass->removeAllTextureUnitStates();
                Ogre::TextureUnitState* state = pass->createTextureUnitState();
                state->setTextureName(textureName);
            }
        }
    }
}

void TightVNCTextureSystem::connectionClosed(int id)
{
    // Remove pending materials for connection
    IDMaterialMap::iterator i = mMaterials.find(id);
    if (i != mMaterials.end())
        mMaterials.erase(id);
}

void TightVNCTextureSystem::clearConnections()
{
    mMaterials.clear();
}

bool TightVNCTextureSystem::initialise()
{
    addBaseParams();

    // Add commands to set VNC connection parameters
    Ogre::ParamDictionary* dict = getParamDictionary();
    dict->addParameter(Ogre::ParameterDef("address", "VNC server address (vnc://host:port)", Ogre::PT_STRING), &mCmdAddr);

    return true;
}

void TightVNCTextureSystem::shutDown()
{
    clearConnections();
    cleanupDictionary();
}

void TightVNCTextureSystem::setAddress(const Ogre::String& addr)
{
    Ogre::LogManager::getSingleton().logMessage("TightVNCTextureSystem::setAddress (" + addr + ")");
    mAddress = addr;
}

void TightVNCTextureSystem::createDefinedTexture(const Ogre::String& materialName, const Ogre::String& group)
{
    // XXX We should respect resource group name here, e.g. group should be passed to TightVNCUpdater somehow

    Ogre::LogManager::getSingleton().logMessage("TightVNCTextureSystem::createDefinedTexture (" + materialName + ")");

    // Get material by name
    // XXX Should we create a new material if an existing isn't found?
    Ogre::MaterialPtr material = (Ogre::MaterialPtr)Ogre::MaterialManager::getSingleton().getByName(materialName);
    if (material.isNull())
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL, "TightVNC material '" + materialName +
            "' not found!");
        return;
    }

    // Parse VNC material parameters
    Ogre::String addr = getParameter("address");
    addr = addr.substr(addr.find_first_of("://") + 3);
    if (addr.empty() || (addr.find(':') == std::string::npos))
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL,
            "TightVNC - Error: missing or invalid parameter 'address'");
        return;
    }
	Ogre::String host = addr.substr(0, addr.find_first_of(':'));
	Ogre::String port = addr.substr(addr.find_first_of(':') + 1);

    int portNum = Ogre::StringConverter::parseInt(port);

    // Lookup existing connection
    int id = mPlugin->lookupConnection(host, portNum);

    if (id == -1)
    {      
        // If no existing connection was found create a new one

        id = mPlugin->newConnection(host, portNum);
        if (id == -1)
        {
            Ogre::LogManager::getSingleton().logMessage(Ogre::LML_CRITICAL,
                "TightVNC - Error: Connection to '" + host + ":" + port + "' failed!");
            return;
        }
    }

    // Add material to pending materials list so that it's texture will be updated once
    // a VNC texture is available for the connection
    IDMaterialMap::iterator i = mMaterials.find(id);
    if (i == mMaterials.end())
    {
        MaterialList list;
        list.push_back(material);
        mMaterials.insert(std::make_pair(id, list));
    }
    else
    {
        i->second.push_back(material);
        Ogre::TexturePtr tex = mPlugin->getTextureForConnection(id);
        if (!tex.isNull())
        {
            Ogre::LogManager::getSingleton().logMessage(
                "TightVNCTextureSystem - Assigning VNC texture '" + tex->getName() +
                "' to material");

            Ogre::Technique* tech = material->getTechnique(0);
            Ogre::Pass* pass = tech->getPass(0);
            pass->removeAllTextureUnitStates();
            Ogre::TextureUnitState* state = pass->createTextureUnitState();
            state->setTextureName(tex->getName());
        }
    }
}

void TightVNCTextureSystem::destroyAdvancedTexture(const Ogre::String& material, const Ogre::String& group)
{
    Ogre::LogManager::getSingleton().logMessage("TightVNCTextureSystem::destroyAdvancedTexture (" + material + ")");

    for (IDMaterialMap::iterator i = mMaterials.begin(); i != mMaterials.end(); ++i)
    {
        MaterialList& materials = i->second;
        for (MaterialList::iterator j = materials.begin(); j != materials.end(); ++j)
        {
            Ogre::MaterialPtr matPtr = *j;
            if (!matPtr.isNull() && matPtr->getName() == material)
            {
                materials.erase(j);
                if (materials.empty())
                    mPlugin->closeConnection(i->first);
                return;
            }
        }
    }
}

bool TightVNCTextureSystem::requiresAuthorization() const
{
    return true;
}
