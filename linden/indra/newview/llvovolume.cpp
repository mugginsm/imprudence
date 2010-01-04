/** 
 * @file llvovolume.cpp
 * @brief LLVOVolume class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

// A "volume" is a box, cylinder, sphere, or other primitive shape.

#include "llviewerprecompiledheaders.h"

#include "llvovolume.h"

#include "llviewercontrol.h"
#include "lldir.h"
#include "llflexibleobject.h"
#include "llmaterialtable.h"
#include "llprimitive.h"
#include "llvolume.h"
#include "llvolumemgr.h"
#include "llvolumemessage.h"
#include "material_codes.h"
#include "message.h"
#include "object_flags.h"
#include "llagent.h"
#include "lldrawable.h"
#include "lldrawpoolbump.h"
#include "llface.h"
#include "llspatialpartition.h"

// TEMP HACK ventrella
#include "llhudmanager.h"
#include "llflexibleobject.h"

#include "llsky.h"
#include "llviewercamera.h"
#include "llviewerimagelist.h"
#include "llviewerregion.h"
#include "llviewertextureanim.h"
#include "llworld.h"
#include "llselectmgr.h"
#include "pipeline.h"

// reX: new includes
#include <OGRE/Ogre.h>
#include "llogre.h"
#include "llogreobject.h"
#include "llogreassetloader.h"
#include "llogre.h"
#include "llogrefont.h"
#include "llappviewer.h"
#include "llhoverview.h"
#include "rexpaneldata.h"
#include "rexogrelegacymaterial.h"
#include "rexogrematerial.h"
#include "rexparticlescript.h"
#include "audioengine.h"	// LLAudioSource & gAudiop

const S32 MIN_QUIET_FRAMES_COALESCE = 30;
const F32 FORCE_SIMPLE_RENDER_AREA = 512.f;
const F32 FORCE_CULL_AREA = 8.f;
const S32 MAX_SCULPT_REZ = 128;

BOOL gAnimateTextures = TRUE;
extern BOOL gHideSelectedObjects;
extern LLAudioEngine* gAudiop;		// reX

F32 LLVOVolume::sLODFactor = 1.f;
F32	LLVOVolume::sLODSlopDistanceFactor = 0.5f; //Changing this to zero, effectively disables the LOD transition slop 
F32 LLVOVolume::sDistanceFactor = 1.0f;
S32 LLVOVolume::sNumLODChanges = 0;

// reX: Whether we're in a reX world: use maximum LOD for prims there to eliminate slowdown from lod changes
extern BOOL gInRex;
// reX: Extended prim properties only allowed in pure rex mode (?)
extern BOOL gInPureRex;
// reX: Whether using Ogre rendering currently
extern BOOL gOgreRender;

// From llface.cpp
void xform(LLVector2 &tex_coord, F32 cosAng, F32 sinAng, F32 offS, F32 offT, F32 magS, F32 magT);
void planarProjection(LLVector2 &tc, const LLVolumeFace::VertexData &vd, const LLVector3 &mCenter, const LLVector3& vec);
void sphericalProjection(LLVector2 &tc, const LLVolumeFace::VertexData &vd, const LLVector3 &mCenter, const LLVector3& vec);
void cylindricalProjection(LLVector2 &tc, const LLVolumeFace::VertexData &vd, const LLVector3 &mCenter, const LLVector3& vec);

LLVOVolume::LLVOVolume(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp)
	: LLViewerObject(id, pcode, regionp),
	// reX
   mVolumeImpl(NULL), mMeshFullbright(FALSE), mMeshSolidAlpha(FALSE), mForceTextureAreaUpdate(FALSE), mFixedMaterial(FIXEDMATERIAL_DEFAULT),
   mCurrentParticleScriptID(LLUUID::null),
   mCurrentSkeletonID(LLUUID::null),
   mSkeletonUpdate(FALSE),
   mParticleFirstTime(TRUE),
   mSkeletonFirstTime(TRUE),//, mUseClonedOgreMaterial(false), mRecreateClonedMaterial(false)
   mRexPrimDataReceived(false)
{
	mTexAnimMode = 0;
	mRelativeXform.setIdentity();
	mRelativeXformInvTrans.setIdentity();

	// reX
	if (!gInRex)
	{
		mLOD = MIN_LOD;
	}
	else
	{
		mLOD = MAX_LOD;
	}
	mSculptLevel = -2;
	mTextureAnimp = NULL;
	mVObjRadius = LLVector3(1,1,0.5f).length();
	mNumFaces = 0;
	mLODChanged = FALSE;
	mSculptChanged = FALSE;

    // reX, check for queued RexData & RexPrimData in case we received them before the object existed
    RexPanelData::getQueuedRexData(id, mRexData);
    if (RexPrimData::getQueuedRexPrimData(id, mRexPrimData))
        mRexPrimDataReceived = true;
}

LLVOVolume::~LLVOVolume()
{
	delete mTextureAnimp;
	mTextureAnimp = NULL;
	delete mVolumeImpl;
	mVolumeImpl = NULL;
	createRexSoundSource(FALSE);	// reX
}


// static
void LLVOVolume::initClass()
{
}


U32 LLVOVolume::processUpdateMessage(LLMessageSystem *mesgsys,
										  void **user_data,
										  U32 block_num, EObjectUpdateType update_type,
										  LLDataPacker *dp)
{
	LLColor4U color;

	// Do base class updates...
	U32 retval = LLViewerObject::processUpdateMessage(mesgsys, user_data, block_num, update_type, dp);

	LLUUID sculpt_id;
	U8 sculpt_type = 0;
	if (isSculpted())
	{
		LLSculptParams *sculpt_params = (LLSculptParams *)getParameterEntry(LLNetworkData::PARAMS_SCULPT);
		sculpt_id = sculpt_params->getSculptTexture();
		sculpt_type = sculpt_params->getSculptType();
	}

	if (!dp)
	{
		if (update_type == OUT_FULL)
		{
			////////////////////////////////
			//
			// Unpack texture animation data
			//
			//

			if (mesgsys->getSizeFast(_PREHASH_ObjectData, block_num, _PREHASH_TextureAnim))
			{
				if (!mTextureAnimp)
				{
					mTextureAnimp = new LLViewerTextureAnim();
				}
				else
				{
					if (!(mTextureAnimp->mMode & LLTextureAnim::SMOOTH))
					{
						mTextureAnimp->reset();
					}
				}
				mTexAnimMode = 0;
				mTextureAnimp->unpackTAMessage(mesgsys, block_num);
			}
			else
			{
				if (mTextureAnimp)
				{
					delete mTextureAnimp;
					mTextureAnimp = NULL;
					gPipeline.markTextured(mDrawable);
					mFaceMappingChanged = TRUE;
					mTexAnimMode = 0;
				}
			}

			// Unpack volume data
			LLVolumeParams volume_params;
			LLVolumeMessage::unpackVolumeParams(&volume_params, mesgsys, _PREHASH_ObjectData, block_num);
			volume_params.setSculptID(sculpt_id, sculpt_type);

			if (setVolume(volume_params, 0))
			{
				markForUpdate(TRUE);
			}
		}

		// Sigh, this needs to be done AFTER the volume is set as well, otherwise bad stuff happens...
		////////////////////////////
		//
		// Unpack texture entry data
		//
		if (unpackTEMessage(mesgsys, _PREHASH_ObjectData, block_num) & (TEM_CHANGE_TEXTURE|TEM_CHANGE_COLOR))
		{
			updateTEData();
		}
	}
	else
	{
		// CORY TO DO: Figure out how to get the value here
		if (update_type != OUT_TERSE_IMPROVED)
		{
			LLVolumeParams volume_params;
			BOOL res = LLVolumeMessage::unpackVolumeParams(&volume_params, *dp);
			if (!res)
			{
				llwarns << "Bogus volume parameters in object " << getID() << llendl;
				llwarns << getRegion()->getOriginGlobal() << llendl;
			}

			volume_params.setSculptID(sculpt_id, sculpt_type);

			if (setVolume(volume_params, 0))
			{
				markForUpdate(TRUE);
			}
			S32 res2 = unpackTEMessage(*dp);
			if (TEM_INVALID == res2)
			{
				// Well, crap, there's something bogus in the data that we're unpacking.
				dp->dumpBufferToLog();
				llwarns << "Flushing cache files" << llendl;
				std::string mask;
				mask = gDirUtilp->getDirDelimiter() + "*.slc";
				gDirUtilp->deleteFilesInDir(gDirUtilp->getExpandedFilename(LL_PATH_CACHE,""), mask);
// 				llerrs << "Bogus TE data in " << getID() << ", crashing!" << llendl;
				llwarns << "Bogus TE data in " << getID() << llendl;
			}
			else if (res2 & (TEM_CHANGE_TEXTURE|TEM_CHANGE_COLOR))
			{
				updateTEData();
			}

			U32 value = dp->getPassFlags();

			if (value & 0x40)
			{
				if (!mTextureAnimp)
				{
					mTextureAnimp = new LLViewerTextureAnim();
				}
				else
				{
					if (!(mTextureAnimp->mMode & LLTextureAnim::SMOOTH))
					{
						mTextureAnimp->reset();
					}
				}
				mTexAnimMode = 0;
				mTextureAnimp->unpackTAMessage(*dp);
			}
			else if (mTextureAnimp)
			{
				delete mTextureAnimp;
				mTextureAnimp = NULL;
				gPipeline.markTextured(mDrawable);
				mFaceMappingChanged = TRUE;
				mTexAnimMode = 0;
			}
		}
		else
		{
			S32 texture_length = mesgsys->getSizeFast(_PREHASH_ObjectData, block_num, _PREHASH_TextureEntry);
			if (texture_length)
			{
				U8							tdpbuffer[1024];
				LLDataPackerBinaryBuffer	tdp(tdpbuffer, 1024);
				mesgsys->getBinaryDataFast(_PREHASH_ObjectData, _PREHASH_TextureEntry, tdpbuffer, 0, block_num);
				if ( unpackTEMessage(tdp) & (TEM_CHANGE_TEXTURE|TEM_CHANGE_COLOR))
				{
					updateTEData();
				}
			}
		}
	}
	
	return retval;
}


void LLVOVolume::animateTextures()
{
	F32 off_s = 0.f, off_t = 0.f, scale_s = 1.f, scale_t = 1.f, rot = 0.f;
	S32 result = mTextureAnimp->animateTextures(off_s, off_t, scale_s, scale_t, rot);
	
	if (result)
	{
		if (!mTexAnimMode)
		{
			mFaceMappingChanged = TRUE;
			gPipeline.markTextured(mDrawable);
		}
		mTexAnimMode = result | mTextureAnimp->mMode;
				
		S32 start=0, end=mDrawable->getNumFaces()-1;
		if (mTextureAnimp->mFace >= 0 && mTextureAnimp->mFace <= end)
		{
			start = end = mTextureAnimp->mFace;
		}
		
		for (S32 i = start; i <= end; i++)
		{
			LLFace* facep = mDrawable->getFace(i);
			if(facep->getVirtualSize() <= MIN_TEX_ANIM_SIZE && facep->mTextureMatrix) continue;

			const LLTextureEntry* te = facep->getTextureEntry();
			
			if (!te)
			{
				continue;
			}
		
			if (!(result & LLViewerTextureAnim::ROTATE))
			{
				te->getRotation(&rot);
			}
			if (!(result & LLViewerTextureAnim::TRANSLATE))
			{
				te->getOffset(&off_s,&off_t);
			}			
			if (!(result & LLViewerTextureAnim::SCALE))
			{
				te->getScale(&scale_s, &scale_t);
			}

			LLVector3 scale(scale_s, scale_t, 1.f);
			LLVector3 trans(off_s+0.5f, off_t+0.5f, 0.f);
			LLQuaternion quat;
			quat.setQuat(rot, 0, 0, -1.f);
		
			if (!facep->mTextureMatrix)
			{
				facep->mTextureMatrix = new LLMatrix4();
			}

			LLMatrix4& tex_mat = *facep->mTextureMatrix;
			tex_mat.setIdentity();
			tex_mat.translate(LLVector3(-0.5f, -0.5f, 0.f));
			tex_mat.rotate(quat);				

			LLMatrix4 mat;
			mat.initAll(scale, LLQuaternion(), LLVector3());
			tex_mat *= mat;
		
			tex_mat.translate(trans);
		}
	}
	else
	{
		if (mTexAnimMode && mTextureAnimp->mRate == 0)
		{
			U8 start, count;

			if (mTextureAnimp->mFace == -1)
			{
				start = 0;
				count = getNumTEs();
			}
			else
			{
				start = (U8) mTextureAnimp->mFace;
				count = 1;
			}

			for (S32 i = start; i < start + count; i++)
			{
				if (mTexAnimMode & LLViewerTextureAnim::TRANSLATE)
				{
					setTEOffset(i, mTextureAnimp->mOffS, mTextureAnimp->mOffT);				
				}
				if (mTexAnimMode & LLViewerTextureAnim::SCALE)
				{
					setTEScale(i, mTextureAnimp->mScaleS, mTextureAnimp->mScaleT);	
				}
				if (mTexAnimMode & LLViewerTextureAnim::ROTATE)
				{
					setTERotation(i, mTextureAnimp->mRot);
				}
			}

			gPipeline.markTextured(mDrawable);
			mFaceMappingChanged = TRUE;
			mTexAnimMode = 0;
		}
	}
}
BOOL LLVOVolume::idleUpdate(LLAgent &agent, LLWorld &world, const F64 &time)
{
	LLViewerObject::idleUpdate(agent, world, time);

	if (mDead || mDrawable.isNull())
	{
		return TRUE;
	}
	
	///////////////////////
	//
	// Do texture animation stuff
	//

	if (mTextureAnimp && gAnimateTextures)
	{
		animateTextures();
	}

	// Dispatch to implementation
	if (mVolumeImpl)
	{
		mVolumeImpl->doIdleUpdate(agent, world, time);
	}

	return TRUE;
}

void LLVOVolume::updateTextures(LLAgent &agent)
{
	const F32 TEXTURE_AREA_REFRESH_TIME = 5.f; // seconds
	
	// reX: added forcing of texture area update
	if ((mForceTextureAreaUpdate) || (mDrawable.notNull() && mTextureUpdateTimer.getElapsedTimeF32() > TEXTURE_AREA_REFRESH_TIME))
	{
		mForceTextureAreaUpdate = FALSE;
		if (mDrawable->isVisible())
		{
			updateTextures();
		}
	}
}

void LLVOVolume::updateTextures()
{
	// reX
	//! Size accumulator of all faces for Ogre mesh textures
	F32 accumSize = 0.0;

	// Update the pixel area of all faces

	if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SIMPLE))
	{
		return;
	}
	
	if (LLViewerImage::sDontLoadVolumeTextures || mDrawable.isNull()) // || !mDrawable->isVisible())
	{
		return;
	}

	mTextureUpdateTimer.reset();
	
	F32 old_area = mPixelArea;
	mPixelArea = 0.f;

	const S32 num_faces = mDrawable->getNumFaces();
	F32 min_vsize=999999999.f, max_vsize=0.f;
	for (S32 i = 0; i < num_faces; i++)
	{
		LLFace* face = mDrawable->getFace(i);
		const LLTextureEntry *te = face->getTextureEntry();
		LLViewerImage *imagep = face->getTexture();
		if (!imagep || !te ||
			face->mExtents[0] == face->mExtents[1])
		{
			continue;
		}
		
		F32 vsize;
		
		if (isHUDAttachment())
		{
			F32 area = (F32) LLViewerCamera::getInstance()->getScreenPixelArea();
			vsize = area;
			imagep->setBoostLevel(LLViewerImage::BOOST_HUD);
 			face->setPixelArea(area); // treat as full screen
		}
		else
		{
			vsize = getTextureVirtualSize(face);
		}

		mPixelArea = llmax(mPixelArea, face->getPixelArea());

		F32 old_size = face->getVirtualSize();

		if (face->mTextureMatrix != NULL)
		{
			if (vsize < MIN_TEX_ANIM_SIZE && old_size > MIN_TEX_ANIM_SIZE ||
				vsize > MIN_TEX_ANIM_SIZE && old_size < MIN_TEX_ANIM_SIZE)
			{
				gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_TCOORD, FALSE);
			}
		}
		
		face->setVirtualSize(vsize);
		imagep->addTextureStats(vsize);
		
		// reX
        if (gOgreRender) imagep->resetBindTime(); // Fake texture binding in Ogre mode
		accumSize += vsize;		
		
		if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_AREA))
		{
			if (vsize < min_vsize) min_vsize = vsize;
			if (vsize > max_vsize) max_vsize = vsize;
		}
		else if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY))
		{
			F32 pri = imagep->getDecodePriority();
			pri = llmax(pri, 0.0f);
			if (pri < min_vsize) min_vsize = pri;
			if (pri > max_vsize) max_vsize = pri;
		}
		else if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_FACE_AREA))
		{
			F32 pri = mPixelArea;
			if (pri < min_vsize) min_vsize = pri;
			if (pri > max_vsize) max_vsize = pri;
		}	
	}
	
	if (isSculpted())
	{
		LLSculptParams *sculpt_params = (LLSculptParams *)getParameterEntry(LLNetworkData::PARAMS_SCULPT);
		LLUUID id =  sculpt_params->getSculptTexture(); 
		mSculptTexture = gImageList.getImage(id);
		if (mSculptTexture.notNull())
		{
			S32 lod = llmin(mLOD, 3);
			F32 lodf = ((F32)(lod + 1.0f)/4.f); 
			F32 tex_size = lodf * MAX_SCULPT_REZ;
			mSculptTexture->addTextureStats(2.f * tex_size * tex_size);
			mSculptTexture->setBoostLevel(llmax((S32)mSculptTexture->getBoostLevel(),
												(S32)LLViewerImage::BOOST_SCULPTED));
		}

		S32 texture_discard = mSculptTexture->getDiscardLevel(); //try to match the texture
		S32 current_discard = mSculptLevel;

		if (texture_discard >= 0 && //texture has some data available
			(texture_discard < current_discard || //texture has more data than last rebuild
			current_discard < 0)) //no previous rebuild
		{
			gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME, FALSE);

			// reX
			markOgreUpdate();
			mSculptChanged = TRUE;
		}

		if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SCULPTED))
			{
				setDebugText(llformat("T%d C%d V%d\n%dx%d",
									  texture_discard, current_discard, getVolume()->getSculptLevel(),
									  mSculptTexture->getHeight(), mSculptTexture->getWidth()));
			}
	}


	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_AREA))
	{
		setDebugText(llformat("%.0f:%.0f", fsqrtf(min_vsize),fsqrtf(max_vsize)));
	}
	else if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY))
	{
		setDebugText(llformat("%.0f:%.0f", fsqrtf(min_vsize),fsqrtf(max_vsize)));
	}
	else if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_FACE_AREA))
	{
		setDebugText(llformat("%.0f:%.0f", fsqrtf(min_vsize),fsqrtf(max_vsize)));
	}

	if (mPixelArea == 0)
	{ //flexi phasing issues make this happen
		mPixelArea = old_area;
	}

	// reX
	// hack: pump up texture stats for Ogre mesh textures & materials (based on the underlying prim's total screen space), also reset their bind time
    if (getRexIsMesh() || getRexIsBillboard())
    {
		for (U32 i = 0; i < mOgreTextures.size(); ++i)
	    {
			if (mOgreTextures[i].notNull())			
		    {
				LLViewerImage* image = mOgreTextures[i];
			    image->addTextureStats(accumSize);
			    image->resetBindTime();
		    }
	    }
    }

    for (U32 i = 0; i < mOgreMaterials.size(); ++i)
    {
        if (mOgreMaterials[i])
            mOgreMaterials[i]->pumpImages(accumSize);
    }

    pumpParticleTextures();
}

// reX: new function
void LLVOVolume::pumpParticleTextures()
{
    if (mCurrentParticleScriptID != LLUUID::null)
    {
	    LLOgreAssetLoader* assetLoader = LLOgreRenderer::getPointer()->getAssetLoader();
	    if (assetLoader)
        {
            RexParticleScript* script = assetLoader->getParticleScript(mCurrentParticleScriptID);
            if (script) script->pumpImages();
        }
    }
}

F32 LLVOVolume::getTextureVirtualSize(LLFace* face)
{
	//get area of circle around face
	LLVector3 center = face->getPositionAgent();
	LLVector3 size = (face->mExtents[1] - face->mExtents[0]) * 0.5f;
	
	F32 face_area = LLPipeline::calcPixelArea(center, size, *LLViewerCamera::getInstance());

	face->setPixelArea(face_area);

	if (face_area <= 0)
	{
		return 0.f;
	}

	//get area of circle in texture space
	LLVector2 tdim = face->mTexExtents[1] - face->mTexExtents[0];
	F32 texel_area = (tdim * 0.5f).lengthSquared()*3.14159f;
	if (texel_area <= 0)
	{
		// Probably animated, use default
		texel_area = 1.f;
	}

	//apply texel area to face area to get accurate ratio
	face_area /= llclamp(texel_area, 1.f/64.f, 16.f);

	return face_area;
}

BOOL LLVOVolume::isActive() const
{
	return !mStatic || mTextureAnimp || (mVolumeImpl && mVolumeImpl->isActive());
}

BOOL LLVOVolume::setMaterial(const U8 material)
{
	BOOL res = LLViewerObject::setMaterial(material);
	
	return res;
}

void LLVOVolume::setTexture(const S32 face)
{
	llassert(face < getNumTEs());
	gGL.getTexUnit(0)->bind(getTEImage(face));
}

void LLVOVolume::setScale(const LLVector3 &scale, BOOL damped)
{
	if (scale != getScale())
	{
		// store local radius
		LLViewerObject::setScale(scale);

		if (mVolumeImpl)
		{
			mVolumeImpl->onSetScale(scale, damped);
		}
		
		updateRadius();

		//since drawable transforms do not include scale, changing volume scale
		//requires an immediate rebuild of volume verts.
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_POSITION, TRUE);
		// reX
		markOgreUpdate();
	}
}

LLFace* LLVOVolume::addFace(S32 f)
{
	const LLTextureEntry* te = getTE(f);
	LLViewerImage* imagep = getTEImage(f);
	return mDrawable->addFace(te, imagep);
}

LLDrawable *LLVOVolume::createDrawable(LLPipeline *pipeline)
{
	pipeline->allocDrawable(this);
		
	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_VOLUME);

	S32 max_tes_to_set = getNumTEs();
	for (S32 i = 0; i < max_tes_to_set; i++)
	{
		addFace(i);
	}
	mNumFaces = max_tes_to_set;

	if (isAttachment())
	{
		mDrawable->makeActive();
	}

	if (getIsLight())
	{
		// Add it to the pipeline mLightSet
		gPipeline.setLight(mDrawable, TRUE);
	}
	
	// reX
	// Create Ogre object, no offsetnode
	if ((!mOgreObject) && (gInRex))
	{
		mOgreObject = new LLOgreObject(LLOgreRenderer::getPointer(), false);
        mOgreObject->setViewerObject(this);
	}
	updateOgreLight();

	updateRadius();
	bool force_update = true; // avoid non-alpha mDistance update being optimized away
	mDrawable->updateDistance(*LLViewerCamera::getInstance(), force_update);

	return mDrawable;
}


BOOL LLVOVolume::setVolume(const LLVolumeParams &volume_params, const S32 detail, bool unique_volume)
{
	// Check if we need to change implementations
	bool is_flexible = (volume_params.getPathParams().getCurveType() == LL_PCODE_PATH_FLEXIBLE);
	if (is_flexible)
	{
		setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, TRUE, false);
		if (!mVolumeImpl)
		{
			LLFlexibleObjectData* data = (LLFlexibleObjectData*)getParameterEntry(LLNetworkData::PARAMS_FLEXIBLE);
			mVolumeImpl = new LLVolumeImplFlexible(this, data);
		}
	}
	else
	{
		// Mark the parameter not in use
		setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, FALSE, false);
		if (mVolumeImpl)
		{
			delete mVolumeImpl;
			mVolumeImpl = NULL;
			if (mDrawable.notNull())
			{
				// Undo the damage we did to this matrix
				mDrawable->updateXform(FALSE);
			}
		}
	}
	
	if ((LLPrimitive::setVolume(volume_params, mLOD, (mVolumeImpl && mVolumeImpl->isVolumeUnique()))) || mSculptChanged)
	{
		mFaceMappingChanged = TRUE;
		
		if (mVolumeImpl)
		{
			mVolumeImpl->onSetVolume(volume_params, detail);
		}
		
		if (isSculpted())
		{
			mSculptTexture = gImageList.getImage(volume_params.getSculptID());
			if (mSculptTexture.notNull())
			{
				sculpt();
				mSculptLevel = getVolume()->getSculptLevel();
			}
		}
		else
		{
			mSculptTexture = NULL;
		}

		return TRUE;
	}
	return FALSE;
}

// sculpt replaces generate() for sculpted surfaces
void LLVOVolume::sculpt()
{
	U16 sculpt_height = 0;
	U16 sculpt_width = 0;
	S8 sculpt_components = 0;
	const U8* sculpt_data = NULL;

	if (mSculptTexture.notNull())
	{
		S32 discard_level;
		S32 desired_discard = 0; // lower discard levels have MUCH less resolution 

		discard_level = desired_discard;
		
		S32 max_discard = mSculptTexture->getMaxDiscardLevel();
		if (discard_level > max_discard)
			discard_level = max_discard;    // clamp to the best we can do

		S32 best_discard = mSculptTexture->getDiscardLevel();
		if (discard_level < best_discard)
			discard_level = best_discard;   // clamp to what we have

		if (best_discard == -1)
			discard_level = -1;  // and if we have nothing, set to nothing

		
		S32 current_discard = getVolume()->getSculptLevel();
		if(current_discard < -2)
		{
			llwarns << "WARNING!!: Current discard of sculpty at " << current_discard 
				<< " is less than -2." << llendl;
			
			// corrupted volume... don't update the sculpty
			return;
		}
		else if (current_discard > MAX_DISCARD_LEVEL)
		{
			llwarns << "WARNING!!: Current discard of sculpty at " << current_discard 
				<< " is more than than allowed max of " << MAX_DISCARD_LEVEL << llendl;
			
			// corrupted volume... don't update the sculpty			
			return;
		}

		if (current_discard == discard_level)  // no work to do here
			return;
		
		LLPointer<LLImageRaw> raw_image = new LLImageRaw();
		BOOL is_valid = mSculptTexture->readBackRaw(discard_level, raw_image, FALSE);

		sculpt_height = raw_image->getHeight();
		sculpt_width = raw_image->getWidth();
		sculpt_components = raw_image->getComponents();		

		if(is_valid)
		{
			is_valid = mSculptTexture->isValidForSculpt(discard_level, sculpt_width, sculpt_height, sculpt_components) ;
		}
		if(!is_valid)
		{
			sculpt_width = 0;
			sculpt_height = 0;
			sculpt_data = NULL ;
		}
		else
		{
			if (raw_image->getDataSize() < sculpt_height * sculpt_width * sculpt_components)
				llerrs << "Sculpt: image data size = " << raw_image->getDataSize()
					   << " < " << sculpt_height << " x " << sculpt_width << " x " <<sculpt_components << llendl;
					   
			sculpt_data = raw_image->getData();
		}
		getVolume()->sculpt(sculpt_width, sculpt_height, sculpt_components, sculpt_data, discard_level);
	}
}

S32	LLVOVolume::computeLODDetail(F32 distance, F32 radius)
{
	S32	cur_detail;
	if (LLPipeline::sDynamicLOD)
	{
		// We've got LOD in the profile, and in the twist.  Use radius.
		F32 tan_angle = (LLVOVolume::sLODFactor*radius)/distance;
		cur_detail = LLVolumeLODGroup::getDetailFromTan(llround(tan_angle, 0.01f));
	}
	else
	{
		cur_detail = llclamp((S32) (sqrtf(radius)*LLVOVolume::sLODFactor*4.f), 0, 3);		
	}
	return cur_detail;
}

BOOL LLVOVolume::calcLOD()
{
	if (mDrawable.isNull())
	{
		return FALSE;
	}

	//update face texture sizes on lod calculation
	updateTextures();

	S32 cur_detail = 0;
	
	F32 radius = getVolume()->mLODScaleBias.scaledVec(getScale()).length();
	F32 distance = mDrawable->mDistanceWRTCamera;
	distance *= sDistanceFactor;
			
	F32 rampDist = LLVOVolume::sLODFactor * 2;
	
	if (distance < rampDist)
	{
		// Boost LOD when you're REALLY close
		distance *= 1.0f/rampDist;
		distance *= distance;
		distance *= rampDist;
	}
	
	// DON'T Compensate for field of view changing on FOV zoom.
	distance *= 3.14159f/3.f;

	// reX
	if (!gInRex)
	{
	cur_detail = computeLODDetail(llround(distance, 0.01f), 
									llround(radius, 0.01f));
	}
	else
	{
		cur_detail = MAX_LOD;
	}

	if (cur_detail != mLOD)
	{
		mAppAngle = llround((F32) atan2( mDrawable->getRadius(), mDrawable->mDistanceWRTCamera) * RAD_TO_DEG, 0.01f);
		mLOD = cur_detail;		
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL LLVOVolume::updateLOD()
{
	if (mDrawable.isNull())
	{
		return FALSE;
	}
	
	BOOL lod_changed = calcLOD();

	if (lod_changed)
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME, FALSE);
		// reX
		markOgreUpdate();
		mLODChanged = TRUE;
	}

	lod_changed |= LLViewerObject::updateLOD();
	
	return lod_changed;
}

BOOL LLVOVolume::setDrawableParent(LLDrawable* parentp)
{
	if (!LLViewerObject::setDrawableParent(parentp))
	{
		// no change in drawable parent
		return FALSE;
	}

	if (!mDrawable->isRoot())
	{
		// rebuild vertices in parent relative space
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME, TRUE);
		// reX
		markOgreUpdate();

		if (mDrawable->isActive() && !parentp->isActive())
		{
			parentp->makeActive();
		}
		else if (mDrawable->isStatic() && parentp->isActive())
		{
			mDrawable->makeActive();
		}
	}
	
	return TRUE;
}

void LLVOVolume::updateFaceFlags()
{
	for (S32 i = 0; i < getVolume()->getNumFaces(); i++)
	{
		LLFace *face = mDrawable->getFace(i);
		BOOL fullbright = getTE(i)->getFullbright();
		face->clearState(LLFace::FULLBRIGHT | LLFace::HUD_RENDER | LLFace::LIGHT);

		if (fullbright || (mMaterial == LL_MCODE_LIGHT))
		{
			face->setState(LLFace::FULLBRIGHT);
		}
		if (mDrawable->isLight())
		{
			face->setState(LLFace::LIGHT);
		}
		if (isHUDAttachment())
		{
			face->setState(LLFace::HUD_RENDER);
		}
	}
}

void LLVOVolume::setParent(LLViewerObject* parent)
{
	if (parent != getParent())
	{
		LLViewerObject::setParent(parent);
		if (mDrawable)
		{
			gPipeline.markMoved(mDrawable);
			gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME, TRUE);

			// reX
			markOgreUpdate();
		}
	}
}

// NOTE: regenFaces() MUST be followed by genTriangles()!
void LLVOVolume::regenFaces()
{
	// remove existing faces
	BOOL count_changed = mNumFaces != getNumTEs();
	
	if (count_changed)
	{
		deleteFaces();		
		// add new faces
		mNumFaces = getNumTEs();
	}
		
	for (S32 i = 0; i < mNumFaces; i++)
	{
		LLFace* facep = count_changed ? addFace(i) : mDrawable->getFace(i);
		facep->setTEOffset(i);
		facep->setTexture(getTEImage(i));
		facep->setViewerObject(this);
	}
	
	if (!count_changed)
	{
		updateFaceFlags();
	}
}

BOOL LLVOVolume::genBBoxes(BOOL force_global)
{
	BOOL res = TRUE;

	LLVector3 min,max;

	BOOL rebuild = mDrawable->isState(LLDrawable::REBUILD_VOLUME | LLDrawable::REBUILD_POSITION);

	for (S32 i = 0; i < getVolume()->getNumFaces(); i++)
	{
		LLFace *face = mDrawable->getFace(i);
		res &= face->genVolumeBBoxes(*getVolume(), i,
										mRelativeXform, mRelativeXformInvTrans,
										(mVolumeImpl && mVolumeImpl->isVolumeGlobal()) || force_global);
		
		if (rebuild)
		{
			if (i == 0)
			{
				min = face->mExtents[0];
				max = face->mExtents[1];
			}
			else
			{
				for (U32 i = 0; i < 3; i++)
				{
					if (face->mExtents[0].mV[i] < min.mV[i])
					{
						min.mV[i] = face->mExtents[0].mV[i];
					}
					if (face->mExtents[1].mV[i] > max.mV[i])
					{
						max.mV[i] = face->mExtents[1].mV[i];
					}
				}
			}
		}
	}
	
	if (rebuild)
	{
		mDrawable->setSpatialExtents(min,max);
		mDrawable->setPositionGroup((min+max)*0.5f);	
	}

	updateRadius();
	mDrawable->movePartition();
			
	return res;
}

void LLVOVolume::preRebuild()
{
	if (mVolumeImpl != NULL)
	{
		mVolumeImpl->preRebuild();
	}
}

void LLVOVolume::updateRelativeXform()
{
	if (mVolumeImpl)
	{
		mVolumeImpl->updateRelativeXform();
		return;
	}
	
	LLDrawable* drawable = mDrawable;
	
	if (drawable->isActive())
	{				
		// setup relative transforms
		LLQuaternion delta_rot;
		LLVector3 delta_pos, delta_scale;
		
		//matrix from local space to parent relative/global space
		delta_rot = drawable->isSpatialRoot() ? LLQuaternion() : mDrawable->getRotation();
		delta_pos = drawable->isSpatialRoot() ? LLVector3(0,0,0) : mDrawable->getPosition();
		delta_scale = mDrawable->getScale();

		// Vertex transform (4x4)
		LLVector3 x_axis = LLVector3(delta_scale.mV[VX], 0.f, 0.f) * delta_rot;
		LLVector3 y_axis = LLVector3(0.f, delta_scale.mV[VY], 0.f) * delta_rot;
		LLVector3 z_axis = LLVector3(0.f, 0.f, delta_scale.mV[VZ]) * delta_rot;

		mRelativeXform.initRows(LLVector4(x_axis, 0.f),
								LLVector4(y_axis, 0.f),
								LLVector4(z_axis, 0.f),
								LLVector4(delta_pos, 1.f));

		
		// compute inverse transpose for normals
		// mRelativeXformInvTrans.setRows(x_axis, y_axis, z_axis);
		// mRelativeXformInvTrans.invert(); 
		// mRelativeXformInvTrans.setRows(x_axis, y_axis, z_axis);
		// grumble - invert is NOT a matrix invert, so we do it by hand:

		LLMatrix3 rot_inverse = LLMatrix3(~delta_rot);

		LLMatrix3 scale_inverse;
		scale_inverse.setRows(LLVector3(1.0, 0.0, 0.0) / delta_scale.mV[VX],
							  LLVector3(0.0, 1.0, 0.0) / delta_scale.mV[VY],
							  LLVector3(0.0, 0.0, 1.0) / delta_scale.mV[VZ]);
							   
		
		mRelativeXformInvTrans = rot_inverse * scale_inverse;

		mRelativeXformInvTrans.transpose();
	}
	else
	{
		LLVector3 pos = getPosition();
		LLVector3 scale = getScale();
		LLQuaternion rot = getRotation();
	
		if (mParent)
		{
			pos *= mParent->getRotation();
			pos += mParent->getPosition();
			rot *= mParent->getRotation();
		}
		
		//LLViewerRegion* region = getRegion();
		//pos += region->getOriginAgent();
		
		LLVector3 x_axis = LLVector3(scale.mV[VX], 0.f, 0.f) * rot;
		LLVector3 y_axis = LLVector3(0.f, scale.mV[VY], 0.f) * rot;
		LLVector3 z_axis = LLVector3(0.f, 0.f, scale.mV[VZ]) * rot;

		mRelativeXform.initRows(LLVector4(x_axis, 0.f),
								LLVector4(y_axis, 0.f),
								LLVector4(z_axis, 0.f),
								LLVector4(pos, 1.f));

		// compute inverse transpose for normals
		LLMatrix3 rot_inverse = LLMatrix3(~rot);

		LLMatrix3 scale_inverse;
		scale_inverse.setRows(LLVector3(1.0, 0.0, 0.0) / scale.mV[VX],
							  LLVector3(0.0, 1.0, 0.0) / scale.mV[VY],
							  LLVector3(0.0, 0.0, 1.0) / scale.mV[VZ]);
							   
		
		mRelativeXformInvTrans = rot_inverse * scale_inverse;

		mRelativeXformInvTrans.transpose();
	}

    // reX
    updateOgrePosition();
}

BOOL LLVOVolume::updateGeometry(LLDrawable *drawable)
{
	LLFastTimer t(LLFastTimer::FTM_UPDATE_PRIMITIVES);
	
	if (mVolumeImpl != NULL)
	{
		BOOL res;
		{
			LLFastTimer t(LLFastTimer::FTM_GEN_FLEX);
			res = mVolumeImpl->doUpdateGeometry(drawable);
		}
		updateFaceFlags();
		return res;
	}
	
	dirtySpatialGroup();

	BOOL compiled = FALSE;
			
	updateRelativeXform();
	
	if (mDrawable.isNull()) // Not sure why this is happening, but it is...
	{
		return TRUE; // No update to complete
	}

	// reX
	//! For some reason prim texture parameter updates like rotation weren't happening
	//! in Ogre mode when parameter was 0 (done in llviewerobject.cpp), so added this
	if (mDrawable->isState(LLDrawable::REBUILD_TCOORD))
		mFaceMappingChanged = TRUE;

	if (mVolumeChanged || mFaceMappingChanged )
	{
		compiled = TRUE;

		if (mVolumeChanged)
		{
			LLFastTimer ftm(LLFastTimer::FTM_GEN_VOLUME);
			LLVolumeParams volume_params = getVolume()->getParams();
			setVolume(volume_params, 0);
			drawable->setState(LLDrawable::REBUILD_VOLUME);
		}

		{
			LLFastTimer t(LLFastTimer::FTM_GEN_TRIANGLES);
			regenFaces();
			genBBoxes(FALSE);
		}

		// reX
		markOgreUpdate();
	}
	else if ((mLODChanged) || (mSculptChanged))
	{
		LLVolume *old_volumep, *new_volumep;
		F32 old_lod, new_lod;
		S32 old_num_faces, new_num_faces ;

		old_volumep = getVolume();
		old_lod = old_volumep->getDetail();
		old_num_faces = old_volumep->getNumFaces() ;
		old_volumep = NULL ;

		{
			LLFastTimer ftm(LLFastTimer::FTM_GEN_VOLUME);
			LLVolumeParams volume_params = getVolume()->getParams();
			setVolume(volume_params, 0);
		}

		new_volumep = getVolume();
		new_lod = new_volumep->getDetail();
		new_num_faces = new_volumep->getNumFaces() ;
		new_volumep = NULL ;

		if ((new_lod != old_lod) || mSculptChanged)
		{
			compiled = TRUE;
			sNumLODChanges += new_num_faces ;
	
			drawable->setState(LLDrawable::REBUILD_VOLUME); // for face->genVolumeTriangles()
			// reX
			markOgreUpdate();

			{
				LLFastTimer t(LLFastTimer::FTM_GEN_TRIANGLES);
				if (new_num_faces != old_num_faces)
				{
					regenFaces();
				}
				genBBoxes(FALSE);
			}
		}
	}
	// it has its own drawable (it's moved) or it has changed UVs or it has changed xforms from global<->local
	else
	{
		compiled = TRUE;
		// All it did was move or we changed the texture coordinate offset
		LLFastTimer t(LLFastTimer::FTM_GEN_TRIANGLES);
		genBBoxes(FALSE);
	}

	// Update face flags
	updateFaceFlags();
	
	if(compiled)
	{
		LLPipeline::sCompiles++;
	}
	
	mVolumeChanged = FALSE;
	mLODChanged = FALSE;
	mSculptChanged = FALSE;
	mFaceMappingChanged = FALSE;

	return LLViewerObject::updateGeometry(drawable);
}

void LLVOVolume::updateFaceSize(S32 idx)
{
	LLFace* facep = mDrawable->getFace(idx);
	if (idx >= getVolume()->getNumVolumeFaces())
	{
		facep->setSize(0,0);
	}
	else
	{
		const LLVolumeFace& vol_face = getVolume()->getVolumeFace(idx);
		facep->setSize(vol_face.mVertices.size(), vol_face.mIndices.size());
	}
}

BOOL LLVOVolume::isRootEdit() const
{
	if (mParent && !((LLViewerObject*)mParent)->isAvatar())
	{
		return FALSE;
	}
	return TRUE;
}

void LLVOVolume::setTEImage(const U8 te, LLViewerImage *imagep)
{
	BOOL changed = (mTEImages[te] != imagep);
	LLViewerObject::setTEImage(te, imagep);
	if (changed)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
}

S32 LLVOVolume::setTETexture(const U8 te, const LLUUID &uuid)
{
	S32 res = LLViewerObject::setTETexture(te, uuid);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return res;
}

S32 LLVOVolume::setTEColor(const U8 te, const LLColor3& color)
{
	return setTEColor(te, LLColor4(color));
}

S32 LLVOVolume::setTEColor(const U8 te, const LLColor4& color)
{
	S32 retval = 0;
	const LLTextureEntry *tep = getTE(te);
	if (!tep)
	{
		llwarns << "No texture entry for te " << (S32)te << ", object " << mID << llendl;
	}
	else if (color != tep->getColor())
	{
		if (color.mV[3] != tep->getColor().mV[3])
		{
			gPipeline.markTextured(mDrawable);
		}
		retval = LLPrimitive::setTEColor(te, color);
		if (mDrawable.notNull() && retval)
		{
			// These should only happen on updates which are not the initial update.
			mDrawable->setState(LLDrawable::REBUILD_COLOR);
			dirtyMesh();
		}
	}

	return  retval;
}

S32 LLVOVolume::setTEBumpmap(const U8 te, const U8 bumpmap)
{
	S32 res = LLViewerObject::setTEBumpmap(te, bumpmap);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return  res;
}

S32 LLVOVolume::setTETexGen(const U8 te, const U8 texgen)
{
	S32 res = LLViewerObject::setTETexGen(te, texgen);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return  res;
}

S32 LLVOVolume::setTEMediaTexGen(const U8 te, const U8 media)
{
	S32 res = LLViewerObject::setTEMediaTexGen(te, media);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return  res;
}

S32 LLVOVolume::setTEShiny(const U8 te, const U8 shiny)
{
	S32 res = LLViewerObject::setTEShiny(te, shiny);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return  res;
}

S32 LLVOVolume::setTEFullbright(const U8 te, const U8 fullbright)
{
	S32 res = LLViewerObject::setTEFullbright(te, fullbright);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return  res;
}

S32 LLVOVolume::setTEBumpShinyFullbright(const U8 te, const U8 bump)
{
	S32 res = LLViewerObject::setTEBumpShinyFullbright(te, bump);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return res;
}

S32 LLVOVolume::setTEMediaFlags(const U8 te, const U8 media_flags)
{
	S32 res = LLViewerObject::setTEMediaFlags(te, media_flags);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return  res;
}

S32 LLVOVolume::setTEGlow(const U8 te, const F32 glow)
{
	S32 res = LLViewerObject::setTEGlow(te, glow);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return  res;
}

S32 LLVOVolume::setTEScale(const U8 te, const F32 s, const F32 t)
{
	S32 res = LLViewerObject::setTEScale(te, s, t);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return res;
}

S32 LLVOVolume::setTEScaleS(const U8 te, const F32 s)
{
	S32 res = LLViewerObject::setTEScaleS(te, s);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return res;
}

S32 LLVOVolume::setTEScaleT(const U8 te, const F32 t)
{
	S32 res = LLViewerObject::setTEScaleT(te, t);
	if (res)
	{
		gPipeline.markTextured(mDrawable);
		mFaceMappingChanged = TRUE;
	}
	return res;
}

void LLVOVolume::updateTEData()
{
	/*if (mDrawable.notNull())
	{
		mFaceMappingChanged = TRUE;
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_MATERIAL, TRUE);
	}*/
}

