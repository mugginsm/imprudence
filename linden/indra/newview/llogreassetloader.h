/** 
 * @file llogreassetloader.h
 * @brief LLOgreAssetLoader class header file
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#ifndef LL_LLOGREASSETLOADER_H
#define LL_LLOGREASSETLOADER_H

#include "llstring.h"
#include "lluuid.h"
#include "llassettype.h"
#include "llviewerimage.h"
#include "lldispatcher.h"
#include <list>

// forward declarations
class LLViewerObject;
class LLVFS;
class RexOgreMaterial;
class RexParticleScript;
class RexOgreAssetNotifiable;

namespace Ogre
{
	class Mesh;
};

//!

//! Dispatcher for asset preload requests
class RexPreloadAssetsDispatchHandler : public LLDispatchHandler
{
public:
   //! Handle dispatch
	virtual bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& string);

   virtual std::string getKey() const { return key ; }

   static void assetCallback(LLVFS *vfs, const LLUUID &uuid, LLAssetType::EType type, void *user_data, S32 result_code, LLExtStat ext_status);

   const static std::string key;
};

//! A manager for coordinating Ogre-related asset (meshes, materials, particles etc.) transfers.
class LLOgreAssetLoader
{
public:
	//! Constructor
	LLOgreAssetLoader();
	
	//! Destructor
	~LLOgreAssetLoader();
	
    //! Checks if loaded particle scripts exist for a certain asset UUID
    bool areParticleScriptsLoaded(const LLUUID& particleScriptID);

    //! Returns true if loaded Ogre skeleton exists for the specified asset UUID
    bool isSkeletonLoaded(const LLUUID& skeletonID);

    //! Returns true if loaded material script exists for the specified asset UUID
    bool isMaterialLoaded(const LLUUID& materialID);

    //! Returns material by UUID, null if has not been loaded yet
    RexOgreMaterial* getMaterial(const LLUUID& materialID);

    //! Returns particle script asset by UUID, null if has not been loaded yet
    RexParticleScript* getParticleScript(const LLUUID& particleScriptID);

	//! Checks if a mesh with certain asset UUID is loaded
	bool isMeshLoaded(const LLUUID& meshID);

	//! Returns a mesh name constructed from asset UUID
	const std::string getMeshName(const LLUUID& meshID);

   //! Returns a skeleton name constructed from asset UUID
	std::string getSkeletonName(const LLUUID& skeletonID);

   //! Returns a material name constructed from asset UUID
	std::string getMaterialName(const LLUUID& materialID);

   //! returns flash animation name constructed from asset UUID
   std::string getFlashAnimationName(const LLUUID& animationID);

    //! Purge asset in case it has been reuploaded, so it can be reloaded into Ogre
    void purgeAsset(const LLUUID& assetID, LLAssetType::EType assetType);

	//! Queue an Ogre asset for loading, with optional notifiable object
    /*! chainNotifyAssetID can be used to copy notifiables of an another (previously requested) asset for the new asset
     */
    void loadAsset(const LLUUID& assetID, LLAssetType::EType assetType, RexOgreAssetNotifiable* notifiable, bool highPriority = true, const LLUUID& chainNotifyAssetID = LLUUID::null);

	//! Cancel all asset notifications for a certain notifiable (for the case a notifiable is destroyed first)
	void cancelNotify(RexOgreAssetNotifiable* notifiable);

	//! Implementation of the required TransferManager callback
	static void assetCallback(LLVFS *vfs, const LLUUID &uuid, LLAssetType::EType type, void *user_data, S32 result_code, LLExtStat ext_status);

	//! Flip Y texture coordinates of an Ogre mesh 
	void flipYTexCoords(Ogre::Mesh* mesh);

    //! Flip X texture coordinates of an Ogre mesh 
	void flipXTexCoords(Ogre::Mesh* mesh);

	//! Create Ogre mesh from a buffer. Returns true if successful
	bool createMesh(const LLUUID& assetID, U8* buffer, S32 size, BOOL TexFlipX, BOOL TexFlipY);

    //! Internal method to parse braces from an Ogre script. Returns true if line contained open/close brace
    static bool processBraces(const std::string& line, int& braceLevel);

    //! Class static initialization (registers genericmessage handler(s))
    static void initClass();

protected:
    //! Transfer original notifiable(s) of an asset to another asset (for assets that depend on other assets)
    void transferNotify(const LLUUID& oldAssetID, LLAssetType::EType newType, const LLUUID& newAssetID);

	//! Check if a transfer for certain asset UUID is already running
	bool isDuplicateTransfer(const LLUUID& assetID);
	
	//! Cancel notification of all objects tracking a certain asset UUID
	void cancelNotify(const LLUUID& assetID);

	//! Notify objects tracking this asset UUID of success
	void notifySuccess(const LLUUID& assetID);
    
    //! Create Ogre particle script from a buffer. Returns true if successful
    bool createParticleScript(const LLUUID& assetID, U8* buffer, S32 size);

    //! Create Ogre skeleton from a buffer. Returns true if successful
    bool createSkeleton(const LLUUID& assetID, U8* buffer, S32 size);

    //! Create Ogre material from a buffer. Returns true if successful
    bool createMaterial(const LLUUID& assetID, U8* buffer, S32 size);

	//! Structure for describing a queued transfer
	class LLOgreAssetQueueItem
	{
	public:
		//! Object to notify, NULL if none
		RexOgreAssetNotifiable* mNotifiable;
		//! Asset type
		LLAssetType::EType mAssetType;
		//! Asset UUID
		LLUUID mAssetID;
	};

	//! List for queued transfers
	std::list<LLOgreAssetQueueItem> mNotifyList;

    //! Map of materials created from scripts
    std::map<LLUUID, RexOgreMaterial*> mMaterials;

    //! Map of particle script assets
    std::map<LLUUID, RexParticleScript*> mParticleScripts;

    static RexPreloadAssetsDispatchHandler smRexPreloadAssetsHandler;
};

#endif // LL_LLOGREASSETLOADER_H
