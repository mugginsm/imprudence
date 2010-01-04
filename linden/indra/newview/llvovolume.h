/** 
 * @file llvovolume.h
 * @brief LLVOVolume class header file
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

#ifndef LL_LLVOVOLUME_H
#define LL_LLVOVOLUME_H

#include "llviewerobject.h"
#include "llviewerimage.h"
#include "llframetimer.h"
#include "llapr.h"
// reX: new includes
#include "llbufferstream.h"
#include "rexprimdata.h"
#include <map>

// ReX: forward reference
class RexOgreMaterial;

class LLViewerTextureAnim;
class LLDrawPool;
class LLSelectNode;

enum LLVolumeInterfaceType
{
	INTERFACE_FLEXIBLE = 1,
};

// Base class for implementations of the volume - Primitive, Flexible Object, etc.
class LLVolumeInterface
{
public:
	virtual ~LLVolumeInterface() { }
	virtual LLVolumeInterfaceType getInterfaceType() const = 0;
	virtual BOOL doIdleUpdate(LLAgent &agent, LLWorld &world, const F64 &time) = 0;
	virtual BOOL doUpdateGeometry(LLDrawable *drawable) = 0;
	virtual LLVector3 getPivotPosition() const = 0;
	virtual void onSetVolume(const LLVolumeParams &volume_params, const S32 detail) = 0;
	virtual void onSetScale(const LLVector3 &scale, BOOL damped) = 0;
	virtual void onParameterChanged(U16 param_type, LLNetworkData *data, BOOL in_use, bool local_origin) = 0;
	virtual void onShift(const LLVector3 &shift_vector) = 0;
	virtual bool isVolumeUnique() const = 0; // Do we need a unique LLVolume instance?
	virtual bool isVolumeGlobal() const = 0; // Are we in global space?
	virtual bool isActive() const = 0; // Is this object currently active?
	virtual const LLMatrix4& getWorldMatrix(LLXformMatrix* xform) const = 0;
	virtual void updateRelativeXform() = 0;
	virtual U32 getID() const = 0;
	virtual void preRebuild() = 0;
};

// Class which embodies all Volume objects (with pcode LL_PCODE_VOLUME)
class LLVOVolume : public LLViewerObject
{
protected:
	virtual				~LLVOVolume();

public:
	static		void	initClass();
	static 		void 	preUpdateGeom();
	
	enum 
	{
		VERTEX_DATA_MASK =	(1 << LLVertexBuffer::TYPE_VERTEX) |
							(1 << LLVertexBuffer::TYPE_NORMAL) |
							(1 << LLVertexBuffer::TYPE_TEXCOORD0) |
							(1 << LLVertexBuffer::TYPE_TEXCOORD1) |
							(1 << LLVertexBuffer::TYPE_COLOR)
	};

public:
						LLVOVolume(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp);

	/*virtual*/ LLDrawable* createDrawable(LLPipeline *pipeline);

				void	deleteFaces();

				void	animateTextures();
	/*virtual*/ BOOL	idleUpdate(LLAgent &agent, LLWorld &world, const F64 &time);

	/*virtual*/ BOOL	isActive() const;
	/*virtual*/ BOOL	isAttachment() const;
	/*virtual*/ BOOL	isRootEdit() const; // overridden for sake of attachments treating themselves as a root object
	/*virtual*/ BOOL	isHUDAttachment() const;

				void	generateSilhouette(LLSelectNode* nodep, const LLVector3& view_point);
	/*virtual*/	void	setParent(LLViewerObject* parent);
				S32		getLOD() const							{ return mLOD; }
	const LLVector3		getPivotPositionAgent() const;
	const LLMatrix4&	getRelativeXform() const				{ return mRelativeXform; }
	const LLMatrix3&	getRelativeXformInvTrans() const		{ return mRelativeXformInvTrans; }
	/*virtual*/	const LLMatrix4	getRenderMatrix() const;


	/*virtual*/ BOOL lineSegmentIntersect(const LLVector3& start, const LLVector3& end, 
										  S32 face = -1,                        // which face to check, -1 = ALL_SIDES
										  BOOL pick_transparent = FALSE,
										  S32* face_hit = NULL,                 // which face was hit
										  LLVector3* intersection = NULL,       // return the intersection point
										  LLVector2* tex_coord = NULL,          // return the texture coordinates of the intersection point
										  LLVector3* normal = NULL,             // return the surface normal at the intersection point
										  LLVector3* bi_normal = NULL           // return the surface bi-normal at the intersection point
		);
	
				LLVector3 agentPositionToVolume(const LLVector3& pos) const;
				LLVector3 agentDirectionToVolume(const LLVector3& dir) const;
				LLVector3 volumePositionToAgent(const LLVector3& dir) const;
				LLVector3 volumeDirectionToAgent(const LLVector3& dir) const;

				
				BOOL	getVolumeChanged() const				{ return mVolumeChanged; }
				F32		getTextureVirtualSize(LLFace* face);
	/*virtual*/ F32  	getRadius() const						{ return mVObjRadius; };
				const LLMatrix4& getWorldMatrix(LLXformMatrix* xform) const;

				void	markForUpdate(BOOL priority)			{ LLViewerObject::markForUpdate(priority); mVolumeChanged = TRUE; }

	/*virtual*/ void	onShift(const LLVector3 &shift_vector); // Called when the drawable shifts

	/*virtual*/ void	parameterChanged(U16 param_type, bool local_origin);
	/*virtual*/ void	parameterChanged(U16 param_type, LLNetworkData* data, BOOL in_use, bool local_origin);

	/*virtual*/ U32		processUpdateMessage(LLMessageSystem *mesgsys,
											void **user_data,
											U32 block_num, const EObjectUpdateType update_type,
											LLDataPacker *dp);

	/*virtual*/ void	setSelected(BOOL sel);
	/*virtual*/ BOOL	setDrawableParent(LLDrawable* parentp);

	/*virtual*/ void	setScale(const LLVector3 &scale, BOOL damped);

	/*virtual*/ void	setTEImage(const U8 te, LLViewerImage *imagep);
	/*virtual*/ S32		setTETexture(const U8 te, const LLUUID &uuid);
	/*virtual*/ S32		setTEColor(const U8 te, const LLColor3 &color);
	/*virtual*/ S32		setTEColor(const U8 te, const LLColor4 &color);
	/*virtual*/ S32		setTEBumpmap(const U8 te, const U8 bump);
	/*virtual*/ S32		setTEShiny(const U8 te, const U8 shiny);
	/*virtual*/ S32		setTEFullbright(const U8 te, const U8 fullbright);
	/*virtual*/ S32		setTEBumpShinyFullbright(const U8 te, const U8 bump);
	/*virtual*/ S32		setTEMediaFlags(const U8 te, const U8 media_flags);
	/*virtual*/ S32		setTEGlow(const U8 te, const F32 glow);
	/*virtual*/ S32		setTEScale(const U8 te, const F32 s, const F32 t);
	/*virtual*/ S32		setTEScaleS(const U8 te, const F32 s);
	/*virtual*/ S32		setTEScaleT(const U8 te, const F32 t);
	/*virtual*/ S32		setTETexGen(const U8 te, const U8 texgen);
	/*virtual*/ S32		setTEMediaTexGen(const U8 te, const U8 media);
	/*virtual*/ BOOL 	setMaterial(const U8 material);

				void	setTexture(const S32 face);

	/*virtual*/ BOOL	setVolume(const LLVolumeParams &volume_params, const S32 detail, bool unique_volume = false);
				void	sculpt();
				void	updateRelativeXform();
	/*virtual*/ BOOL	updateGeometry(LLDrawable *drawable);
	/*virtual*/ void	updateFaceSize(S32 idx);
	/*virtual*/ BOOL	updateLOD();
				void	updateRadius();
	/*virtual*/ void	updateTextures(LLAgent &agent);
				void	updateTextures();

				void	updateFaceFlags();
				void	regenFaces();
				BOOL	genBBoxes(BOOL force_global);
				void	preRebuild();
	virtual		void	updateSpatialExtents(LLVector3& min, LLVector3& max);
	virtual		F32		getBinRadius();
	
	virtual U32 getPartitionType() const;

	// For Lights
	void setIsLight(BOOL is_light);
	void setLightColor(const LLColor3& color);
	void setLightIntensity(F32 intensity);
	void setLightRadius(F32 radius);
	void setLightFalloff(F32 falloff);
	void setLightCutoff(F32 cutoff);
	BOOL getIsLight() const;
	LLColor3 getLightBaseColor() const; // not scaled by intensity
	LLColor3 getLightColor() const; // scaled by intensity
	F32 getLightIntensity() const;
	F32 getLightRadius() const;
	F32 getLightFalloff() const;
	F32 getLightCutoff() const;
	
	// Flexible Objects
	U32 getVolumeInterfaceID() const;
	virtual BOOL isFlexible() const;
	virtual BOOL isSculpted() const;
	BOOL isVolumeGlobal() const;
	BOOL canBeFlexible() const;
	BOOL setIsFlexible(BOOL is_flexible);
			
	// reX: new functions
	// Ogre update
	void updateOgrePosition();
	void updateOgreLight();
	void updateOgreMeshMaterials(bool force_update);
	void preloadOgreMeshMaterials();
	/*virtual*/ void updateOgreGeometry();
   /*virtual*/ void setDescriptionTexture(const std::string &description);
   virtual void onOgreAssetLoaded(LLAssetType::EType assetType, const LLUUID& uuid);
    const std::string& getRexClassName() const;
    
    RexPrimData::RexDrawType getRexDrawType() const;
	BOOL getRexIsMesh() const;
	BOOL getRexIsVisible() const;
	BOOL getRexCastShadows() const;
	BOOL getRexLightCastsShadows() const;
	BOOL getRexShowText() const;
	BOOL getRexScaleMesh() const;
	BOOL getRexSolidAlpha() const;
    BOOL getRexIsBillboard() const;
	int getRexSelectionPriority() const;
    F32 getRexLOD() const;
	F32 getRexDrawDistance() const;
    const LLUUID& getRexMeshID() const;
	const LLUUID& getRexCollisionMeshID() const;
    U16 getRexNumMaterials() const;
	const LLUUID& getRexMaterialID(U16 index) const;
	const RexMaterialDef& getRexMaterialDef(U16 index) const;
    const LLUUID& getRexAnimationPackID() const;
    const std::string& getRexAnimationName() const;
    F32 getRexAnimationRate() const;
    const LLUUID& getRexSoundID() const;
	const LLUUID& getRexSoundSourceID() const;
    F32 getRexSoundRadius() const;
    F32 getRexSoundVolume() const;
	FixedOgreMaterial getRexFixedMaterial() const;
	const LLUUID& getRexParticleScriptID() const;
	std::string getRexData();
	
    RexPrimData& getRexPrimData() { return mRexPrimData; }
    void onRexPrimDataChanged(bool local_origin);
    void sendRexPrimData();
    void setRexPrimDataReceived() { mRexPrimDataReceived = true; }

	void setRexClassName(const std::string& name, bool suppressSend = false);
    void setRexDrawType(RexPrimData::RexDrawType drawType, bool suppressSend = false);
	void setRexIsVisible(BOOL enabled, bool suppressSend = false);
	void setRexCastShadows(BOOL enabled, bool suppressSend = false);
	void setRexLightCastsShadows(BOOL enabled, bool suppressSend = false);
	void setRexShowText(BOOL enabled, bool suppressSend = false);
	void setRexScaleMesh(BOOL enabled, bool suppressSend = false);
	void setRexSolidAlpha(BOOL enabled, bool suppressSend = false);
	void setRexLOD(F32 lod, bool suppressSend = false);
	void setRexSelectionPriority(int priority, bool suppressSend = false);
    void setRexDrawDistance(F32 drawDistance, bool suppressSend = false);
	void setRexMeshID(const LLUUID& meshID, bool suppressSend = false);
	void setRexCollisionMeshID(const LLUUID& meshID, bool suppressSend = false);
	void setRexNumMaterials(U16 numMaterials, bool suppressSend = false);
	void setRexMaterialID(U16 index, const LLUUID& materialID, bool suppressSend = false);
	void setRexMaterialDef(U16 index, const RexMaterialDef& materialDef, bool suppressSend = false);
	void setRexFixedMaterial(FixedOgreMaterial material, bool suppressSend = false);
	void setRexParticleScriptID(const LLUUID& particleScriptID, bool suppressSend = false);
	void setRexAnimationPackID(const LLUUID& animationPackageID, bool suppressSend = false);
    void setRexAnimationName(const std::string& name, bool suppressSend = false);
    void setRexAnimationRate(F32 rate, bool suppressSend = false);
    void setRexSoundID(const LLUUID& soundID, bool suppressSend = false);
	void setRexSoundSourceID(LLUUID soundSourceID);
    void setRexSoundRadius(F32 radius, bool suppressSend = false);
    void setRexSoundVolume(F32 volume, bool suppressSend = false);
    void setRexData(std::string data);
	void createRexSoundSource(BOOL create);

   void setUseClonedMaterial(bool useCloned);