//----------------------------------------------------------------------------

void LLVOVolume::setIsLight(BOOL is_light)
{
	if (is_light != getIsLight())
	{
		if (is_light)
		{
			setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT, TRUE, true);
		}
		else
		{
			setParameterEntryInUse(LLNetworkData::PARAMS_LIGHT, FALSE, true);
		}

		if (is_light)
		{
			// Add it to the pipeline mLightSet
			gPipeline.setLight(mDrawable, TRUE);
		}
		else
		{
			// Not a light.  Remove it from the pipeline's light set.
			gPipeline.setLight(mDrawable, FALSE);
		}
	}

	// reX
	updateOgreLight();
}

void LLVOVolume::setLightColor(const LLColor3& color)
{
	LLLightParams *param_block = (LLLightParams *)getParameterEntry(LLNetworkData::PARAMS_LIGHT);
	if (param_block)
	{
		if (param_block->getColor() != color)
		{
			param_block->setColor(LLColor4(color, param_block->getColor().mV[3]));
			parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
			gPipeline.markTextured(mDrawable);
			mFaceMappingChanged = TRUE;
		}
	}
}

void LLVOVolume::setLightIntensity(F32 intensity)
{
	LLLightParams *param_block = (LLLightParams *)getParameterEntry(LLNetworkData::PARAMS_LIGHT);
	if (param_block)
	{
		if (param_block->getColor().mV[3] != intensity)
		{
			param_block->setColor(LLColor4(LLColor3(param_block->getColor()), intensity));
			parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
		}
	}
}

