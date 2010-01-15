/** 
 * @file llogreassetloader.h
 * @brief LLOgreAssetLoader class implementation
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#include "llviewerprecompiledheaders.h"
#include "llviewerobject.h"
#include "llviewerimagelist.h"
#include "llogre.h"
#include "llogreassetloader.h"
#include "rexogrelegacymaterial.h"
#include "rexogrematerial.h"
#include "rexogreassetnotifiable.h"
#include "rexparticlescript.h"
#include "rexflashanimationmanager.h"
#include "OGRE/Ogre.h"
#include "OGRE/OgreMeshSerializer.h"

extern LLDispatcher gGenericDispatcher;

const std::string RexPreloadAssetsDispatchHandler::key = "RexPreloadAssets";

RexPreloadAssetsDispatchHandler LLOgreAssetLoader::smRexPreloadAssetsHandler;

bool RexPreloadAssetsDispatchHandler::operator ()(const LLDispatcher *dispatcher, const std::string &key, const LLUUID &invoice, const LLDispatchHandler::sparam_t &string)
{
    for (unsigned i = 0; i < string.size(); ++i)
    {
        // Strings should consist of assettype, space & UUID
        std::vector<Ogre::String> lineVec = Ogre::StringUtil::split(string[i], "\t ");
        if (lineVec.size() >= 2)
        {
            LLAssetType::EType assetType;
            assetType = (LLAssetType::EType)Ogre::StringConverter::parseInt(lineVec[0]);
            LLUUID assetID(lineVec[1]);
            if (assetID != LLUUID::null)
            {
                // Check if it's a texture or other asset
                if (assetType == LLAssetType::AT_TEXTURE)
                {
                    // This should trigger image loading, however boost may not be sufficient, because it will fade without anyone actually using the texture
                    LLViewerImage* image = gImageList.getImage(assetID);
                    if (image)
                        image->addTextureStats(1024.0 * 1024.0); // great boost
                }
                else
                {
                    gAssetStorage->getAssetData(assetID, assetType, assetCallback, 0, true);
                }
            }
        }
        else
        {
            llwarns << "Badly formatted RexPreloadAssets line: " << string[i] << llendl;
            return false;
        }
    }

    return true;
}

void RexPreloadAssetsDispatchHandler::assetCallback(LLVFS *vfs, const LLUUID &uuid, LLAssetType::EType type, void *user_data, S32 result_code, LLExtStat ext_status)
{
    // We may eventually do something here, for now do nothing
}

//! Static init, register genericmessage handler(s)
void LLOgreAssetLoader::initClass()
{
    gGenericDispatcher.addHandler(smRexPreloadAssetsHandler.getKey(), &smRexPreloadAssetsHandler);
}

//! Constructor
LLOgreAssetLoader::LLOgreAssetLoader()
{
}

//! Destructor
LLOgreAssetLoader::~LLOgreAssetLoader()
{
    // Destroy materials loaded from scripts
    std::map<LLUUID, RexOgreMaterial* >::iterator i = mMaterials.begin();
    while (i != mMaterials.end())
    {
        if (i->second)
        {
            delete i->second;
            i->second = 0;
        }
        ++i;
    }
    mMaterials.clear();

    // Destroy particle scripts
    std::map<LLUUID, RexParticleScript* >::iterator j = mParticleScripts.begin();
    while (j != mParticleScripts.end())
    {
        if (j->second)
        {
            delete j->second;
            j->second = 0;
        }
        ++j;
    }
    mParticleScripts.clear();
}

//! Checks if loaded particle scripts exist for a certain asset UUID
bool LLOgreAssetLoader::areParticleScriptsLoaded(const LLUUID& particleScriptID)
{
    RexParticleScript* script = getParticleScript(particleScriptID);
    if (!script)
        return false;
    return script->getNumParticleSystems();
}

//! Checks if a skeleton with certain asset UUID is loaded
bool LLOgreAssetLoader::isSkeletonLoaded(const LLUUID& skeletonID)
{
   return Ogre::SkeletonManager::getSingleton().getByName(skeletonID.asString()).isNull() ? false : true;
}

////! Checks if a flash animation with certain asset UUID is loaded
//bool LLOgreAssetLoader::isFlashAnimationLoaded(const LLUUID& animationID)
//{
//   return Ogre::SkeletonManager::getSingleton().getByName(skeletonID.asString()).isNull() ? false : true;
//}

//! Returns particlescript with certain asset UUID, or null if does not yet exist
RexParticleScript* LLOgreAssetLoader::getParticleScript(const LLUUID& particleScriptID)
{
    std::map<LLUUID, RexParticleScript* >::iterator i  = mParticleScripts.find(particleScriptID);
    if (i != mParticleScripts.end())
        return i->second;
    else return 0;
}

//! Returns material with certain asset UUID, or null if does not yet exist
RexOgreMaterial* LLOgreAssetLoader::getMaterial(const LLUUID& materialID)
{
    std::map<LLUUID, RexOgreMaterial* >::iterator i  = mMaterials.find(materialID);
    if (i != mMaterials.end())
        return i->second;
    else return 0;
}

//! Checks if a material with certain asset UUID is loaded
bool LLOgreAssetLoader::isMaterialLoaded(const LLUUID& materialID)
{
    RexOgreMaterial* mat = getMaterial(materialID);
    if (!mat) return false;
    return mat->isLoaded();
}

//! Checks if a mesh with certain asset UUID is loaded
bool LLOgreAssetLoader::isMeshLoaded(const LLUUID& meshID)
{
	return Ogre::MeshManager::getSingleton().getByName(meshID.asString()).isNull() ? false : true;
}

//! Returns a mesh name constructed from asset UUID
const std::string LLOgreAssetLoader::getMeshName(const LLUUID& meshID)
{
	return meshID.asString();
}

//! Returns a skeleton name constructed from asset UUID
std::string LLOgreAssetLoader::getSkeletonName(const LLUUID& skeletonID)
{
	return skeletonID.asString();
}

//! Returns a material name constructed from asset UUID
std::string LLOgreAssetLoader::getMaterialName(const LLUUID& materialID)
{
	return materialID.asString();
}

std::string LLOgreAssetLoader::getFlashAnimationName(const LLUUID& animationID)
{
	return animationID.asString();
}

//! Queue an Ogre asset for loading, with optional notifiable object
void LLOgreAssetLoader::loadAsset(const LLUUID& assetID, LLAssetType::EType assetType, RexOgreAssetNotifiable* notifiable, bool highPriority, const LLUUID& chainNotifyAssetID)
{	
	bool dup = isDuplicateTransfer(assetID);

	LLOgreAssetQueueItem item;
	item.mAssetID = assetID;
	item.mAssetType = assetType;
	// Can be null, that just means we track the transfer internally but never notify anyone
	item.mNotifiable = notifiable;
	mNotifyList.push_back(item);

    // Chain notifiables of an old request to this, if necessary
    if (chainNotifyAssetID != LLUUID::null)
        transferNotify(chainNotifyAssetID, assetType, assetID);

	// Checking for duplicate isn't strictly necessary, because transfermanager handles it too
	if (!dup)
	{
		llinfos << "Requesting asset " << assetID << llendl;
        // Note: asset callback may happen immediately, if data exists in cache
		gAssetStorage->getAssetData(assetID, assetType, assetCallback, (void *)this, highPriority);
	}
}

//! Implementation of the required TransferManager callback
void LLOgreAssetLoader::assetCallback(LLVFS *vfs, const LLUUID &uuid, LLAssetType::EType type, void *user_data, S32 result_code, LLExtStat ext_status)
{
	LLOgreAssetLoader* assetLoader = (LLOgreAssetLoader*)user_data;
	bool success = false;

	if (result_code)
	{
		llinfos << "Error in asset transfer: " << LLAssetStorage::getErrorString( result_code ) << " (" << result_code << ")" << llendl;
	}
	else
	{
		LLVFile file(vfs, uuid, type, LLVFile::READ);
		S32 size = file.getSize();

		U8* buffer = new U8[size];
		file.read((U8*)buffer, size);	/*Flawfinder: ignore*/

		if (type == LLAssetType::AT_MESH)
		{
			success = assetLoader->createMesh(uuid, buffer, size, false, true);
		}
        if (type == LLAssetType::AT_PARTICLE_SCRIPT)
        {
            success = assetLoader->createParticleScript(uuid, buffer, size);
        }
        if (type == LLAssetType::AT_SKELETON)
        {
            success = assetLoader->createSkeleton(uuid, buffer, size);
        }
        if (type == LLAssetType::AT_MATERIAL)
        {
            success = assetLoader->createMaterial(uuid, buffer, size);
        }
        if (type == LLAssetType::AT_FLASH_ANIMATION)
        {
           success = LLOgreRenderer::getPointer()->getFlashAnimationManager()->saveToTemp(uuid, buffer, size);
        }
		delete []buffer;
	}

	// Notify on success
	if (success)
	{
		assetLoader->notifySuccess(uuid);
	}
	else
	{
		assetLoader->cancelNotify(uuid); // Transfer failed, client(s) must retry on their own
	}
}