protected:
	S32	computeLODDetail(F32	distance, F32 radius);
	BOOL calcLOD();
	LLFace* addFace(S32 face_index);
	void updateTEData();
	// reX: new functions
	void updateOgreMesh();
   //! Update/create new billboard to represent this volume
   void updateOgreBillboard();
   //! Update particle systems of this object (if any)
   void updateOgreParticles();
   //! Update Ogre skeleton of this object (if any)
   void updateOgreSkeleton();
   //! Internal method, set particle system into use, or disable (null ID)
   void setParticleSystem(const LLUUID& particleScriptID);
   //! Internal method, set skeleton into use, or disable (null ID)
   void setOgreSkeleton(const LLUUID& skeletonID);
   //! Speed up loading of particle system textures
   void pumpParticleTextures();

public:
	LLViewerTextureAnim *mTextureAnimp;
	U8 mTexAnimMode;
private:
	friend class LLDrawable;
	
	BOOL		mFaceMappingChanged;
	LLFrameTimer mTextureUpdateTimer;
	S32			mLOD;
	BOOL		mLODChanged;
	S32         mSculptLevel;
	BOOL		mSculptChanged;
	LLMatrix4	mRelativeXform;
	LLMatrix3	mRelativeXformInvTrans;
	BOOL		mVolumeChanged;
	F32			mVObjRadius;
	LLVolumeInterface *mVolumeImpl;
	LLPointer<LLViewerImage> mSculptTexture;
	
	// rex: new member variables
    RexPrimData mRexPrimData;
	BOOL		mForceTextureAreaUpdate;
   std::string mCurrentTextureText;
   std::string mCurrentDescription;
   //! Material name for the new material that's created when something is written on the object
   std::string mMaterialNameText;	
	//! Textures for Ogre mesh display currently in use (legacy mode)
   std::vector<LLPointer<LLViewerImage> > mOgreTextures;
	//! Materials currently in use (material script mode)
   std::vector<RexOgreMaterial* > mOgreMaterials;
    //! Fullbright status for Ogre mesh
    BOOL mMeshFullbright;	
	//! Solidalpha status for Ogre mesh
	BOOL mMeshSolidAlpha;
    //! Current particle script in use
    LLUUID mCurrentParticleScriptID;
    //! Current skeleton in use
    LLUUID mCurrentSkeletonID;
    //! Particle update on first time geom.rebuild
    BOOL mParticleFirstTime;
    //! Ogre skeleton update on first time geom. rebuild
    BOOL mSkeletonFirstTime;
    //! Is skeleton updated and do we need to rebuild ogre mesh
    BOOL mSkeletonUpdate;
    //! If RexPrimData received, we should no longer use PARAMS_REX block
    bool mRexPrimDataReceived;

    //! If true, clone the Ogre material instead of using the actual material. Needed when per-object materials are needed.
    std::vector<bool> mUseClonedOgreMaterial;
    StringVector mCloneMaterialName;
    StringVector mCurrentMaterials;
    std::vector<bool> mRecreateClonedMaterial;

   //! Current fixed material type
   FixedOgreMaterial mFixedMaterial;

   // RexData
   std::string mRexData;
   
	// statics
public:
	static F32 sLODSlopDistanceFactor;// Changing this to zero, effectively disables the LOD transition slop 
	static F32 sLODFactor;				// LOD scale factor
	static F32 sDistanceFactor;			// LOD distance factor
		
protected:
	static S32 sNumLODChanges;
	
	friend class LLVolumeImplFlexible;
};

// reX: new class
//! http responder for writing description url to texture
class TextTextureHttpResponder : public LLHTTPClient::Responder
{
public:
   //! constructor
   TextTextureHttpResponder(LLVOVolume *node);
   //! destructor
   ~TextTextureHttpResponder();
   
   //! Parse the received content and handle it
   void result(const LLBufferStream &stream); 

   //! process http request result
	void completedRaw(U32 status, const std::string& reason, const LLChannelDescriptors& channels,
							const LLIOPipe::buffer_ptr_t& buffer);

private:
   //! Receiver for
   LLVOVolume* mNode;
};

#endif // LL_LLVOVOLUME_H