void LLVOVolume::setLightRadius(F32 radius)
{
	LLLightParams *param_block = (LLLightParams *)getParameterEntry(LLNetworkData::PARAMS_LIGHT);
	if (param_block)
	{
		if (param_block->getRadius() != radius)
		{
			param_block->setRadius(radius);
			parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
		}
	}
}

void LLVOVolume::setLightFalloff(F32 falloff)
{
	LLLightParams *param_block = (LLLightParams *)getParameterEntry(LLNetworkData::PARAMS_LIGHT);
	if (param_block)
	{
		if (param_block->getFalloff() != falloff)
		{
			param_block->setFalloff(falloff);
			parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
		}
	}
}

void LLVOVolume::setLightCutoff(F32 cutoff)
{
	LLLightParams *param_block = (LLLightParams *)getParameterEntry(LLNetworkData::PARAMS_LIGHT);
	if (param_block)
	{
		if (param_block->getCutoff() != cutoff)
		{
			param_block->setCutoff(cutoff);
			parameterChanged(LLNetworkData::PARAMS_LIGHT, true);
		}
	}
}

//----------------------------------------------------------------------------

BOOL LLVOVolume::getIsLight() const
{
	return getParameterEntryInUse(LLNetworkData::PARAMS_LIGHT);
}

LLColor3 LLVOVolume::getLightBaseColor() const
{
	const LLLightParams *param_block = (const LLLightParams *)getParameterEntry(LLNetworkData::PARAMS_LIGHT);
	if (param_block)
	{
		return LLColor3(param_block->getColor());
	}
	else
	{
		return LLColor3(1,1,1);
	}
}

LLColor3 LLVOVolume::getLightColor() const
{
	const LLLightParams *param_block = (const LLLightParams *)getParameterEntry(LLNetworkData::PARAMS_LIGHT);
	if (param_block)
	{
		return LLColor3(param_block->getColor()) * param_block->getColor().mV[3];
	}
	else
	{
		return LLColor3(1,1,1);
	}
}

F32 LLVOVolume::getLightIntensity() const
{
	const LLLightParams *param_block = (const LLLightParams *)getParameterEntry(LLNetworkData::PARAMS_LIGHT);
	if (param_block)
	{
		return param_block->getColor().mV[3];
	}
	else
	{
		return 1.f;
	}
}

F32 LLVOVolume::getLightRadius() const
{
	const LLLightParams *param_block = (const LLLightParams *)getParameterEntry(LLNetworkData::PARAMS_LIGHT);
	if (param_block)
	{
		return param_block->getRadius();
	}
	else
	{
		return 0.f;
	}
}

F32 LLVOVolume::getLightFalloff() const
{
	const LLLightParams *param_block = (const LLLightParams *)getParameterEntry(LLNetworkData::PARAMS_LIGHT);
	if (param_block)
	{
		return param_block->getFalloff();
	}
	else
	{
		return 0.f;
	}
}

F32 LLVOVolume::getLightCutoff() const
{
	const LLLightParams *param_block = (const LLLightParams *)getParameterEntry(LLNetworkData::PARAMS_LIGHT);
	if (param_block)
	{
		return param_block->getCutoff();
	}
	else
	{
		return 0.f;
	}
}

U32 LLVOVolume::getVolumeInterfaceID() const
{
	if (mVolumeImpl)
	{
		return mVolumeImpl->getID();
	}

	return 0;
}