//! Check if a transfer for certain asset UUID is already running
bool LLOgreAssetLoader::isDuplicateTransfer(const LLUUID& assetID)
{
	std::list<LLOgreAssetQueueItem>::iterator i = mNotifyList.begin();
	while (i != mNotifyList.end()) 
	{
		if ((*i).mAssetID == assetID) return true;
		
		++i;
	}	
	return false;
}

//! Cancel all asset notifications for a certain notifiable (for the case a notifiable is destroyed first)
void LLOgreAssetLoader::cancelNotify(RexOgreAssetNotifiable* notifiable)
{
	std::list<LLOgreAssetQueueItem>::iterator i = mNotifyList.begin();
	while (i != mNotifyList.end())
	{
		if ((*i).mNotifiable == notifiable)
		{
			i = mNotifyList.erase(i);
		}
		else ++i;
	}
}

//! Cancel notification of all objects tracking a certain asset UUID
void LLOgreAssetLoader::cancelNotify(const LLUUID& assetID)
{
	std::list<LLOgreAssetQueueItem>::iterator i = mNotifyList.begin();
	while (i != mNotifyList.end())
	{
		if ((*i).mAssetID == assetID)
		{
			i = mNotifyList.erase(i);
		}
		else ++i;
	}
}

//! Transfer original notifiable of an asset to another asset (for assets that depend on other assets)
void LLOgreAssetLoader::transferNotify(const LLUUID& oldAssetID, LLAssetType::EType newType, const LLUUID& newAssetID)
{
    if ((newAssetID == LLUUID::null) || (newAssetID == oldAssetID))
        return;

    std::list<LLOgreAssetQueueItem>::iterator i = mNotifyList.begin();
    while (i != mNotifyList.end())
    {
        if (((*i).mAssetID == oldAssetID) && ((*i).mNotifiable))
        {
            LLOgreAssetQueueItem item;
            item.mAssetID = newAssetID;
            item.mAssetType = newType;
            item.mNotifiable = (*i).mNotifiable;
            mNotifyList.push_back(item);
        }
        ++i;
    }
}

