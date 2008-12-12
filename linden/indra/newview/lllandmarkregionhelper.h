/** 
* @file llfloateraddlandmark.cpp
* @author Vectorform
* @brief Floater to add your landmarks

* $LicenseInfo:firstyear=2001&license=viewergpl$
* 
* Copyright (c) 2001-2008, Linden Research, Inc.
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
* online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#ifndef LL_LANDMARKREGIONHELPER_H
#define LL_LANDMARKREGIONHELPER_H

#include "llworld.h"

//////////////////////////////////////////////////////////////////////////

class LLLandmarkRegionHelper : public LLRegionHandleCallback, public LLSingleton<LLLandmarkRegionHelper>
{
public:

	LLLandmarkRegionHelper() {}
	virtual ~LLLandmarkRegionHelper() {}

	// signature of the final callback
	typedef void (*LLGetAssetAndRegionCallback)( LLLandmark* landmark, LLInventoryItem* item, U64 region_handle );

	// the main function to call
	// fetches landmark from server if needed
	// then fetches the region data
	// then calls the callback with a landmark, region handle, and inventory item
	void fetchLandmarkAndRegionData( LLInventoryItem* inventory_item, LLGetAssetAndRegionCallback callback )
	{
		gAssetStorage->getAssetData( inventory_item->getAssetUUID(), LLAssetType::AT_LANDMARK, onLandmarkFetchCompleteWrapper, new LLMetaData( callback, inventory_item ) );
	}

	// not guaranteed to return a valid landmark
	LLLandmark* getLandmarkFromRegionHandle( U64 region_handle )
	{
		std::map<U64, LLLandmark*>::iterator landmark_finder = mRegionHandleToLandmark.find( region_handle );
		if( landmark_finder == mRegionHandleToLandmark.end() ) {
			llerrs << "LLLandmarkRegionHelper::onFetchRegionInfo(); can't find landmark for this region handle!" << llendl;
		}

		LLLandmark* landmark = landmark_finder->second;

		if( ! landmark ) {
			llwarns << "LLLandmarkRegionHelper::getLandmarkFromRegionHandle(); error getting landmark from region handle: " << region_handle << llendl;
		}

		return landmark;
	}

protected:

	struct LLMetaData
	{
		LLMetaData( LLGetAssetAndRegionCallback callback, LLInventoryItem* item )
		{
			InventoryItem = item;
			Callback = callback;
		}

		LLInventoryItem* InventoryItem;
		LLGetAssetAndRegionCallback Callback;
	};

	// stores data callbacks need (like inventory item and the actual callback address)
	std::map<LLLandmark*,LLMetaData*> mMetaData;

	// primarily serves to avoid race conditions when the final static callback is called
	// we dont really want to cache landmark info HERE, the LLandmark/LLWorldMap systems do that intelligently for us
	std::map<LLUUID, LLLandmark*> mRegionIdToLandmark;
	std::map<U64, LLLandmark*> mRegionHandleToLandmark;

	// not guaranteed to return a valid landmark
	LLLandmark* getLandmarkFromRegionId( LLUUID region_id )
	{
		std::map<LLUUID, LLLandmark*>::iterator landmark_finder = mRegionIdToLandmark.find( region_id );
		if( landmark_finder == mRegionIdToLandmark.end() ) {
			llerrs << "LLLandmarkRegionHelper::getLandmarkFromRegionId(); can't find landmark for this region id!" << llendl;
		}

		LLLandmark* landmark = landmark_finder->second;
		return landmark;
	}

	void onRegionDataFetchComplete(U64 region_handle, const std::string& url, const LLUUID& snapshot_id, bool teleport)
	{
		LLLandmark* landmark = getLandmarkFromRegionHandle( region_handle );
		if( ! landmark )
		{
			llerrs << "LLLandmarkRegionHelper::onRegionDataFetchComplete(); could not get region handle!" << llendl;
		}

		std::map<LLLandmark*,LLMetaData*>::iterator metadata_finder = mMetaData.find( landmark );
		if( metadata_finder == mMetaData.end() )
		{
			llerrs << "LLLandmarkRegionHelper::dataReady(); can't find meta data associated with landmark: " << landmark << llendl;
		}

		LLMetaData* associated_data = metadata_finder->second;

		if( ! associated_data->Callback )
		{
			llerrs << "LLLandmarkRegionHelper::dataReady(); no callback in metadata" << llendl;
		}

		associated_data->Callback( landmark, associated_data->InventoryItem, region_handle );
	}

	// this is a callback given to getAssetData()
	// the inventory item will contain a region id, but might not have position data (as per version 1 of the message protocol)
	// once the asset/landmark is loaded, the region data fetch begins
	void onLandmarkFetchComplete( LLVFS *vfs, const LLUUID& uuid, LLAssetType::EType type, void* user_data, S32 status, LLExtStat ext_status )
	{
		// let the data be processed by the landmark list
		LLLandmarkList::processGetAssetReply( vfs, uuid, type, user_data, status, ext_status );

		LLMetaData* MetaData = static_cast<LLMetaData*>( user_data );

		// save some of the inventory item data
		LLUUID item_id = MetaData->InventoryItem->getAssetUUID();
		LLLandmark* landmark = gLandmarkList.getAsset( item_id );
		LLUUID region_id;

		// store the meta data
		LLLandmarkRegionHelper::getInstance()->mMetaData[landmark] = MetaData;

		// if we have a valid global position, use it to get the region handle
		// and then get 
		LLVector3d global_pos;
		if( landmark->getGlobalPos( global_pos ) && ! global_pos.isExactlyZero() )
		{
			U64 region_handle = to_region_handle( global_pos );

			mRegionHandleToLandmark[region_handle] = landmark;

			llinfos << "LLLandmarkRegionHelper::onLandmarkFetchComplete(); already have region_handle = " << std::hex << region_handle << " and global pos = " << std::dec << global_pos << llendl;

			if( region_handle ) {
				LLWorldMap::getInstance()->sendHandleRegionRequest( region_handle, onRegionDataFetchCompleteWrapper, "llandmark region helper", false );
			} else {
				llerrs << "LLLandmarkRegionHelper::onLandmarkFetchComplete(); could not get region handle! bad landmark?" << llendl;
			}				
		}
		// otherwise see if we can use the region id to lookup the region handle
		else if( landmark->getRegionID( region_id ) )
		{	// if we have a valid region id, request the region handle
			mRegionIdToLandmark[region_id] = landmark;

			// start a region handle request, will call dataReady() when complete
			LLLandmark::requestRegionHandle( gMessageSystem, gAgent.getRegionHost(), region_id, this );
		}
		else
		{
			llerrs << "LLLandmarkRegionHelper::onLandmarkFetchComplete(); landmark regionid and global pos are unknown! bad landmark?" << llendl;
		}
	}

	LLMetaData* getMetaDataFor( LLLandmark* landmark )
	{
		std::map<LLLandmark*,LLMetaData*>::iterator metadata_finder = mMetaData.find( landmark );
		if( metadata_finder == mMetaData.end() )
		{
			llwarns << "LLLandmarkRegionHelper::getMetaDataFor(" << landmark << "); can't find meta data associated this landmark." << llendl;
			return NULL;
		}

		LLMetaData* associated_data = metadata_finder->second;
		return associated_data;
	}

	// callback for LLLandmark::requestRegionHandle()
	// initiates a get region data request
	virtual bool dataReady( const LLUUID& region_id, const U64& region_handle )
	{
		// we HAD region_id and now we have region_handle
		LLLandmark* landmark = getLandmarkFromRegionId( region_id );
		if( ! landmark )
		{
			llerrs << "LLLandmarkRegionHelper::dataReady(); failed to get landmark for region: " << region_handle << llendl;
		}

		// make the landmark searchable by region_handle too
		mRegionHandleToLandmark[region_handle] = landmark;

		std::map<LLLandmark*,LLMetaData*>::iterator metadata_finder = mMetaData.find( landmark );
		if( metadata_finder == mMetaData.end() )
		{
			llerrs << "LLLandmarkRegionHelper::dataReady(); can't find meta data associated with landmark: " << landmark << llendl;
		}

		LLWorldMap::getInstance()->sendHandleRegionRequest( region_handle, onRegionDataFetchCompleteWrapper, "LLLandmarkRegionHelper", false );

		return true;
	}

	// callback wrappers
	static void onRegionDataFetchCompleteWrapper(U64 region_handle, const std::string& url, const LLUUID& snapshot_id, bool teleport) {
		LLLandmarkRegionHelper::getInstance()->onRegionDataFetchComplete( region_handle, url, snapshot_id, teleport );
	}

	static void onLandmarkFetchCompleteWrapper( LLVFS *vfs, const LLUUID& uuid, LLAssetType::EType type, void* user_data, S32 status, LLExtStat ext_status ) {
		LLLandmarkRegionHelper::getInstance()->onLandmarkFetchComplete( vfs, uuid, type, user_data, status, ext_status );
	}

	friend class LLWorldMap;
};

#endif