BOOL LLVOVolume::isFlexible() const
{
	if (getParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE))
	{
		LLVolume* volume = getVolume();
		if (volume && volume->getParams().getPathParams().getCurveType() != LL_PCODE_PATH_FLEXIBLE)
		{
			LLVolumeParams volume_params = getVolume()->getParams();
			U8 profile_and_hole = volume_params.getProfileParams().getCurveType();
			volume_params.setType(profile_and_hole, LL_PCODE_PATH_FLEXIBLE);
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL LLVOVolume::isSculpted() const
{
	if (getParameterEntryInUse(LLNetworkData::PARAMS_SCULPT))
	{
		return TRUE;
	}
	
	return FALSE;
}

BOOL LLVOVolume::isVolumeGlobal() const
{
	if (mVolumeImpl)
	{
		return mVolumeImpl->isVolumeGlobal() ? TRUE : FALSE;
	}
	return FALSE;
}

BOOL LLVOVolume::canBeFlexible() const
{
	U8 path = getVolume()->getParams().getPathParams().getCurveType();
	return (path == LL_PCODE_PATH_FLEXIBLE || path == LL_PCODE_PATH_LINE);
}

BOOL LLVOVolume::setIsFlexible(BOOL is_flexible)
{
	BOOL res = FALSE;
	BOOL was_flexible = isFlexible();
	LLVolumeParams volume_params;
	if (is_flexible)
	{
		if (!was_flexible)
		{
			volume_params = getVolume()->getParams();
			U8 profile_and_hole = volume_params.getProfileParams().getCurveType();
			volume_params.setType(profile_and_hole, LL_PCODE_PATH_FLEXIBLE);
			res = TRUE;
			setFlags(FLAGS_USE_PHYSICS, FALSE);
			setFlags(FLAGS_PHANTOM, TRUE);
			setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, TRUE, true);
			if (mDrawable)
			{
				mDrawable->makeActive();
			}
		}
	}
	else
	{
		if (was_flexible)
		{
			volume_params = getVolume()->getParams();
			U8 profile_and_hole = volume_params.getProfileParams().getCurveType();
			volume_params.setType(profile_and_hole, LL_PCODE_PATH_LINE);
			res = TRUE;
			setFlags(FLAGS_PHANTOM, FALSE);
			setParameterEntryInUse(LLNetworkData::PARAMS_FLEXIBLE, FALSE, true);
		}
	}
	if (res)
	{
		res = setVolume(volume_params, 1);
		if (res)
		{
			markForUpdate(TRUE);
		}
	}
	return res;
}

//----------------------------------------------------------------------------

void LLVOVolume::generateSilhouette(LLSelectNode* nodep, const LLVector3& view_point)
{
	LLVolume *volume = getVolume();

	if (volume)
	{
		LLVector3 view_vector;
		view_vector = view_point; 

		//transform view vector into volume space
		view_vector -= getRenderPosition();
		mDrawable->mDistanceWRTCamera = view_vector.length();
		LLQuaternion worldRot = getRenderRotation();
		view_vector = view_vector * ~worldRot;
		if (!isVolumeGlobal())
		{
			LLVector3 objScale = getScale();
			LLVector3 invObjScale(1.f / objScale.mV[VX], 1.f / objScale.mV[VY], 1.f / objScale.mV[VZ]);
			view_vector.scaleVec(invObjScale);
		}
		
		updateRelativeXform();
		LLMatrix4 trans_mat = mRelativeXform;
		if (mDrawable->isStatic())
		{
			trans_mat.translate(getRegion()->getOriginAgent());
		}

		volume->generateSilhouetteVertices(nodep->mSilhouetteVertices, nodep->mSilhouetteNormals, nodep->mSilhouetteSegments, view_vector, trans_mat, mRelativeXformInvTrans);

		nodep->mSilhouetteExists = TRUE;
	}
}

void LLVOVolume::deleteFaces()
{
	S32 face_count = mNumFaces;
	if (mDrawable.notNull())
	{
		mDrawable->deleteFaces(0, face_count);
	}

	mNumFaces = 0;
}

void LLVOVolume::updateRadius()
{
	if (mDrawable.isNull())
	{
		return;
	}
	
	mVObjRadius = getScale().length();
	mDrawable->setRadius(mVObjRadius);
}


BOOL LLVOVolume::isAttachment() const
{
	if (mState == 0)
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

BOOL LLVOVolume::isHUDAttachment() const
{
	// *NOTE: we assume hud attachment points are in defined range
	// since this range is constant for backwards compatibility
	// reasons this is probably a reasonable assumption to make
	S32 attachment_id = ATTACHMENT_ID_FROM_STATE(mState);
	return ( attachment_id >= 31 && attachment_id <= 38 );
}


const LLMatrix4 LLVOVolume::getRenderMatrix() const
{
	if (mDrawable->isActive() && !mDrawable->isRoot())
	{
		return mDrawable->getParent()->getWorldMatrix();
	}
	return mDrawable->getWorldMatrix();
}

//static
void LLVOVolume::preUpdateGeom()
{
	sNumLODChanges = 0;
}

void LLVOVolume::parameterChanged(U16 param_type, bool local_origin)
{
	LLViewerObject::parameterChanged(param_type, local_origin);
}

void LLVOVolume::parameterChanged(U16 param_type, LLNetworkData* data, BOOL in_use, bool local_origin)
{
	LLViewerObject::parameterChanged(param_type, data, in_use, local_origin);
	if (mVolumeImpl)
	{
		mVolumeImpl->onParameterChanged(param_type, data, in_use, local_origin);
	}
	if (mDrawable.notNull())
	{
		BOOL is_light = getIsLight();
		if (is_light != mDrawable->isState(LLDrawable::LIGHT))
		{
			gPipeline.setLight(mDrawable, is_light);

			// reX
			if (mOgreObject) mOgreObject->setLight(is_light);
		}
    }
	
	// reX, rebuild RexPrimData from the deprecated PARAMS_REX block, if RexPrimData not received yet
    if ((param_type == LLNetworkData::PARAMS_REX) && (!local_origin) && (!mRexPrimDataReceived))
    {
        LLRexParams* rexParams = (LLRexParams*)data;
        mRexPrimData.setFromRexParams(*rexParams);
    }

	if ((param_type == LLNetworkData::PARAMS_LIGHT) || (param_type == LLNetworkData::PARAMS_REX))
		updateOgreLight();

    if ((param_type == LLNetworkData::PARAMS_REX) && (!mRexPrimDataReceived))
    {
        onRexPrimDataChanged(local_origin);
    }
}

void LLVOVolume::onRexPrimDataChanged(bool local_origin)
{
    updateOgreLight();
    updateOgreParticles();
    updateOgreSkeleton();

    // If RexPrimData changes with non local origin, rebuild geometry
    // (however not all changes are rendering related, this wastes some rebuilds)
    if (!local_origin)
        markOgreUpdate();
}

void LLVOVolume::setSelected(BOOL sel)
{
	LLViewerObject::setSelected(sel);
	if (mDrawable.notNull())
	{
		markForUpdate(TRUE);
	}
}

void LLVOVolume::updateSpatialExtents(LLVector3& newMin, LLVector3& newMax)
{		
}

F32 LLVOVolume::getBinRadius()
{
	F32 radius;
	
	const LLVector3* ext = mDrawable->getSpatialExtents();
	
	BOOL shrink_wrap = mDrawable->isAnimating();
	BOOL alpha_wrap = FALSE;

	if (!isHUDAttachment())
	{
		for (S32 i = 0; i < mDrawable->getNumFaces(); i++)
		{
			LLFace* face = mDrawable->getFace(i);
			if (face->getPoolType() == LLDrawPool::POOL_ALPHA &&
				(!LLPipeline::sFastAlpha || 
				face->getFaceColor().mV[3] != 1.f ||
				!face->getTexture()->getIsAlphaMask()))
			{
				alpha_wrap = TRUE;
				break;
			}
		}
	}
	else
	{
		shrink_wrap = FALSE;
	}

	if (alpha_wrap)
	{
		LLVector3 bounds = getScale();
		radius = llmin(bounds.mV[1], bounds.mV[2]);
		radius = llmin(radius, bounds.mV[0]);
		radius *= 0.5f;
	}
	else if (shrink_wrap)
	{
		radius = (ext[1]-ext[0]).length()*0.5f;
	}
	else if (mDrawable->isStatic())
	{
		/*if (mDrawable->getRadius() < 2.0f)
		{
			radius = 16.f;
		}
		else
		{
			radius = llmax(mDrawable->getRadius(), 32.f);
		}*/

		radius = (((S32) mDrawable->getRadius())/2+1)*8;
	}
	else if (mDrawable->getVObj()->isAttachment())
	{
		radius = (((S32) (mDrawable->getRadius()*4)+1))*2;
	}
	else
	{
		radius = 8.f;
	}

	return llclamp(radius, 0.5f, 256.f);
}

const LLVector3 LLVOVolume::getPivotPositionAgent() const
{
	if (mVolumeImpl)
	{
		return mVolumeImpl->getPivotPosition();
	}
	return LLViewerObject::getPivotPositionAgent();
}

void LLVOVolume::onShift(const LLVector3 &shift_vector)
{
	if (mVolumeImpl)
	{
		mVolumeImpl->onShift(shift_vector);
	}

	updateRelativeXform();
}

const LLMatrix4& LLVOVolume::getWorldMatrix(LLXformMatrix* xform) const
{
	if (mVolumeImpl)
	{
		return mVolumeImpl->getWorldMatrix(xform);
	}
	return xform->getWorldMatrix();
}

LLVector3 LLVOVolume::agentPositionToVolume(const LLVector3& pos) const
{
	LLVector3 ret = pos - getRenderPosition();
	ret = ret * ~getRenderRotation();
	LLVector3 objScale = isVolumeGlobal() ? LLVector3(1,1,1) : getScale();
	LLVector3 invObjScale(1.f / objScale.mV[VX], 1.f / objScale.mV[VY], 1.f / objScale.mV[VZ]);
	ret.scaleVec(invObjScale);
	
	return ret;
}

LLVector3 LLVOVolume::agentDirectionToVolume(const LLVector3& dir) const
{
	LLVector3 ret = dir * ~getRenderRotation();
	
	LLVector3 objScale = isVolumeGlobal() ? LLVector3(1,1,1) : getScale();
	ret.scaleVec(objScale);

	return ret;
}

LLVector3 LLVOVolume::volumePositionToAgent(const LLVector3& dir) const
{
	LLVector3 ret = dir;
	LLVector3 objScale = isVolumeGlobal() ? LLVector3(1,1,1) : getScale();
	ret.scaleVec(objScale);
	ret = ret * getRenderRotation();
	ret += getRenderPosition();
	
	return ret;
}

LLVector3 LLVOVolume::volumeDirectionToAgent(const LLVector3& dir) const
{
	LLVector3 ret = dir;
	LLVector3 objScale = isVolumeGlobal() ? LLVector3(1,1,1) : getScale();
	LLVector3 invObjScale(1.f / objScale.mV[VX], 1.f / objScale.mV[VY], 1.f / objScale.mV[VZ]);
	ret.scaleVec(invObjScale);
	ret = ret * getRenderRotation();

	return ret;
}


BOOL LLVOVolume::lineSegmentIntersect(const LLVector3& start, const LLVector3& end, S32 face, BOOL pick_transparent, S32 *face_hitp,
									  LLVector3* intersection,LLVector2* tex_coord, LLVector3* normal, LLVector3* bi_normal)
	
{
	if (!mbCanSelect ||
//		(gHideSelectedObjects && isSelected()) ||
// [RLVa:KB] - Checked: 2009-10-10 (RLVa-1.0.5a) | Modified: RLVa-1.0.5a
		( (gHideSelectedObjects && isSelected()) && 
		  ((!rlv_handler_t::isEnabled()) || (!isHUDAttachment()) || (!gRlvHandler.isLockedAttachment(this, RLV_LOCK_REMOVE))) ) ||
// [/RLVa:KB]
			mDrawable->isDead() || 
			!gPipeline.hasRenderType(mDrawable->getRenderType()))
	{
		return FALSE;
	}

	BOOL ret = FALSE;

	LLVolume* volume = getVolume();
	if (volume)
	{	
		LLVector3 v_start, v_end, v_dir;
	
		v_start = agentPositionToVolume(start);
		v_end = agentPositionToVolume(end);
		
		LLVector3 p;
		LLVector3 n;
		LLVector2 tc;
		LLVector3 bn;

		if (intersection != NULL)
		{
			p = *intersection;
		}

		if (tex_coord != NULL)
		{
			tc = *tex_coord;
		}

		if (normal != NULL)
		{
			n = *normal;
		}

		if (bi_normal != NULL)
		{
			bn = *bi_normal;
		}

		S32 face_hit = -1;

		S32 start_face, end_face;
		if (face == -1)
		{
			start_face = 0;
			end_face = volume->getNumFaces();
		}
		else
		{
			start_face = face;
			end_face = face+1;
		}

		for (S32 i = start_face; i < end_face; ++i)
		{
			face_hit = volume->lineSegmentIntersect(v_start, v_end, i,
													&p, &tc, &n, &bn);
			
			if (face_hit >= 0 && mDrawable->getNumFaces() > face_hit)
			{
				LLFace* face = mDrawable->getFace(face_hit);
			
				if (pick_transparent || !face->getTexture() || face->getTexture()->getMask(face->surfaceToTexture(tc, p, n)))
				{
					v_end = p;
					if (face_hitp != NULL)
					{
						*face_hitp = face_hit;
					}
					
					if (intersection != NULL)
					{
						*intersection = volumePositionToAgent(p);  // must map back to agent space
					}

					if (normal != NULL)
					{
						*normal = volumeDirectionToAgent(n);
						(*normal).normVec();
					}

					if (bi_normal != NULL)
					{
						*bi_normal = volumeDirectionToAgent(bn);
						(*bi_normal).normVec();
					}

					if (tex_coord != NULL)
					{
						*tex_coord = tc;
					}
					
					ret = TRUE;
				}
			}
		}
	}
		
	return ret;
}

U32 LLVOVolume::getPartitionType() const
{
	if (isHUDAttachment())
	{
		return LLViewerRegion::PARTITION_HUD;
	}

	return LLViewerRegion::PARTITION_VOLUME;
}

LLVolumePartition::LLVolumePartition()
: LLSpatialPartition(LLVOVolume::VERTEX_DATA_MASK, FALSE)
{
	mLODPeriod = 16;
	mDepthMask = FALSE;
	mDrawableType = LLPipeline::RENDER_TYPE_VOLUME;
	mPartitionType = LLViewerRegion::PARTITION_VOLUME;
	mSlopRatio = 0.25f;
	mBufferUsage = GL_DYNAMIC_DRAW_ARB;
}

LLVolumeBridge::LLVolumeBridge(LLDrawable* drawablep)
: LLSpatialBridge(drawablep, LLVOVolume::VERTEX_DATA_MASK)
{
	mDepthMask = FALSE;
	mLODPeriod = 16;
	mDrawableType = LLPipeline::RENDER_TYPE_VOLUME;
	mPartitionType = LLViewerRegion::PARTITION_BRIDGE;
	
	mBufferUsage = GL_DYNAMIC_DRAW_ARB;

	mSlopRatio = 0.25f;
}

void LLVolumeGeometryManager::registerFace(LLSpatialGroup* group, LLFace* facep, U32 type)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);

//	if (facep->getViewerObject()->isSelected() && gHideSelectedObjects)
//	{
//		return;
//	}
// [RLVa:KB] - Checked: 2009-10-10 (RLVa-1.0.5a) | Modified: RLVa-1.0.5a
	LLViewerObject* pObj = facep->getViewerObject();
	if ( (pObj->isSelected() && gHideSelectedObjects) && 
		 ((!rlv_handler_t::isEnabled()) || (!pObj->isHUDAttachment()) || (!gRlvHandler.isLockedAttachment(pObj, RLV_LOCK_REMOVE))) )
	{
		return;
	}
// [/RVLa:KB]

	//add face to drawmap
	LLSpatialGroup::drawmap_elem_t& draw_vec = group->mDrawMap[type];	

	S32 idx = draw_vec.size()-1;


	BOOL fullbright = (type == LLRenderPass::PASS_FULLBRIGHT ||
					  type == LLRenderPass::PASS_ALPHA) ? facep->isState(LLFace::FULLBRIGHT) : FALSE;

	const LLMatrix4* tex_mat = NULL;
	if (facep->isState(LLFace::TEXTURE_ANIM) && facep->getVirtualSize() > MIN_TEX_ANIM_SIZE)
	{
		tex_mat = facep->mTextureMatrix;	
	}

	const LLMatrix4* model_mat = NULL;

	LLDrawable* drawable = facep->getDrawable();
	if (drawable->isActive())
	{
		model_mat = &(drawable->getRenderMatrix());
	}
	else
	{
		model_mat = &(drawable->getRegion()->mRenderMatrix);
	}

	U8 bump = (type == LLRenderPass::PASS_BUMP ? facep->getTextureEntry()->getBumpmap() : 0);
	
	LLViewerImage* tex = facep->getTexture();

	U8 glow = 0;
		
	if (type == LLRenderPass::PASS_GLOW)
	{
		glow = (U8) (facep->getTextureEntry()->getGlow() * 255);
	}

	if (facep->mVertexBuffer.isNull())
	{
		llerrs << "WTF?" << llendl;
	}

	if (idx >= 0 && 
		draw_vec[idx]->mVertexBuffer == facep->mVertexBuffer &&
		draw_vec[idx]->mEnd == facep->getGeomIndex()-1 &&
		(LLPipeline::sTextureBindTest || draw_vec[idx]->mTexture == tex) &&
#if LL_DARWIN
		draw_vec[idx]->mEnd - draw_vec[idx]->mStart + facep->getGeomCount() <= (U32) gGLManager.mGLMaxVertexRange &&
		draw_vec[idx]->mCount + facep->getIndicesCount() <= (U32) gGLManager.mGLMaxIndexRange &&
#endif
		draw_vec[idx]->mGlowColor.mV[3] == glow &&
		draw_vec[idx]->mFullbright == fullbright &&
		draw_vec[idx]->mBump == bump &&
		draw_vec[idx]->mTextureMatrix == tex_mat &&
		draw_vec[idx]->mModelMatrix == model_mat)
	{
		draw_vec[idx]->mCount += facep->getIndicesCount();
		draw_vec[idx]->mEnd += facep->getGeomCount();
		draw_vec[idx]->mVSize = llmax(draw_vec[idx]->mVSize, facep->getVirtualSize());
		validate_draw_info(*draw_vec[idx]);
		update_min_max(draw_vec[idx]->mExtents[0], draw_vec[idx]->mExtents[1], facep->mExtents[0]);
		update_min_max(draw_vec[idx]->mExtents[0], draw_vec[idx]->mExtents[1], facep->mExtents[1]);
	}
	else
	{
		U32 start = facep->getGeomIndex();
		U32 end = start + facep->getGeomCount()-1;
		U32 offset = facep->getIndicesStart();
		U32 count = facep->getIndicesCount();
		LLPointer<LLDrawInfo> draw_info = new LLDrawInfo(start,end,count,offset,tex, 
			facep->mVertexBuffer, fullbright, bump); 
		draw_info->mGroup = group;
		draw_info->mVSize = facep->getVirtualSize();
		draw_vec.push_back(draw_info);
		draw_info->mTextureMatrix = tex_mat;
		draw_info->mModelMatrix = model_mat;
		draw_info->mGlowColor.setVec(0,0,0,glow);
		if (type == LLRenderPass::PASS_ALPHA)
		{ //for alpha sorting
			facep->setDrawInfo(draw_info);
		}
		draw_info->mExtents[0] = facep->mExtents[0];
		draw_info->mExtents[1] = facep->mExtents[1];
		validate_draw_info(*draw_info);
	}
}