//! Notify objects tracking this asset UUID of success
void LLOgreAssetLoader::notifySuccess(const LLUUID& assetID)
{
	std::list<LLOgreAssetQueueItem>::iterator i = mNotifyList.begin();
	while (i != mNotifyList.end())
	{
		if ((*i).mAssetID == assetID)
		{
			if ((*i).mNotifiable)
			{
				(*i).mNotifiable->onOgreAssetLoaded((*i).mAssetType, (*i).mAssetID);
			}
			i = mNotifyList.erase(i);
		}
		else ++i;
	}
}

// #ifndef LL_RELEASE
static const char *SemanticToString(Ogre::VertexElementSemantic s)
{
	const char * const data[] = 
	{
		"",
		"Position",
		"BlendWeights",
		"BlendIndices",
		"Normal",
		"Diffuse",
		"Specular",
		"TexCoord",
		"Binormal",
		"Tangent"
	};

	return data[s];
}
// #endif

// #ifndef LL_RELEASE
static const char *TypeToString(Ogre::VertexElementType t)
{
	const char *const data[] =
	{
		"Float1",
		"Float2",
		"Float3",
		"Float4",
		"Color",
		"Short1",
		"Short2",
		"Short3",
		"Short4",
		"UByte4",
		"ARGB",
		"ABGR"
	};

	return data[t];
}
// #endif

