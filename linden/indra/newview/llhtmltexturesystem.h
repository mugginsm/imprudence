#ifndef LLHTMLTEXTURESYSTEM_H
#define LLHTMLTEXTURESYSTEM_H

#include "llexternaltexturesource.h"

class LLHtmlTextureSystem : public LLExternalTextureSource
{
public:
	LLHtmlTextureSystem();
	~LLHtmlTextureSystem();

protected:
    bool initialise();
    void shutDown();

    void createDefinedTexture(const Ogre::String& material, const Ogre::String& group);
    void destroyAdvancedTexture(const Ogre::String& material, const Ogre::String& group);

    bool requiresAuthorization() const;

    void setAddress(const Ogre::String& addr);
    const Ogre::String& getAddress() const;
    void setRefreshRate(const Ogre::String& rate);
    const Ogre::String& getRefreshRate() const;

private:
    // ------------------------------------------------------------------------
    // Material commands to set connection parameters

    struct _OgrePrivate CmdAddress : public Ogre::ParamCommand
    {
        Ogre::String doGet(const void* target) const
        {
            return ((LLHtmlTextureSystem*)target)->getAddress();
        }

        void doSet(void* target, const Ogre::String& val)
        {
            ((LLHtmlTextureSystem*)target)->setAddress(val);
        }
    };

    struct _OgrePrivate CmdRefreshRate : public Ogre::ParamCommand
    {
        Ogre::String doGet(const void* target) const
        {
            return ((LLHtmlTextureSystem*)target)->getRefreshRate();
        }

        void doSet(void* target, const Ogre::String& val)
        {
            ((LLHtmlTextureSystem*)target)->setRefreshRate(val);
        }
    };

    // ------------------------------------------------------------------------

    CmdAddress mCmdAddr;
    Ogre::String mAddress;
    CmdRefreshRate mCmdRefreshRate;
    Ogre::String mRefreshRate;

};	//	class LLHtmlTextureSystem

#endif	//	LLHTMLTEXTURESYSTEM_H