void LLVolumeGeometryManager::getGeometry(LLSpatialGroup* group)
{

}

void LLVolumeGeometryManager::rebuildGeom(LLSpatialGroup* group)
{
	if (LLPipeline::sSkipUpdate)
	{
		return;
	}

	if (group->changeLOD())
	{
		group->mLastUpdateDistance = group->mDistance;
	}

	group->mLastUpdateViewAngle = group->mViewAngle;

	if (!group->isState(LLSpatialGroup::GEOM_DIRTY | LLSpatialGroup::ALPHA_DIRTY))
	{
		if (group->isState(LLSpatialGroup::MESH_DIRTY) && !LLPipeline::sDelayVBUpdate)
		{
			LLFastTimer ftm(LLFastTimer::FTM_REBUILD_VBO);	
			LLFastTimer ftm2(LLFastTimer::FTM_REBUILD_VOLUME_VB);
		
			rebuildMesh(group);
		}
		return;
	}

	group->mBuilt = 1.f;
	LLFastTimer ftm(LLFastTimer::FTM_REBUILD_VBO);	

	LLFastTimer ftm2(LLFastTimer::FTM_REBUILD_VOLUME_VB);

	group->clearDrawMap();

	mFaceList.clear();

	std::vector<LLFace*> fullbright_faces;
	std::vector<LLFace*> bump_faces;
	std::vector<LLFace*> simple_faces;

	std::vector<LLFace*> alpha_faces;
	U32 useage = group->mSpatialPartition->mBufferUsage;

	U32 max_vertices = (gSavedSettings.getS32("RenderMaxVBOSize")*1024)/LLVertexBuffer::calcStride(group->mSpatialPartition->mVertexDataMask);
	U32 max_total = (gSavedSettings.getS32("RenderMaxNodeSize")*1024)/LLVertexBuffer::calcStride(group->mSpatialPartition->mVertexDataMask);
	max_vertices = llmin(max_vertices, (U32) 65535);

	U32 cur_total = 0;

	//get all the faces into a list
	for (LLSpatialGroup::element_iter drawable_iter = group->getData().begin(); drawable_iter != group->getData().end(); ++drawable_iter)
	{
		LLDrawable* drawablep = *drawable_iter;
		
		if (drawablep->isDead() || drawablep->isState(LLDrawable::FORCE_INVISIBLE) )
		{
			continue;
		}
	
		if (drawablep->isAnimating())
		{ //fall back to stream draw for animating verts
			useage = GL_STREAM_DRAW_ARB;
		}

		LLVOVolume* vobj = drawablep->getVOVolume();
		llassert_always(vobj);
		vobj->updateTextures();
		vobj->preRebuild();

		//for each face
		for (S32 i = 0; i < drawablep->getNumFaces(); i++)
		{
			//sum up face verts and indices
			drawablep->updateFaceSize(i);
			LLFace* facep = drawablep->getFace(i);

			if (cur_total > max_total)
			{
				facep->mVertexBuffer = NULL;
				facep->mLastVertexBuffer = NULL;
				continue;
			}

			cur_total += facep->getGeomCount();

			if (facep->hasGeometry() && facep->mPixelArea > FORCE_CULL_AREA)
			{
				const LLTextureEntry* te = facep->getTextureEntry();
				LLViewerImage* tex = facep->getTexture();

				if (facep->isState(LLFace::TEXTURE_ANIM))
				{
					if (!vobj->mTexAnimMode)
					{
						facep->clearState(LLFace::TEXTURE_ANIM);
					}
				}

				BOOL force_simple = (facep->mPixelArea < FORCE_SIMPLE_RENDER_AREA);
				U32 type = gPipeline.getPoolTypeFromTE(te, tex);
				if (type != LLDrawPool::POOL_ALPHA && force_simple)
				{
					type = LLDrawPool::POOL_SIMPLE;
				}
				facep->setPoolType(type);

				if (vobj->isHUDAttachment())
				{
					facep->setState(LLFace::FULLBRIGHT);
				}

				if (vobj->mTextureAnimp && vobj->mTexAnimMode)
				{
					if (vobj->mTextureAnimp->mFace <= -1)
					{
						S32 face;
						for (face = 0; face < vobj->getNumTEs(); face++)
						{
							drawablep->getFace(face)->setState(LLFace::TEXTURE_ANIM);
						}
					}
					else if (vobj->mTextureAnimp->mFace < vobj->getNumTEs())
					{
						drawablep->getFace(vobj->mTextureAnimp->mFace)->setState(LLFace::TEXTURE_ANIM);
					}
				}

				if (type == LLDrawPool::POOL_ALPHA)
				{
					if (LLPipeline::sFastAlpha &&
						(te->getColor().mV[VW] == 1.0f) &&
						facep->getTexture()->getIsAlphaMask())
					{ //can be treated as alpha mask
						simple_faces.push_back(facep);
					}
					else
					{
						alpha_faces.push_back(facep);
					}
				}
				else
				{
					if (drawablep->isState(LLDrawable::REBUILD_VOLUME))
					{
						facep->mLastUpdateTime = gFrameTimeSeconds;
					}

					if (gPipeline.canUseWindLightShadersOnObjects()
						&& LLPipeline::sRenderBump)
					{
						if (te->getBumpmap())
						{ //needs normal + binormal
							bump_faces.push_back(facep);
						}
						else if (te->getShiny() || !te->getFullbright())
						{ //needs normal
							simple_faces.push_back(facep);
						}
						else 
						{ //doesn't need normal
							facep->setState(LLFace::FULLBRIGHT);
							fullbright_faces.push_back(facep);
						}
					}
					else
					{
						if (te->getBumpmap() && LLPipeline::sRenderBump)
						{ //needs normal + binormal
							bump_faces.push_back(facep);
						}
						else if (te->getShiny() && LLPipeline::sRenderBump ||
							!te->getFullbright())
						{ //needs normal
							simple_faces.push_back(facep);
						}
						else 
						{ //doesn't need normal
							facep->setState(LLFace::FULLBRIGHT);
							fullbright_faces.push_back(facep);
						}
					}
				}
			}
			else
			{	//face has no renderable geometry
				facep->mVertexBuffer = NULL;
				facep->mLastVertexBuffer = NULL;
			}		
		}
	}

	group->mBufferUsage = useage;

	//PROCESS NON-ALPHA FACES
	U32 simple_mask = LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_COLOR;
	U32 alpha_mask = simple_mask | 0x80000000; //hack to give alpha verts their own VBO
	U32 bump_mask = LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1 | LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_COLOR;
	U32 fullbright_mask = LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_COLOR;

	if (LLPipeline::sRenderDeferred)
	{
		bump_mask |= LLVertexBuffer::MAP_BINORMAL;
	}

	genDrawInfo(group, simple_mask, simple_faces);
	genDrawInfo(group, bump_mask, bump_faces);
	genDrawInfo(group, fullbright_mask, fullbright_faces);
	genDrawInfo(group, alpha_mask, alpha_faces, TRUE);

	if (!LLPipeline::sDelayVBUpdate)
	{
		//drawables have been rebuilt, clear rebuild status
		for (LLSpatialGroup::element_iter drawable_iter = group->getData().begin(); drawable_iter != group->getData().end(); ++drawable_iter)
		{
			LLDrawable* drawablep = *drawable_iter;
			drawablep->clearState(LLDrawable::REBUILD_ALL);
		}
	}

	group->mLastUpdateTime = gFrameTimeSeconds;
	group->mBuilt = 1.f;
	group->clearState(LLSpatialGroup::GEOM_DIRTY | LLSpatialGroup::ALPHA_DIRTY);

	if (LLPipeline::sDelayVBUpdate)
	{
		group->setState(LLSpatialGroup::MESH_DIRTY);
	}

	mFaceList.clear();
}

void LLVolumeGeometryManager::rebuildMesh(LLSpatialGroup* group)
{
	if (group->isState(LLSpatialGroup::MESH_DIRTY))
	{
		S32 num_mapped_veretx_buffer = LLVertexBuffer::sMappedCount ;

		group->mBuilt = 1.f;
		
		for (LLSpatialGroup::element_iter drawable_iter = group->getData().begin(); drawable_iter != group->getData().end(); ++drawable_iter)
		{
			LLDrawable* drawablep = *drawable_iter;

			if (drawablep->isDead() || drawablep->isState(LLDrawable::FORCE_INVISIBLE) )
			{
				continue;
			}

			if (drawablep->isState(LLDrawable::REBUILD_ALL))
			{
				LLVOVolume* vobj = drawablep->getVOVolume();
				vobj->preRebuild();
				LLVolume* volume = vobj->getVolume();
				for (S32 i = 0; i < drawablep->getNumFaces(); ++i)
				{
					LLFace* face = drawablep->getFace(i);
					if (face && face->mVertexBuffer.notNull())
					{
						face->getGeometryVolume(*volume, face->getTEOffset(), 
							vobj->getRelativeXform(), vobj->getRelativeXformInvTrans(), face->getGeomIndex());
					}
				}

				drawablep->clearState(LLDrawable::REBUILD_ALL);
			}
		}
		
		//unmap all the buffers
		for (LLSpatialGroup::buffer_map_t::iterator i = group->mBufferMap.begin(); i != group->mBufferMap.end(); ++i)
		{
			LLSpatialGroup::buffer_texture_map_t& map = i->second;
			for (LLSpatialGroup::buffer_texture_map_t::iterator j = map.begin(); j != map.end(); ++j)
			{
				LLSpatialGroup::buffer_list_t& list = j->second;
				for (LLSpatialGroup::buffer_list_t::iterator k = list.begin(); k != list.end(); ++k)
				{
					LLVertexBuffer* buffer = *k;
					if (buffer->isLocked())
					{
						buffer->setBuffer(0);
					}
				}
			}
		}
		
		// don't forget alpha
		if(	group != NULL && 
			!group->mVertexBuffer.isNull() && 
			group->mVertexBuffer->isLocked())
		{
			group->mVertexBuffer->setBuffer(0);
		}

		//if not all buffers are unmapped
		if(num_mapped_veretx_buffer != LLVertexBuffer::sMappedCount) 
		{
			llwarns << "Not all mapped vertex buffers are unmapped!" << llendl ; 
			for (LLSpatialGroup::element_iter drawable_iter = group->getData().begin(); drawable_iter != group->getData().end(); ++drawable_iter)
			{
				LLDrawable* drawablep = *drawable_iter;
				for (S32 i = 0; i < drawablep->getNumFaces(); ++i)
				{
					LLFace* face = drawablep->getFace(i);
					if (face && face->mVertexBuffer.notNull() && face->mVertexBuffer->isLocked())
					{
						face->mVertexBuffer->setBuffer(0) ;
					}
				}
			} 
		}

		group->clearState(LLSpatialGroup::MESH_DIRTY);
	}
}

void LLVolumeGeometryManager::genDrawInfo(LLSpatialGroup* group, U32 mask, std::vector<LLFace*>& faces, BOOL distance_sort)
{
	//calculate maximum number of vertices to store in a single buffer
	U32 max_vertices = (gSavedSettings.getS32("RenderMaxVBOSize")*1024)/LLVertexBuffer::calcStride(group->mSpatialPartition->mVertexDataMask);
	max_vertices = llmin(max_vertices, (U32) 65535);

	if (!distance_sort)
	{
		//sort faces by things that break batches
		std::sort(faces.begin(), faces.end(), LLFace::CompareBatchBreaker());
	}
	else
	{
		//sort faces by distance
		std::sort(faces.begin(), faces.end(), LLFace::CompareDistanceGreater());
	}
				
	std::vector<LLFace*>::iterator face_iter = faces.begin();
	
	LLSpatialGroup::buffer_map_t buffer_map;

	LLViewerImage* last_tex = NULL;
	S32 buffer_index = 0;

	if (distance_sort)
	{
		buffer_index = -1;
	}

	while (face_iter != faces.end())
	{
		//pull off next face
		LLFace* facep = *face_iter;
		LLViewerImage* tex = facep->getTexture();

		if (distance_sort)
		{
			tex = NULL;
		}

		if (last_tex == tex)
		{
			buffer_index++;
		}
		else
		{
			last_tex = tex;
			buffer_index = 0;
		}

		U32 index_count = facep->getIndicesCount();
		U32 geom_count = facep->getGeomCount();

		//sum up vertices needed for this texture
		std::vector<LLFace*>::iterator i = face_iter;
		++i;
		
		while (i != faces.end() && 
			(LLPipeline::sTextureBindTest || (distance_sort || (*i)->getTexture() == tex)))
		{
			facep = *i;
			
			if (geom_count + facep->getGeomCount() > max_vertices)
			{ //cut vertex buffers on geom count too big
				break;
			}

			++i;
			index_count += facep->getIndicesCount();
			geom_count += facep->getGeomCount();
		}
	
		//create/delete/resize vertex buffer if needed
		LLVertexBuffer* buffer = NULL;
		LLSpatialGroup::buffer_texture_map_t::iterator found_iter = group->mBufferMap[mask].find(tex);
		
		if (found_iter != group->mBufferMap[mask].end())
		{
			if ((U32) buffer_index < found_iter->second.size())
			{
				buffer = found_iter->second[buffer_index];
			}
		}
						
		if (!buffer)
		{ //create new buffer if needed
			buffer = createVertexBuffer(mask, 
											group->mBufferUsage);
			buffer->allocateBuffer(geom_count, index_count, TRUE);
		}
		else 
		{
			if (LLVertexBuffer::sEnableVBOs && buffer->getUsage() != group->mBufferUsage)
			{
				buffer = createVertexBuffer(group->mSpatialPartition->mVertexDataMask, 
											group->mBufferUsage);
				buffer->allocateBuffer(geom_count, index_count, TRUE);
			}
			else
			{
				buffer->resizeBuffer(geom_count, index_count);
			}
		}

		buffer_map[mask][tex].push_back(buffer);

		//add face geometry

		U32 indices_index = 0;
		U16 index_offset = 0;

		while (face_iter < i)
		{
			facep = *face_iter;
			facep->mIndicesIndex = indices_index;
			facep->mGeomIndex = index_offset;
			facep->mVertexBuffer = buffer;
			{
				facep->updateRebuildFlags();
				if (!LLPipeline::sDelayVBUpdate)
				{
					LLDrawable* drawablep = facep->getDrawable();
					LLVOVolume* vobj = drawablep->getVOVolume();
					LLVolume* volume = vobj->getVolume();

					U32 te_idx = facep->getTEOffset();

					if (facep->getGeometryVolume(*volume, te_idx, 
						vobj->getRelativeXform(), vobj->getRelativeXformInvTrans(), index_offset))
					{
						buffer->markDirty(facep->getGeomIndex(), facep->getGeomCount(), 
							facep->getIndicesStart(), facep->getIndicesCount());
					}
				}
			}

			index_offset += facep->getGeomCount();
			indices_index += facep->mIndicesCount;

			BOOL force_simple = facep->mPixelArea < FORCE_SIMPLE_RENDER_AREA;
			BOOL fullbright = facep->isState(LLFace::FULLBRIGHT);
			const LLTextureEntry* te = facep->getTextureEntry();

			BOOL is_alpha = facep->getPoolType() == LLDrawPool::POOL_ALPHA ? TRUE : FALSE;
		
			if (is_alpha)
			{
				// can we safely treat this as an alpha mask?
				if (LLPipeline::sFastAlpha &&
				    (te->getColor().mV[VW] == 1.0f) &&
				    facep->getTexture()->getIsAlphaMask())
				{
					if (te->getFullbright())
					{
						registerFace(group, facep, LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK);
					}
					else
					{
						registerFace(group, facep, LLRenderPass::PASS_ALPHA_MASK);
					}
				}
				else
				{
					registerFace(group, facep, LLRenderPass::PASS_ALPHA);
				}

				if (LLPipeline::sRenderDeferred)
				{
					registerFace(group, facep, LLRenderPass::PASS_ALPHA_SHADOW);
				}
			}
			else if (gPipeline.canUseVertexShaders()
				&& LLPipeline::sRenderBump 
				&& te->getShiny())
			{
				if (tex->getPrimaryFormat() == GL_ALPHA)
				{
					registerFace(group, facep, LLRenderPass::PASS_INVISI_SHINY);
					registerFace(group, facep, LLRenderPass::PASS_INVISIBLE);
				}
				else if (LLPipeline::sRenderDeferred)
				{
					if (te->getBumpmap())
					{
						registerFace(group, facep, LLRenderPass::PASS_BUMP);
					}
					else if (te->getFullbright())
					{
						registerFace(group, facep, LLRenderPass::PASS_FULLBRIGHT_SHINY);
					}
					else
					{
						llassert(mask & LLVertexBuffer::MAP_NORMAL);
						registerFace(group, facep, LLRenderPass::PASS_SIMPLE);
					}
				}
				else if (fullbright)
				{						
					registerFace(group, facep, LLRenderPass::PASS_FULLBRIGHT_SHINY);
				}
				else
				{
					registerFace(group, facep, LLRenderPass::PASS_SHINY);
				}
			}
			else
			{
				if (!is_alpha && tex->getPrimaryFormat() == GL_ALPHA)
				{
					registerFace(group, facep, LLRenderPass::PASS_INVISIBLE);
				}
				else if (fullbright)
				{
					registerFace(group, facep, LLRenderPass::PASS_FULLBRIGHT);
				}
				else
				{
					if (LLPipeline::sRenderDeferred && te->getBumpmap())
					{
						registerFace(group, facep, LLRenderPass::PASS_BUMP);
					}
					else
					{
						llassert(mask & LLVertexBuffer::MAP_NORMAL);
						registerFace(group, facep, LLRenderPass::PASS_SIMPLE);
					}
				}
				
				if (!is_alpha && te->getShiny())
				{
					registerFace(group, facep, LLRenderPass::PASS_SHINY);
				}
			}
			
			if (!is_alpha && !LLPipeline::sRenderDeferred)
			{
				llassert((mask & LLVertexBuffer::MAP_NORMAL) || fullbright);
				facep->setPoolType((fullbright) ? LLDrawPool::POOL_FULLBRIGHT : LLDrawPool::POOL_SIMPLE);
				
				if (!force_simple && te->getBumpmap())
				{
					registerFace(group, facep, LLRenderPass::PASS_BUMP);
				}
			}

			if (LLPipeline::sRenderGlow && te->getGlow() > 0.f)
			{
				registerFace(group, facep, LLRenderPass::PASS_GLOW);
			}
						
			++face_iter;
		}

		buffer->setBuffer(0);
	}

	group->mBufferMap[mask].clear();
	for (LLSpatialGroup::buffer_texture_map_t::iterator i = buffer_map[mask].begin(); i != buffer_map[mask].end(); ++i)
	{
		group->mBufferMap[mask][i->first] = i->second;
	}
}