// #ifndef LL_RELEASE
static void DumpOgreMeshVertexFormat(Ogre::Mesh &mesh)
{
	llinfos << "Mesh " << mesh.getName().c_str() << " has " << mesh.getNumSubMeshes() << " submeshes, " << mesh.getNumAnimations() << " animations, " << mesh.getNumLodLevels() << " lod levels. Pose count: " << mesh.getPoseCount() << " Size: " << mesh.getSize() << llendl;
	Ogre::Mesh::SubMeshIterator iter = mesh.getSubMeshIterator();
	int smIndex = 0;
	while(iter.hasMoreElements())
	{
		Ogre::SubMesh *sm = iter.getNext();
		if (!sm)
		{
			llwarns << "OGRE data validation error! Found a null submesh in mesh \"" << mesh.getName().c_str() << "\"!" << llendl;
		}
		else if (!sm->vertexData)
		{
			llwarns << "OGRE data validation error! Found a submesh (index " << smIndex << ") with null vertex data in mesh \"" << mesh.getName().c_str() << "\"!" << llendl;
		}
		else if (!sm->vertexData->vertexDeclaration)
		{
			llwarns << "OGRE data validation error! Found a submesh (index " << smIndex << ") with null vertex declaration in mesh \"" << mesh.getName().c_str() << "\"!" << llendl;
		}
		else
		{
			for(unsigned i = 0; i < sm->vertexData->vertexDeclaration->getElementCount(); ++i)
			{
				Ogre::VertexElementSemantic s = sm->vertexData->vertexDeclaration->getElement(i)->getSemantic();
				Ogre::VertexElementType t = sm->vertexData->vertexDeclaration->getElement(i)->getType();
				size_t size = sm->vertexData->vertexDeclaration->getElement(i)->getSize();
				llinfos << "Index " << i << ": " << SemanticToString(s) << ", " << TypeToString(t) << ", size: " << size << " bytes. " << llendl;
			}
		}
		++smIndex;
	}
}
// #endif

/// Computes some extreme points for the mesh. This is used so that Ogre can do proper Z sorting when rendering transparent objects.
/// The extreme point are really only needed for transparent objects, but opaque objects get them too.
void GenerateExtremities(Ogre::Mesh &mesh)
{
	// Only one extreme point should be enough, unless we have meshes where transparent submeshes get real close to each other.
	// The difference when using extreme points with Ogre seems to be that if no extreme points are computed, Ogre sorts objects by
	// the pivot of the entity, which is apparently the same for each subentity, and thus fails to distinguish submeshes at different positions.
	// (just a guess)
	const int cNumExtremePointsPerSubmesh = 1;

	for(int i = 0; i < mesh.getNumSubMeshes(); ++i)
	{
		Ogre::SubMesh *smesh = mesh.getSubMesh(i);
		assert(smesh);
		smesh->generateExtremes(cNumExtremePointsPerSubmesh);
	}
}

//! Create Ogre mesh from a buffer. Returns true if successful
bool LLOgreAssetLoader::createMesh(const LLUUID& assetID, U8* buffer, S32 size, BOOL TexFlipX, BOOL TexFlipY)
{
	LLOgreRenderer* renderer = LLOgreRenderer::getPointer();
	bool success = false;

	if (isMeshLoaded(assetID)) return true; // Already loaded

	// Buffer creation etc. follows, so switch context
	renderer->setOgreContext();

	try
	{		
		Ogre::MemoryDataStreamPtr stream(new Ogre::MemoryDataStream(buffer, size, false));
		Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().createManual(assetID.asString(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		Ogre::MeshSerializer serializer;
        Ogre::DataStreamPtr castPtr = static_cast<Ogre::DataStreamPtr>(stream);
		serializer.importMesh(castPtr, mesh.getPointer());


		// We build tangent data to all Ogre meshes that we load. Here's some room for optimization - only normal-mapped meshes really need the tangency data.
		try
        {
            unsigned short src, dest;
		    if (!mesh->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
    			mesh->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
        }
        catch (...)
        {
        }

		// Also, compute extreme points to allow Ogre do proper sorting of transparent submeshes inside a single mesh.
		GenerateExtremities(*mesh);

		if(TexFlipX)
			flipXTexCoords(mesh.getPointer());
		
		if(TexFlipY)
			flipYTexCoords(mesh.getPointer());

		success = true;

		//DumpOgreMeshVertexFormat(*mesh);
	}
	catch (Ogre::Exception e)
	{
		llwarns << "Error creating mesh from asset " << assetID << llendl;
		
		// Delete the empty/erroneous mesh if it exists
		if (!Ogre::MeshManager::getSingleton().getByName(assetID.asString()).isNull())
		{
			Ogre::MeshManager::getSingleton().remove(assetID.asString());
		}
	}

	renderer->setSLContext();
	return success;
}

bool LLOgreAssetLoader::createSkeleton(const LLUUID& assetID, U8* buffer, S32 size)
{
   LLOgreRenderer* renderer = LLOgreRenderer::getPointer();
	bool success = false;

	if (isSkeletonLoaded(assetID)) return true; // Already loaded

	// Buffer creation etc. follows, so switch context
	renderer->setOgreContext();

	try
	{		
		Ogre::MemoryDataStreamPtr stream(new Ogre::MemoryDataStream(buffer, size, false));
      Ogre::SkeletonPtr skeleton = Ogre::SkeletonManager::getSingleton().create(assetID.asString(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, true);
      Ogre::SkeletonSerializer serializer;
      Ogre::DataStreamPtr castPtr = static_cast<Ogre::DataStreamPtr>(stream);
		serializer.importSkeleton(castPtr, skeleton.getPointer());

		success = true;
	}
	catch (Ogre::Exception e)
	{
		llwarns << "Error creating skeleton from asset " << assetID << llendl;
		
		// Delete the empty/erroneous mesh if it exists
		if (!Ogre::SkeletonManager::getSingleton().getByName(assetID.asString()).isNull())
		{
			Ogre::SkeletonManager::getSingleton().remove(assetID.asString());
		}
	}

	renderer->setSLContext();
	return success;
}

//! Flip Y texture coordinates of an Ogre mesh
void LLOgreAssetLoader::flipYTexCoords(Ogre::Mesh* mesh)
{
	for (U32 i = 0; i < mesh->getNumSubMeshes(); ++i)
	{
		Ogre::SubMesh* subMesh = mesh->getSubMesh(i);
		if (!subMesh) 
			continue;

		Ogre::VertexData* vertexData = subMesh->vertexData;
		if (!vertexData) 
			continue;

		//! \todo support multiple texture coordinates
		const Ogre::VertexElement* elem = vertexData->vertexDeclaration->findElementBySemantic(Ogre::VES_TEXTURE_COORDINATES);
		
		if (elem)
		{
			// Make sure we got more than 1 dimension or this operation doesn't make sense
			if ((elem->getType() == Ogre::VET_FLOAT2) || (elem->getType() == Ogre::VET_FLOAT3))
			{
				Ogre::HardwareVertexBufferSharedPtr buf = vertexData->vertexBufferBinding->getBuffer(elem->getSource());
				U8* data = (U8*)(buf->lock(Ogre::HardwareBuffer::HBL_NORMAL));		
				U32 size = (U32)buf->getVertexSize();
				U32 count = (U32)vertexData->vertexCount;
				float* ptr;
	
				for (U32 j = 0; j < count; ++j)
				{
					elem->baseVertexPointerToElement(data, &ptr);
					++ptr; // Skip X coord
					*ptr++ = 1.0 - (*ptr); // Flip Y coord
									
					data += size;
				}

				buf->unlock();
			}
		}
	}
}

//! Flip X texture coordinates of an Ogre mesh
void LLOgreAssetLoader::flipXTexCoords(Ogre::Mesh* mesh)
{
	for (U32 i = 0; i < mesh->getNumSubMeshes(); ++i)
	{
		Ogre::SubMesh* subMesh = mesh->getSubMesh(i);
		if (!subMesh) 
			continue;

		Ogre::VertexData* vertexData = subMesh->vertexData;
		if (!vertexData) 
			continue;

		//! \todo support multiple texture coordinates
		const Ogre::VertexElement* elem = vertexData->vertexDeclaration->findElementBySemantic(Ogre::VES_TEXTURE_COORDINATES);
		
		if (elem)
		{
			// Make sure we got more than 1 dimension or this operation doesn't make sense
			if ((elem->getType() == Ogre::VET_FLOAT2) || (elem->getType() == Ogre::VET_FLOAT3))
			{
				Ogre::HardwareVertexBufferSharedPtr buf = vertexData->vertexBufferBinding->getBuffer(elem->getSource());
				U8* data = (U8*)(buf->lock(Ogre::HardwareBuffer::HBL_NORMAL));		
				U32 size = (U32)buf->getVertexSize();
				U32 count = (U32)vertexData->vertexCount;
				float* ptr;
	
				for (U32 j = 0; j < count; ++j)
				{
					elem->baseVertexPointerToElement(data, &ptr);
					
					*ptr++ = 1.0 - (*ptr); // Flip X coord
					 ++ptr; // Skip Y coord
									
					data += size;
				}

				buf->unlock();
			}
		}
	}
}

//! Create Ogre material from a buffer containing script. Returns true if successful
bool LLOgreAssetLoader::createMaterial(const LLUUID& assetID, U8* buffer, S32 size)
{
    RexOgreMaterial* mat = getMaterial(assetID);
    if (!mat)
        mat = mMaterials[assetID] = new RexOgreMaterial(assetID.asString());

    if (!mat->isLoaded())
        return mat->create(buffer, size);
    else return true;
}

//! Create Ogre particle system template(s) from a buffer containing script. Returns true if successful
bool LLOgreAssetLoader::createParticleScript(const LLUUID& assetID, U8* buffer, S32 size)
{
    RexParticleScript* script = getParticleScript(assetID);
    if (!script)
        script = mParticleScripts[assetID] = new RexParticleScript(assetID);

    if (!script->getNumParticleSystems())
        return script->create(buffer, size);
    else return true;
}


//! Purge asset so it can be reloaded into Ogre after reupload
void LLOgreAssetLoader::purgeAsset(const LLUUID& assetID, LLAssetType::EType assetType)
{
    switch(assetType)
    {
    // So far only particle scripts supported
    case LLAssetType::AT_PARTICLE_SCRIPT:
        RexParticleScript* script = getParticleScript(assetID);
        if (script)
            script->purge();
        break;
    }
}

bool LLOgreAssetLoader::processBraces(const std::string& line, int& braceLevel)
{
    if (line == "{")
    {
        ++braceLevel;
        return true;
    } 
    else if (line == "}")
    {
        --braceLevel;
        return true;
    }
    else return false;
}