void LLGeometryManager::addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32 &index_count)
{	
	//initialize to default usage for this partition
	U32 usage = group->mSpatialPartition->mBufferUsage;
	
	//clear off any old faces
	mFaceList.clear();

	//for each drawable
	for (LLSpatialGroup::element_iter drawable_iter = group->getData().begin(); drawable_iter != group->getData().end(); ++drawable_iter)
	{
		LLDrawable* drawablep = *drawable_iter;
		
		if (drawablep->isDead())
		{
			continue;
		}
	
		if (drawablep->isAnimating())
		{ //fall back to stream draw for animating verts
			usage = GL_STREAM_DRAW_ARB;
		}

		//for each face
		for (S32 i = 0; i < drawablep->getNumFaces(); i++)
		{
			//sum up face verts and indices
			drawablep->updateFaceSize(i);
			LLFace* facep = drawablep->getFace(i);
			if (facep->hasGeometry() && facep->mPixelArea > FORCE_CULL_AREA)
			{
				vertex_count += facep->getGeomCount();
				index_count += facep->getIndicesCount();

				//remember face (for sorting)
				mFaceList.push_back(facep);
			}
			else
			{
				facep->mVertexBuffer = NULL;
				facep->mLastVertexBuffer = NULL;
			}
		}
	}
	
	group->mBufferUsage = usage;
}

LLHUDPartition::LLHUDPartition()
{
	mPartitionType = LLViewerRegion::PARTITION_HUD;
	mDrawableType = LLPipeline::RENDER_TYPE_HUD;
	mSlopRatio = 0.f;
	mLODPeriod = 1;
}

void LLHUDPartition::shift(const LLVector3 &offset)
{
	//HUD objects don't shift with region crossing.  That would be silly.
}

// reX: new function
// Update Ogre object position/orientation/scale/visibility
void LLVOVolume::updateOgrePosition()
{
	if (mOgreObject && (!mDrawable.isNull()))
	{
		LLVector3 pos = getRenderPosition();
		//LLQuaternion rot = mDrawable->getWorldRotation();
        LLQuaternion rot = getRenderRotation(); // Doing this instead gets rid of linked prim rotation bug

		mOgreObject->setPosition(pos);

		if (!getRexIsMesh() && !getRexIsBillboard())
		{
			bool visible = true;

			// Check for prim forced invisible
			if (mDrawable.notNull())
			{
				if (mDrawable->isState(LLDrawable::INVISIBLE|LLDrawable::FORCE_INVISIBLE))
					visible = false;
			}

			if (!getRexIsVisible())
				visible = false;

			mOgreObject->setVisible(visible);
			mOgreObject->setOrientation(rot);
			mOgreObject->setScale(LLVector3::all_one); // Prim scaling is already in their vertex data
		}
		else
		{
			//! \todo Hack to get typical Ogre mesh into correct orientation in SL coord system
			LLQuaternion meshRot = LLQuaternion(Ogre::Math::HALF_PI, LLVector3(1,0,0));
			meshRot *= rot;
			mOgreObject->setOrientation(meshRot);

			LLVector3 scale = getScale();
			if (getRexScaleMesh())
			{
            const Ogre::AxisAlignedBox &aabbox = mOgreObject->getBoundingBox();

				Ogre::Vector3 size = aabbox.getMaximum() - aabbox.getMinimum();
				if (size.x != 0.0) scale.mV[VX] /= size.x;
				if (size.y != 0.0) scale.mV[VZ] /= size.y;
				if (size.z != 0.0) scale.mV[VY] /= size.z;
			}
			//! \todo May need changing also
			mOgreObject->setScale(LLVector3(scale.mV[VX], scale.mV[VZ], scale.mV[VY]));
		}
	}
}

// reX: new function
// Update Ogre light parameters
void LLVOVolume::updateOgreLight()
{
	if (mOgreObject)
	{
		mOgreObject->setLight(getIsLight());
		
		mOgreObject->setLightParameters(
			getLightColor(),
			getLightRadius(),
			getLightFalloff(),
			getRexLightCastsShadows()); 
	}
}

// reX: new function
// Asset loading notification
void LLVOVolume::onOgreAssetLoaded(LLAssetType::EType assetType, const LLUUID& uuid)
{
    if (assetType == LLAssetType::AT_MESH)
    {
        // Rebuild volume (actually, set mesh we just loaded)
	    markOgreUpdate();
    }
    if (assetType == LLAssetType::AT_PARTICLE_SCRIPT)
    {
        if (uuid == getRexParticleScriptID())
        {
            setParticleSystem(uuid);
        }
    }
    if (assetType == LLAssetType::AT_SKELETON)
    {
       if (uuid == LLUUID(getRexAnimationPackID()))
       {
          setOgreSkeleton(uuid);
       }
    }
    if (assetType == LLAssetType::AT_MATERIAL)
    {
	    markOgreUpdate();
    }
}

// reX: new function
// Set particle system into use
void LLVOVolume::setParticleSystem(const LLUUID& particleScriptID)
{
    if (particleScriptID != mCurrentParticleScriptID)
    {
        if (particleScriptID != LLUUID::null)
        {
	        LLOgreAssetLoader* assetLoader = LLOgreRenderer::getPointer()->getAssetLoader();
	        if (!assetLoader) return;

            mOgreObject->removeParticleSystems();

            RexParticleScript* script = assetLoader->getParticleScript(particleScriptID);
            if (script)
            {
                const std::vector<std::string>& names = script->getParticleSystemNames();
                bool flipY = script->getNumImages() != 0;
                for (unsigned i = 0; i < names.size(); i++)
                {
                    mOgreObject->addParticleSystem(names[i], flipY);   
                }
                // Some boost right in the beginning of initting the effect
                script->pumpImages();
            }

        }
        else
        {
            mOgreObject->removeParticleSystems();
        }
    }
    
    mCurrentParticleScriptID = particleScriptID;
}

// reX: new function
// Update particles based on parameters
void LLVOVolume::updateOgreParticles()
{
    if (!mOgreObject) return;

    const LLUUID& particleScriptID = getRexParticleScriptID();

    if (particleScriptID != LLUUID::null) 
    {  
        LLOgreAssetLoader* assetLoader = LLOgreRenderer::getPointer()->getAssetLoader();
        if (!assetLoader) return;

        if (!assetLoader->areParticleScriptsLoaded(particleScriptID))
        {
		    assetLoader->loadAsset(particleScriptID, LLAssetType::AT_PARTICLE_SCRIPT, this, true);
        }
        else
        {
            setParticleSystem(particleScriptID);
        }
    }
    else
    {
        setParticleSystem(LLUUID::null);
    }
}

// reX new function
void LLVOVolume::updateOgreSkeleton()
{
   if (!mOgreObject) return;
   bool success = false;

   const LLUUID ogreSkeletonID = getRexAnimationPackID();

   if (ogreSkeletonID != LLUUID::null)
   {
      LLOgreAssetLoader* assetLoader = LLOgreRenderer::getPointer()->getAssetLoader();
      success = true;
 
      if (assetLoader && !assetLoader->isSkeletonLoaded(ogreSkeletonID))
      {
         assetLoader->loadAsset(ogreSkeletonID, LLAssetType::AT_SKELETON, this, true);
      }
      else
      {
          setOgreSkeleton(ogreSkeletonID);
      }
   }
   if (!success)
      setOgreSkeleton(LLUUID::null);
}

// reX: new function
void LLVOVolume::setOgreSkeleton(const LLUUID& skeletonID)
{
    if (getRexIsMesh())
    {
        if (skeletonID != LLUUID::null)
        {
	        LLOgreAssetLoader* assetLoader = LLOgreRenderer::getPointer()->getAssetLoader();
	        if (!assetLoader) return;

           mSkeletonUpdate = (mOgreObject->setSkeleton(assetLoader->getSkeletonName(skeletonID), getRexAnimationName(), getRexAnimationRate()) == false);
        }
        else
        {
            mOgreObject->setSkeleton(std::string());
        }
    }
    mCurrentSkeletonID = skeletonID;
}

// reX: new function
// Update/load mesh. Called from updateOgreGeometry() under following conditions
// - valid Ogre object exists
// - getRexIsMesh() indicates that we are a mesh
void LLVOVolume::updateOgreMesh()
{
	bool force_material_update = false;
	
    mOgreObject->setBillboard(false);

	// If fullbright or solidalpha status has changed, force update of materials
	BOOL isFullbright = getTE(0)->getFullbright();
	BOOL isSolidAlpha = getRexSolidAlpha();
   FixedOgreMaterial fixedMaterial = getRexFixedMaterial();
	if ((isFullbright != mMeshFullbright) || (isSolidAlpha != mMeshSolidAlpha) || (fixedMaterial != mFixedMaterial))
	{
		mMeshFullbright = isFullbright;
		mMeshSolidAlpha = isSolidAlpha;
      mFixedMaterial = fixedMaterial;
		force_material_update = true;
	}
	
	const LLUUID& meshID = getRexMeshID();
	if (meshID == LLUUID::null)
	{
		mOgreObject->removeMesh();
		return;
	}

	LLOgreAssetLoader* assetLoader = LLOgreRenderer::getPointer()->getAssetLoader();
	if (!assetLoader) return;
	
	const std::string& meshName = assetLoader->getMeshName(meshID);
	Ogre::Entity* entity = mOgreObject->getEntity();

    bool enableShadows = getRexCastShadows();

	// Different mesh, mesh not yet loaded, or shadowing changed
	if ((!entity) || (entity->getMesh()->getName() != meshName) || (entity->getCastShadows() != enableShadows) || mSkeletonUpdate)
	{
		if (assetLoader->isMeshLoaded(meshID))
		{
			mOgreObject->setMesh(meshName, enableShadows);
			force_material_update = true;
		}
		else
		{
			// If there is no mesh yet, queue for loading and register us for notification
			assetLoader->loadAsset(meshID, LLAssetType::AT_MESH, this, true);
			preloadOgreMeshMaterials();
			mOgreObject->removeMesh(); // Remove old while new mesh loads
			return;
		}
	}

	// Now we should have a valid entity, set parameters
	entity = mOgreObject->getEntity();
	if (entity)
	{
        F32 lodBias = getRexLOD();
		if (lodBias < 0.001) lodBias = 0.001;
        entity->setCastShadows(enableShadows);
		entity->setMeshLodBias(lodBias);
      mOgreObject->setAnimationLodBias(lodBias);
		mOgreObject->setRenderingDistance(getRexDrawDistance());
		mOgreObject->setVisible(getRexIsVisible());

		// Set/update mesh materials
		updateOgreMeshMaterials(force_material_update);
	}

	// Update scale & rotation
	updateOgrePosition();
}

// reX: new function
void LLVOVolume::updateOgreBillboard()
{
   bool force_material_update = false;
	
   BOOL isFullbright = TRUE; // Billboards are always fullbright (not affected by lights, as having them affected by lights wouldn't make sense)
	BOOL isSolidAlpha = getRexSolidAlpha();
   FixedOgreMaterial material = getRexFixedMaterial();
	if ((isFullbright != mMeshFullbright) || (isSolidAlpha != mMeshSolidAlpha) || (material != mFixedMaterial))
	{
		mMeshFullbright = isFullbright;
		mMeshSolidAlpha = isSolidAlpha;
      mFixedMaterial = material;
		force_material_update = true;
	}

   if (mOgreObject->isBillboard() == false)
   {
      force_material_update = true;
   }

   mOgreObject->setBillboard(this->getRexIsBillboard());

   // Now we should have a valid entity, set parameters
   Ogre::MovableObject *mo = mOgreObject->getMovableObject();
	if (mo)
	{
		mOgreObject->setRenderingDistance(getRexDrawDistance());
		mo->setVisible(getRexIsVisible());

		// Set/update mesh materials
		updateOgreMeshMaterials(force_material_update);
	}

   updateOgrePosition();
}

// reX: new function
// Start preloading mesh textures. Called from updateOgreMesh() when loading the mesh asset
//! \todo may actually be useless, unnecessary and/or counterproductive, testing needed
//! (due to addTextureStats() being an important factor in deciding how textures actually load)
void LLVOVolume::preloadOgreMeshMaterials()
{
    LLOgreAssetLoader* assetLoader = LLOgreRenderer::getPointer()->getAssetLoader();
    if (!assetLoader) return;

    // Have to rely on the stored material count, as info from the entity itself is unavailable
    U16 materials = getRexNumMaterials();

    if (materials != mOgreTextures.size())
    {
        mOgreTextures.resize(materials, NULL);
        mOgreMaterials.resize(materials, NULL);
    }

    for (U16 i = 0; i < materials; ++i)
    {
        const RexMaterialDef& matDef = getRexMaterialDef(i);
        const LLUUID& assetID = matDef.mAssetID;

        if (!matDef.mUseMaterialScript)
        {
            if (assetID != LLUUID::null)
            {
                LLViewerImage* image = gImageList.getImage(assetID);
                mOgreTextures[i] = image;
                //image->addTextureStats(1024.0 * 1024.0); // Add some boost
                image->resetBindTime();
            }
            else
                mOgreTextures[i] = NULL;

            mOgreMaterials[i] = NULL;
        }
        else
        {
            // If material not yet loaded, load it now
            if (assetID != LLUUID::null)
            {
                RexOgreMaterial* mat = assetLoader->getMaterial(assetID);
                mOgreMaterials[i] = mat;
    
                if (!mat)
                    assetLoader->loadAsset(assetID, LLAssetType::AT_MATERIAL, this);
            }
            else 
                mOgreMaterials[i] = NULL;

            mOgreTextures[i] = NULL;
        }
    }

	mForceTextureAreaUpdate = TRUE;
}

// reX: new function
// Update mesh textures. Called from updateOgreMesh() under following conditions
// - valid Ogre object & entity exists
void LLVOVolume::updateOgreMeshMaterials(bool force_update)
{
    LLOgreAssetLoader* assetLoader = LLOgreRenderer::getPointer()->getAssetLoader();
    if (!assetLoader) return;

    // Now that we know it, update material count (one for each subentity)
    U16 materials = mOgreObject->getNumMaterials();

    setRexNumMaterials(materials);

    // Resize the currently-in-use texture/material vectors as necessary
    if (materials != mOgreTextures.size())
    {
        mOgreTextures.resize(materials, NULL);
        mOgreMaterials.resize(materials, NULL);
    }

    std::string suffix = RexOgreLegacyMaterial::getFixedMaterialSuffix(mFixedMaterial);
    if (suffix.empty())
    {
        if (mMeshSolidAlpha) suffix += "salpha";
    }
    if (mMeshFullbright) suffix = "fb" + suffix;

    mCloneMaterialName.resize(materials, LLStringUtil::null);
    mUseClonedOgreMaterial.resize(materials, false);
    for (U16 i = 0; i < materials; ++i)
    {
        const RexMaterialDef& matDef = getRexMaterialDef(i);
        const LLUUID& materialID = matDef.mAssetID;
        bool useMaterialScripts = matDef.mUseMaterialScript;

        if (materialID != LLUUID::null) 
        {
            bool changed = false;
            if (force_update) changed = true;

            // See if textures nonexistent or changed
            if (useMaterialScripts)
            {
                if ((!mOgreMaterials[i]) || (mOgreMaterials[i]->getID() != materialID))
                    changed = true;
            }
            else
            {
                if ((mOgreTextures[i].isNull()) || (mOgreTextures[i]->getID() != materialID))
                    changed = true;
            }

            if ((changed) || (mUseClonedOgreMaterial[i] == true && mCloneMaterialName[i].empty()))
            {
                mOgreObject->beginMaterialUpdate();

                std::string materialName;
                RexOgreLegacyMaterial* legacyMat = NULL;

                if (!useMaterialScripts)
                {
                    LLViewerImage* image = gImageList.getImage(materialID);
                    mOgreTextures[i] = image;
                    mOgreMaterials[i] = NULL;
                    legacyMat = image->getOgreMaterial();
                    if (legacyMat)
                        materialName = legacyMat->getName() + suffix;
                    else
                        materialName = RexOgreLegacyMaterial::sDefaultMaterialName + suffix;

                    //image->addTextureStats(1024.0 * 1024.0); // Add some boost when building or changing texture
                    image->resetBindTime();
                }
                else
                {
                    // If material not yet loaded, load it now
                    if (materialID != LLUUID::null)
                    {
                        RexOgreMaterial* mat = assetLoader->getMaterial(materialID);
                        mOgreMaterials[i] = mat;
                        mOgreTextures[i] = NULL;
        
                        if (!mat)
                        {
                            assetLoader->loadAsset(materialID, LLAssetType::AT_MATERIAL, this);
                        }
                        else materialName = mat->getName();
                    }
                }

                if (mUseClonedOgreMaterial[i])
                {
//                      std::string texNum = Ogre::StringConverter::toString(i);

                    // out with the old
                    if (mCloneMaterialName[i].empty())
                    {
                        mCloneMaterialName[i] = RexOgreLegacyMaterial::generateUniqueMaterialName();

                        // in with the new
                         const Ogre::MaterialPtr &oldMaterial = Ogre::MaterialManager::getSingleton().getByName(materialName);
                         Ogre::MaterialPtr newMaterial = oldMaterial->clone(mCloneMaterialName[i]);
              
                         materialName = mCloneMaterialName[i];
                         // Note: RexOgreMaterial's don't get clones added, but since they don't update themselves on texture changes
                         // the same way as RexLegacyOgreMaterial's do, it (hopefully) shouldn't matter
                         if (legacyMat) 
                             legacyMat->addClone(materialName);
                    }
                }

                mOgreObject->setMaterialName(materialName, i);
            }	
        }
        else
        {
            mOgreObject->beginMaterialUpdate();
            mOgreTextures[i] = NULL;
            mOgreMaterials[i] = NULL;

            mOgreObject->setMaterialName(RexOgreLegacyMaterial::sDefaultMaterialName + suffix, i);
        }
    }

    //// If there are more submeshes than we want to support, set them to default untextured, but with correct variation
    //if (allMaterials > materials)
    //{
    //    mOgreObject->beginMaterialUpdate(); // Note: it doesn't do harm to call this multiple times

    //    for (U16 i = materials; i < allMaterials; i++)
    //    {
    //        mOgreObject->setMaterialName(RexOgreLegacyMaterial::sDefaultMaterialName + suffix, i);
    //    }
    //}

    mOgreObject->endMaterialUpdate();
    mForceTextureAreaUpdate = TRUE;
}

// reX: new function
// Create/update Ogre manual object geometry or mesh for the volume
void LLVOVolume::updateOgreGeometry()
{
    LLOgreAssetLoader* assetLoader = LLOgreRenderer::getPointer()->getAssetLoader();
    if (!assetLoader) return;

	// Note: we are in Ogre context here, no need for context switching
	mOgreUpdatePending = false;

	// Have a valid Ogre object?
	if (!mOgreObject) return;

    // On first time, update particle system (because we get the network parameter update when the ogreobject doesn't yet exist)
    if (mParticleFirstTime)
    {
        mParticleFirstTime = FALSE;
        updateOgreParticles();
    }
    if (mSkeletonFirstTime)
    {
      mSkeletonFirstTime = FALSE;
      updateOgreSkeleton();
    }

	// Are we supposed to be a mesh?
	if (getRexIsMesh())
	{
		updateOgreMesh();
		return;
	}

   if (getRexIsBillboard())
   {
      updateOgreBillboard();
      return;
   }

	// Remove previously existing renderables
	mOgreObject->removeMesh();
	mOgreObject->removeManualObject();
   mOgreObject->removeBillboard();

	// If we are a hud attachment, skip
	if (isHUDAttachment())
	{		
		return;
	}

	// Have a valid volume?
	LLVolume* volume = getVolume();
	if (!volume)
	{
		return;
	}

	// Check for null or dead drawable 
	if ((mDrawable.isNull()) || (mDrawable->isDead()))
	{
		return;
	}

	// Create new manual object
	Ogre::ManualObject* manualObject = mOgreObject->createManualObject();

	S32 faces = volume->getNumVolumeFaces();
	S32 index_offset = 0;
	LLViewerImage *prevImagep = 0;
	bool first_face = true;
	bool isAlpha = false;
	bool prevAlpha = false;
	BOOL isFullbright = false;
	BOOL prevFullbright = false;
           
    FixedOgreMaterial fixedMaterial = getRexFixedMaterial();

	// Loop first to estimate vertex count
	S32 total_vertices = 0;
	S32 total_indices = 0;
	for (S32 f = 0; f < faces; ++f)
	{
		const LLVolumeFace& vf = volume->getVolumeFace(f);

		total_vertices += (S32)vf.mVertices.size();
		total_indices += (S32)vf.mIndices.size();
	}

   mCurrentMaterials.resize(mNumFaces, LLStringUtil::null);
   mRecreateClonedMaterial.resize(mNumFaces, false);
   mUseClonedOgreMaterial.resize(mNumFaces, false);
         

	LLVector3 scale = isVolumeGlobal() ? LLVector3(1,1,1) : getScale();

   S32 facePitch = (S32)sqrtf(mNumFaces);
   S32 faceDepth = (S32)ceil(mNumFaces / (F32)facePitch);
	for (S32 f = 0; f < mNumFaces; ++f)
	{
		// Get face, associated image, color
		LLFace *facep = mDrawable->getFace(f);
		const LLVolumeFace& vf = volume->getVolumeFace(f);
		if (!facep) continue;
		LLViewerImage *imagep = facep->getTexture();
		if (!imagep) continue;
		const LLColor4& c = facep->getRenderColor();

		// Check for face being very transparent 
		if (c.mV[VALPHA] <= 0.11) continue;

      LLVector2 faceRatio((f % facePitch) / (F32)(facePitch), (f / facePitch) / (F32)(faceDepth));
		
		// Get amount of vertices / indices
		S32 num_vertices = (S32)vf.mVertices.size();
		S32 num_indices = (S32)vf.mIndices.size();
		if ((!num_vertices) || (!num_indices)) continue;

		// See if it's a different texture, begin new section then (and another new batch :))
		isAlpha = (c.mV[VALPHA] < 0.99);
		isFullbright = getTE(f)->getFullbright();
      bool alphaChanged = (prevAlpha != isAlpha);
      bool fullBrightChanged = (prevFullbright != isFullbright);

		if ((first_face) || (prevImagep != imagep) || alphaChanged || fullBrightChanged)
		{
			if (first_face)
			{
				// Start first manual object section, estimate amounts
				manualObject->estimateVertexCount(total_vertices);
				manualObject->estimateIndexCount(total_indices);
			}
			else
			{
				// End the previous manual object section and start new
				manualObject->end();
				index_offset = 0;
			}

			first_face = false;
			prevImagep = imagep;
			prevAlpha = isAlpha;
			prevFullbright = isFullbright;

		 	// Get Ogre material for this image
			RexOgreLegacyMaterial *ogreMaterial = imagep->getOgreMaterial();

         std::string suffix = RexOgreLegacyMaterial::getFixedMaterialSuffix(fixedMaterial);
         if (suffix.empty())
         {
			    if (isAlpha)
			    {
    				suffix += "alpha";
			    }
			    else
			    {
    				if (getRexSolidAlpha()) suffix += "salpha";
	    		}
         }
			if (isFullbright) suffix = "fb" + suffix;

         std::string materialName;
			if (ogreMaterial)
            materialName = ogreMaterial->getName() + suffix;
         else
            materialName = RexOgreLegacyMaterial::sDefaultMaterialName + suffix;

        // Hack support of materials for prims for now: use first materialscript for all faces
        const RexMaterialDef& matDef = getRexMaterialDef(0);
        if (matDef.mUseMaterialScript)
        {
            materialName = RexOgreLegacyMaterial::sDefaultMaterialName + suffix;

            mOgreMaterials.resize(1, NULL);
            mOgreTextures.resize(1, NULL);

            const LLUUID& materialID = matDef.mAssetID;
            if (materialID != LLUUID::null)
            {
                RexOgreMaterial* mat = assetLoader->getMaterial(materialID);
                mOgreMaterials[0] = mat;
        
                if (!mat)
                {
                    assetLoader->loadAsset(materialID, LLAssetType::AT_MATERIAL, this);
                }
                else materialName = mat->getName();
            }
            else mOgreMaterials[0] = NULL;

            mOgreTextures[0] = NULL;
        }

         std::string oldMaterial = mCurrentMaterials[f];
         mCurrentMaterials[f] = materialName;
         if (oldMaterial.empty())
            oldMaterial = materialName;

         if (mUseClonedOgreMaterial[f])
			{
            mCloneMaterialName.resize(mNumFaces, LLStringUtil::null);
//            std::string faceString = Ogre::StringConverter::toString(f);
            if (mCloneMaterialName[f].empty())
            {
               mCloneMaterialName[f] = RexOgreLegacyMaterial::generateUniqueMaterialName();

               const Ogre::MaterialPtr &material = Ogre::MaterialManager::getSingleton().getByName(materialName);
               Ogre::MaterialPtr newMaterial = material->clone(mCloneMaterialName[f]);
            
               materialName = mCloneMaterialName[f];
               ogreMaterial->addClone(materialName);
			   } else if (mRecreateClonedMaterial[f])
            {
               const Ogre::MaterialPtr &baseMaterial = Ogre::MaterialManager::getSingleton().getByName(materialName);
               Ogre::MaterialPtr clonematerial = Ogre::MaterialManager::getSingleton().getByName(mCloneMaterialName[f]);
               baseMaterial->copyDetailsTo(clonematerial);
               materialName = mCloneMaterialName[f];
               mRecreateClonedMaterial[f] = false;
            } else if (materialName == oldMaterial)
               materialName = mCloneMaterialName[f];
            else
            {
               mUseClonedOgreMaterial[f] = false;
               mRecreateClonedMaterial[f] = true;
            }
         }
         

         manualObject->begin(materialName, Ogre::RenderOperation::OT_TRIANGLE_LIST);
		}

		// Prepare parameters for texture mapping
		F32 r = 0.0f, os = 0.0f, ot = 0.0f, ms = 1.0f, mt = 1.0f, cos_ang = 1.0f, sin_ang = 0.0f;
		U8 texgen = LLTextureEntry::TEX_GEN_DEFAULT;
		const LLTextureEntry* tep = facep->getTextureEntry();	
		if (tep)
		{
			texgen = tep->getTexGen();
			r  = tep->getRotation();
			os = tep->mOffsetS;
			ot = tep->mOffsetT;
			ms = tep->mScaleS;
			mt = tep->mScaleT;
			cos_ang = cos(r);
			sin_ang = sin(r);
		}

		for (S32 i = 0; i < num_vertices; ++i)
		{
			const LLVolumeFace::VertexData &v = vf.mVertices[i];

			// Set position & normal & colour: perform position scaling here 
			// computationally, so that HW scaling doesn't throw lighting off
			LLVector3 pos = v.mPosition;
			pos.scaleVec(scale); 
			manualObject->position(LLOgreRenderer::toOgreVector(pos));			
			LLVector3 normal = v.mNormal;
			normal.normVec();
			manualObject->normal(LLOgreRenderer::toOgreVector(v.mNormal));
			manualObject->colour(c.mV[0], c.mV[1], c.mV[2], c.mV[3]);

			// Texture mapping (from llface.cpp)
			LLVector2 tc = v.mTexCoord;
			if (texgen != LLTextureEntry::TEX_GEN_DEFAULT)
			{
				LLVector3 vec = v.mPosition; 
			
				vec.scaleVec(scale);

				switch (texgen)
				{
					case LLTextureEntry::TEX_GEN_PLANAR:
						planarProjection(tc, vf.mVertices[i], vf.mCenter, vec);
						break;
					// These are supposedly broken
					case LLTextureEntry::TEX_GEN_SPHERICAL:
						sphericalProjection(tc, vf.mVertices[i], vf.mCenter, vec);
						break;
					case LLTextureEntry::TEX_GEN_CYLINDRICAL:
						cylindricalProjection(tc, vf.mVertices[i], vf.mCenter, vec);
						break;
					default:
						break;
				}		
			}

         LLVector2 tc2 = v.mTexCoord;
			xform(tc, cos_ang, sin_ang, os, ot, ms, mt);
			manualObject->textureCoord(tc.mV[0], tc.mV[1]);

         // Second set of texture coords for light mapping the primitive
         // Makes the assumption the original texture coords are always clamped to [0,1]
         // Divides the coords evenly to the texture
         tc2.mV[0] /= (F32)(facePitch);
         tc2.mV[1] /= (F32)(faceDepth);
         manualObject->textureCoord(tc2.mV[0] + faceRatio.mV[0], tc2.mV[1] + faceRatio.mV[1]);
		}

		for (S32 i = 0; i < num_indices; i += 3)
		{
			// Create triangles from indices
			manualObject->triangle(
				vf.mIndices[i] + index_offset,
				vf.mIndices[i+1] + index_offset,
				vf.mIndices[i+2] + index_offset);
		}

		index_offset += num_vertices;
	}

	// Did we make any faces?
	if (!first_face)
	{
		// End the last section of the manual object
		manualObject->end();		

		// Convert manual object to mesh
		std::string meshName = mOgreObject->convertToMesh(manualObject);

		updateOgrePosition();

      // disable skeleton for primitives
      mOgreObject->setSkeleton(std::string());

		// Now create entity from it
		mOgreObject->setMesh(meshName, getRexCastShadows());
		mOgreObject->setRenderingDistance(getRexDrawDistance());		 

		// Reapply description texture if one exists
		if (mCurrentDescription.length() > 0)
		{
			setDescriptionTexture(mCurrentDescription);
		}
	}

	// Manual object has served its purpose, now destroy
	LLOgreRenderer::getPointer()->getSceneMgr()->destroyManualObject(manualObject);
}

// reX: new function
void LLVOVolume::setDescriptionTexture(const std::string &description)
{ 
	// If we should be a mesh, don't apply
	if (getRexIsMesh() || getRexIsBillboard()) return;

    if (!getRexShowText()) return;
	
	//mMaterialNameText.clear();

//   //! \todo disabled for now
//   return;
//   mCurrentTextureText.clear();
   // see if we have a description
	if (description.empty() || description == LLHoverView::DEFAULT_DESC)
		 return;

	if (description.size() >= 7 && description.substr(0, 7).compare("http://") == 0)
	{	
		// Fetch data from web
		llinfos << "Fetching data from web: " << description << llendl;
		LLHTTPClient::ResponderPtr responder = new TextTextureHttpResponder(this);
		LLHTTPClient::get(description, responder);
		return;
	}

	// Save in case we fail for not having an entity yet, then we can reapply
	mCurrentDescription = description;

    // If we are a hud attachment, skip
	if (isHUDAttachment())
	{
		return;
	}

	// Have a valid volume?
	LLVolume* volume = getVolume();
	if (!volume)
	{
		return;
	}

	// Check for null or dead drawable 
	if ((mDrawable.isNull()) || (mDrawable->isDead()))
	{
		return;
	}

	// Check for existence of valid Ogre object & entity
	if (!mOgreObject)
		return;

	Ogre::Entity* entity = mOgreObject->getEntity();
	if (!entity) 
		return;

	bool reapply = (mCurrentTextureText == description);	

//   mCurrentTextureText = description;

//   Ogre::Texture *texture = 0;
   Ogre::TexturePtr texture;

	//LLViewerImage *oldImage = 0;
	//S32 f;
	//for (f = 0; f < mNumFaces; ++f)
	//{
	// Get first face and associated ogre material
	LLFace *facep = mDrawable->getFace(0);
	if (!facep) 
		return;

	LLViewerImage *imagep = facep->getTexture();
	if (!imagep)
		return;

	//oldImage = imagep;

	RexOgreLegacyMaterial* ogreMaterial = imagep->getOgreMaterial();
	if (!ogreMaterial) 
		return;

	BOOL isFullbright = getTE(0)->getFullbright();
	std::string suffix = "";
	if (isFullbright) suffix = "fb";

	Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName(ogreMaterial->getName() + suffix);
	if (material.isNull())
		return;

	std::string newName;
	if (!reapply)
	{
		newName = RexOgreLegacyMaterial::generateUniqueMaterialName();
		if (newName.empty())
			return;
	}
	else 
	{
		newName = mMaterialNameText;
	}	

	//continue;

	// Remove old material and texture
	LLOgreRenderer::getPointer()->setOgreContext();
	Ogre::MaterialManager::getSingleton().remove(mMaterialNameText);
	if (!reapply)
	{
		Ogre::TextureManager::getSingleton().remove(mMaterialNameText);
	}
	LLOgreRenderer::getPointer()->setSLContext();
	mMaterialNameText = newName;

	Ogre::MaterialPtr newMaterial = material->clone(newName);
	ogreMaterial->addClone(newName);

	mOgreObject->beginMaterialUpdate();
	entity->setMaterialName(newName);
	mOgreObject->endMaterialUpdate();

//     imagep->getOgreMaterial()->setMaterial(newMaterial);
     
	Ogre::Pass *pass = newMaterial->getTechnique(0)->getPass(0);
	S32 numtextures = pass->getNumTextureUnitStates();

	if (!reapply)
	{
//      if (!texture)
		if (texture.isNull())
		{
//         LLOgreRenderer::getPointer()->setOgreContext();
//         const std::string &texName = imagep->getID().getString();
		// Remove old texture before creating new
//         Ogre::ResourcePtr oldtexture = Ogre::TextureManager::getSingleton().getByName(texName);
//         if (oldtexture.isNull() == false)
//            Ogre::TextureManager::getSingleton().remove(oldtexture);

//         LLOgreRenderer::getPointer()->setSLContext();
         
		texture = LLOgreFont::createTextureFromString(mMaterialNameText, description, std::string("None"), 128, 128);
		if (texture.isNull())
			return;
		mCurrentTextureText = description;
	 }
  }
  else
  {
	  texture = Ogre::TextureManager::getSingleton().getByName(mMaterialNameText);
	  if (texture.isNull())
		  return;
  }

  if (numtextures < 4)
  {
     bool found = false;
     S32 n;
     for (n=0 ; n<numtextures ; ++n)
     {
        if (pass->getTextureUnitState(n)->getTextureName().compare(texture->getName()) == 0)
        {
           pass->getTextureUnitState(n)->setTextureName(texture->getName());
           found = true;
           break;
        }
     }
     if (!found)
     {
        // Add text as second texture layer
        pass->createTextureUnitState(texture->getName());
        pass->getTextureUnitState(numtextures)->setColourOperation(Ogre::LBO_ALPHA_BLEND);
        pass->getTextureUnitState(numtextures)->setTextureVScale(-1.0);
     }
  }
   //}
}

// reX: new function
TextTextureHttpResponder::TextTextureHttpResponder(LLVOVolume *node) : mNode(node)
{
   if (mNode)
      mNode->ref();
}

// reX: new function
TextTextureHttpResponder::~TextTextureHttpResponder()
{
   if (mNode)
      mNode->unref();
}
//! Parse the received content and handle it
void TextTextureHttpResponder::result(const LLBufferStream &stream)
{
//   stream.mStreamBuf.
   // do stuff
//   std::string str;
//   std::stringstream ss;
//   ss << stream.mStreamBuf;
//   ss >> str;
//   std::istream str = stream;
//   stream.get(
//   std::string str = stream.get();
   llinfos << "Result: " << stream << llendl;

//   delete this;
}

// reX: new function
void TextTextureHttpResponder::completedRaw(U32 status, const std::string &reason, const LLChannelDescriptors &channels,
                                       const LLIOPipe::buffer_ptr_t &buffer)
{
   llinfos << "status: " << status << llendl;

   if (200 <= status  &&  status < 300)
	{
      LLBufferStream istr(channels, buffer.get());
		result(istr);
	}
	else
		error(status, reason);
}

// reX: new function
void LLVOVolume::sendRexPrimData()
{
    mRexPrimData.send(this->getID());
}

// reX: new function
void LLVOVolume::setRexDrawType(RexPrimData::RexDrawType drawType, bool suppressSend)
{
    mRexPrimData.setDrawType(drawType);
    if (mRexPrimData.isChanged())
    {
        markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexAnimationPackID(const LLUUID& animationPackageID, bool suppressSend)
{
    mRexPrimData.setAnimationPackID(animationPackageID);
    if (mRexPrimData.isChanged())
    {
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexAnimationName(const std::string& animationName, bool suppressSend)
{
    mRexPrimData.setAnimationName(animationName);
    if (mRexPrimData.isChanged())
    {
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexAnimationRate(F32 animationRate, bool suppressSend)
{
    mRexPrimData.setAnimationRate(animationRate);
    if (mRexPrimData.isChanged())
    {
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexSoundID(const LLUUID& soundID, bool suppressSend)
{
    mRexPrimData.setSoundID(soundID);
	
	if (mRexPrimData.isChanged())
    {
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexSoundSourceID(LLUUID soundSourceID)
{
	// Server doesn't need this information
    mRexPrimData.setSoundSourceID(soundSourceID);
}

// reX: new function
void LLVOVolume::setRexSoundVolume(F32 volume, bool suppressSend)
{
    mRexPrimData.setSoundVolume(volume);

	if (mRexPrimData.isChanged())
    {
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexSoundRadius(F32 radius, bool suppressSend)
{
    mRexPrimData.setSoundRadius(radius);

	if (mRexPrimData.isChanged())
    {
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexClassName(const std::string& name, bool suppressSend)
{
    mRexPrimData.setClassName(name);
    if (mRexPrimData.isChanged())
    {
        if (!suppressSend) sendRexPrimData();
	}
}

// reX: new function
void LLVOVolume::setRexIsVisible(BOOL enabled, bool suppressSend)
{
    mRexPrimData.setIsVisible(enabled);

    if (mRexPrimData.isChanged())
    {
	    if (getRexIsMesh()) markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexCastShadows(BOOL enabled, bool suppressSend)
{
    mRexPrimData.setCastShadows(enabled);

    if (mRexPrimData.isChanged())
    {
        markOgreUpdate();
        if (!suppressSend) sendRexPrimData(); 
    }
}

// reX: new function
void LLVOVolume::setRexLightCastsShadows(BOOL enabled, bool suppressSend)
{
    mRexPrimData.setLightCastsShadows(enabled);

    if (mRexPrimData.isChanged())
    {
        markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}


// reX: new function
void LLVOVolume::setRexShowText(BOOL enabled, bool suppressSend)
{
    mRexPrimData.setShowText(enabled);

    if (mRexPrimData.isChanged())
    {
        markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexScaleMesh(BOOL enabled, bool suppressSend)
{
    mRexPrimData.setScaleMesh(enabled);

    if (mRexPrimData.isChanged())
    {
	    if (getRexIsMesh()) markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexSolidAlpha(BOOL enabled, bool suppressSend)
{
    mRexPrimData.setSolidAlpha(enabled);

    if (mRexPrimData.isChanged())
    {
        markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexLOD(F32 lod, bool suppressSend)
{
    mRexPrimData.setLOD(lod);

    if (mRexPrimData.isChanged())
    {
        markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexSelectionPriority(int priority, bool suppressSend)
{
    mRexPrimData.setSelectionPriority(priority);

    if (mRexPrimData.isChanged())
    {
        if (!suppressSend) sendRexPrimData();
    }
}


// reX: new function
void LLVOVolume::setRexDrawDistance(F32 drawDistance, bool suppressSend)
{
    mRexPrimData.setDrawDistance(drawDistance);

    if (mRexPrimData.isChanged())
    {
        markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}


// reX: new function
void LLVOVolume::setRexMeshID(const LLUUID& meshID, bool suppressSend)
{
    mRexPrimData.setMeshID(meshID);

    if (mRexPrimData.isChanged())
    {
        markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexCollisionMeshID(const LLUUID& meshID, bool suppressSend)
{
    mRexPrimData.setCollisionMeshID(meshID);

    if (mRexPrimData.isChanged())
    {
	    // Affects only serverside collision detection, no rebuild necessary
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexNumMaterials(U16 numMaterials, bool suppressSend)
{
    mRexPrimData.setNumMaterials(numMaterials);

    if (mRexPrimData.isChanged())
    {
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexMaterialID(U16 index, const LLUUID& materialID, bool suppressSend)
{
    mRexPrimData.setMaterialID(index, materialID);

    if (mRexPrimData.isChanged())
    {
        markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}


// reX: new function
void LLVOVolume::setRexMaterialDef(U16 index, const RexMaterialDef& materialDef, bool suppressSend)
{
    mRexPrimData.setMaterialDef(index, materialDef);

    if (mRexPrimData.isChanged())
    {
        markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexFixedMaterial(FixedOgreMaterial material, bool suppressSend)
{
    mRexPrimData.setFixedMaterial(material);

    if (mRexPrimData.isChanged())
    {
        markOgreUpdate();
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexParticleScriptID(const LLUUID& particleScriptID, bool suppressSend)
{
    mRexPrimData.setParticleScriptID(particleScriptID);

    if (mRexPrimData.isChanged())
    {
        updateOgreParticles();
        if (!suppressSend) sendRexPrimData();
    }
}

// reX: new function
void LLVOVolume::setRexData(std::string data)
{
	mRexData = data;
}

// reX: new fuction
void LLVOVolume::createRexSoundSource(BOOL create)
{	
    if (!gAudiop)
        return;
	
	// Delete the old audio source (if exists)
	LLAudioSource *temp_asp = gAudiop->findAudioSource(mRexPrimData.getSoundSourceID());
	if(temp_asp)
	{	
		gAudiop->cleanupAudioSource(temp_asp);
	}

	// Add a new audio source
	if(gAudiop && create && mRexPrimData.getSoundID().notNull())
	{
		LLUUID source_id;
		source_id.generate();
		
		BOOL loop = TRUE;	//TRUE on default at the moment
		F32 radius = mRexPrimData.getSoundRadius();
		F32 gain = mRexPrimData.getSoundVolume();

		LLAudioSource *asp = new LLAudioSource(source_id, gAgent.getID(), gain);
		gAudiop->addAudioSource(asp);

		asp->setRexSoundID(mRexPrimData.getSoundID());
		asp->setRexSound(TRUE);
		asp->setRexLoop(loop);
		
		if(radius == 0)
		{
			asp->setAmbient(TRUE); // Ambient means that the sound volume level is same everywhere in the world
			asp->setRadius(FALSE);
		}
		else
		{
			asp->setRadius(TRUE);
			asp->defineRadius(radius);
			asp->setPositionGlobal(getPositionGlobal());
		}				
		mRexPrimData.setSoundSourceID(source_id);
	}
}

// reX: new function
void LLVOVolume::setUseClonedMaterial(bool useCloned)
{
   mUseClonedOgreMaterial.clear();
   mUseClonedOgreMaterial.resize(std::max(mNumFaces, (S32)mOgreObject->getNumMaterials()), true);
//   mUseClonedOgreMaterial = useCloned;
}

// reX: new function
const std::string& LLVOVolume::getRexClassName() const
{
    return mRexPrimData.getClassName();
}

// reX: new function
BOOL LLVOVolume::getRexIsMesh() const
{
    return mRexPrimData.getDrawType() == RexPrimData::DRAWTYPE_MESH;
}

// reX: new function
BOOL LLVOVolume::getRexIsVisible() const
{
    return mRexPrimData.getIsVisible();
}

// reX: new function
BOOL LLVOVolume::getRexCastShadows() const
{
    return mRexPrimData.getCastShadows();
}

// reX: new function
BOOL LLVOVolume::getRexLightCastsShadows() const
{
    return mRexPrimData.getLightCastsShadows();
}

// reX: new function
BOOL LLVOVolume::getRexShowText() const
{
    return mRexPrimData.getShowText();
}

// reX: new function
BOOL LLVOVolume::getRexScaleMesh() const
{
    return mRexPrimData.getScaleMesh();
}

// reX: new function
BOOL LLVOVolume::getRexSolidAlpha() const
{
    return mRexPrimData.getSolidAlpha();
}

// reX: new function
BOOL LLVOVolume::getRexIsBillboard() const
{
    return mRexPrimData.getDrawType() == RexPrimData::DRAWTYPE_BILLBOARD;
}

// reX: new function
F32 LLVOVolume::getRexLOD() const
{
    return mRexPrimData.getLOD();
}

// reX: new function
int LLVOVolume::getRexSelectionPriority() const
{
    return mRexPrimData.getSelectionPriority();
}

// reX: new function
F32 LLVOVolume::getRexDrawDistance() const
{
    return mRexPrimData.getDrawDistance();
}

// reX: new function
const LLUUID& LLVOVolume::getRexMeshID() const
{
    return mRexPrimData.getMeshID();
}

// reX: new function
const LLUUID& LLVOVolume::getRexCollisionMeshID() const
{
    return mRexPrimData.getCollisionMeshID();
}

// reX: new function
RexPrimData::RexDrawType LLVOVolume::getRexDrawType() const
{
    return mRexPrimData.getDrawType();
}

// reX: new function
const LLUUID& LLVOVolume::getRexAnimationPackID() const
{
    return mRexPrimData.getAnimationPackID();
}

// reX: new function
const std::string& LLVOVolume::getRexAnimationName() const
{
    return mRexPrimData.getAnimationName();
}

// reX: new function
F32 LLVOVolume::getRexAnimationRate() const
{
    return mRexPrimData.getAnimationRate();
}

// reX: new function
const LLUUID& LLVOVolume::getRexSoundID() const
{
    return mRexPrimData.getSoundID();
}
// reX: new function
const LLUUID& LLVOVolume::getRexSoundSourceID() const
{
	return mRexPrimData.getSoundSourceID();
}

// reX: new function
F32 LLVOVolume::getRexSoundVolume() const
{
    return mRexPrimData.getSoundVolume();
}

// reX: new function
F32 LLVOVolume::getRexSoundRadius() const
{
    return mRexPrimData.getSoundRadius();
}

// reX: new function
U16 LLVOVolume::getRexNumMaterials() const
{
    return mRexPrimData.getNumMaterials();
}

// reX: new function
const LLUUID& LLVOVolume::getRexMaterialID(U16 index) const
{
    // This is sort of hack, should use the materialdef's instead for handling both asset ID & per-submesh material script usage at once
    return mRexPrimData.getMaterialID(index);
}

// reX: new function
const RexMaterialDef& LLVOVolume::getRexMaterialDef(U16 index) const
{
    return mRexPrimData.getMaterialDef(index);
}

// reX: new function
const LLUUID& LLVOVolume::getRexParticleScriptID() const
{
    return mRexPrimData.getParticleScriptID();
}

// reX: new function
FixedOgreMaterial LLVOVolume::getRexFixedMaterial() const
{
    return mRexPrimData.getFixedMaterial();
}

// reX: new function
std::string LLVOVolume::getRexData()
{
	return mRexData;
}
