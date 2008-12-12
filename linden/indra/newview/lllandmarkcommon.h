/**
 * @file lllandmarkcommon.h
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008, Linden Research, Inc.
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

#ifndef LL_LANDMARKCOMMON_H
#define LL_LANDMARKCOMMON_H

#include "llgenericmultiplexer.h"

#include "llhttpclient.h"
#include "lluuid.h"
#include "llsdutil.h"
#include "lllandmark.h"
#include "llagent.h"
#include "llviewerregion.h"
#include "llurldispatcher.h"
#include "lllineeditor.h"
#include "lltexteditor.h"
#include "llcombobox.h"
#include "llfloaterselectfolder.h"

template< class T>
class LLLandmarkFloaterFactory : public LLSingleton< LLLandmarkFloaterFactory<T> >
{
public:

	T* createFloater()
	{
		T* floater = new T();
		registerFloater( floater );
		return floater;
	}

	void registerFloater( T* floater ) {
		mFloaters.push_back( floater );
	}

	void removeFloater( T* floater ) {
		mFloaters.remove( floater );
	}

	int numOfOpenFloaters() {
		return mFloaters.size();
	}

	typename std::list<T*>::iterator getIterStart() {
		return mFloaters.begin();
	}

	typename std::list<T*>::iterator getIterEnd() {
		return mFloaters.end();
	}

	typedef typename std::list<T*>::iterator iterator;

protected:

	std::list<T*> mFloaters;
};

std::string build_sl_url( std::string region_name, LLLandmark* landmark );

// parcel info as parsed by LLPanelPlace::processParcelInfoReply()
struct LLParcelInfo
{
	LLUUID		agent_id;
	LLUUID		parcel_id;
	LLUUID		owner_id;
	std::string	name;
	std::string	desc;
	S32			actual_area;
	S32			billable_area;
	U8			flags;
	F32			global_x;
	F32			global_y;
	F32			global_z;
	std::string	sim_name;
	LLUUID		snapshot_id;
	F32			dwell;
	S32			sale_price;
	S32			auction_id;
};

// edit/add/receive landmark floaters derive from this class to interact with LLLandmarkParcelRequestResponder
// and receive appropriate parcel info data
class LLLandmarkParcelInfoObserver : public LLVFGenericObserver<LLParcelInfo>
{
public:

	LLLandmarkParcelInfoObserver()
	{
		mParcelId = LLUUID::null;
	}

	virtual ~LLLandmarkParcelInfoObserver()
	{
	}

	virtual void onReceiveParcelId( LLUUID parcel_id )
	{
		mParcelId = parcel_id;
	}

protected:

	LLUUID mParcelId;
};

class LLLandmarkParcelRequestResponder : public LLHTTPClient::Responder
{
	LLLandmarkParcelInfoObserver* mObserver;

public:

	LLLandmarkParcelRequestResponder( LLLandmarkParcelInfoObserver* InfoTarget, LLLandmark* Landmark, const LLUUID& LandmarkAssetID )
	{
		LLUUID region_id;
		Landmark->getRegionID(region_id);

		LLVector3d global_position;
		Landmark->getGlobalPos(global_position);

		LLVector3 region_position = Landmark->getRegionPos();

		mObserver = InfoTarget;

		// now start off the request for the ParcelInfo by first acquiring the ParcelId
		// and then kicking off the actual request
		startParcelInfoRequest( region_position, region_id, global_position );
	}

	LLLandmarkParcelRequestResponder( LLLandmarkParcelInfoObserver* InfoTarget, LLVector3d global_position )
	{
		LLUUID region_id;
		LLVector3 region_position = LLVector3::zero;

		mObserver = InfoTarget;

		// now start off the request for the ParcelInfo by first acquiring the ParcelId
		// and then kicking off the actual request
		startParcelInfoRequest( region_position, region_id, global_position );
	}

	~LLLandmarkParcelRequestResponder()
	{
	}

	LLVector3 regionPositionFromGlobalPosition( LLVector3d global_position )
	{
		S32 local_x = (int) global_position.mdV[0] % REGION_WIDTH_UNITS;
		S32 local_y = (int) global_position.mdV[1] % REGION_WIDTH_UNITS;
		if (local_x < 0)
			local_x += REGION_WIDTH_UNITS;
		if (local_y < 0)
			local_y += REGION_WIDTH_UNITS;

		LLVector3 local_pos;
		local_pos.mV[VX] = (F32)local_x;
		local_pos.mV[VY] = (F32)local_y;
		local_pos.mV[VZ] = global_position.mdV[2];

		return local_pos;
	}

	void startParcelInfoRequest( LLVector3 pos_region,
		const LLUUID& region_id,
		const LLVector3d& pos_global)
	{
		LLSD body;

		std::string url = gAgent.getRegion()->getCapability("RemoteParcelRequest");

		llinfos << "LLLandmarkParcelRequestResponder::startParcelInfoRequest(); url = " << url << llendl;

		if (!url.empty())
		{
			// sometimes we dont have the region position
			// we can build it using the global position
			if( pos_region.isExactlyZero() && ! pos_global.isExactlyZero() )
				pos_region = regionPositionFromGlobalPosition( pos_global );

			body["location"] = ll_sd_from_vector3(pos_region);
			if (!region_id.isNull())
			{
				body["region_id"] = region_id;
			} else
			if (!pos_global.isExactlyZero())
			{
				U64 region_handle = to_region_handle(pos_global);
				body["region_handle"] = ll_sd_from_U64(region_handle);
			}

			llinfos << "LLLandmarkParcelRequestResponder::startParcelInfoRequest(); region_id = " << region_id << llendl;
			llinfos << "LLLandmarkParcelRequestResponder::startParcelInfoRequest(); pos_global = " << pos_global << llendl;

			llinfos << "LLLandmarkParcelRequestResponder::startParcelInfoRequest(); body = " << body << llendl;

			// after the post reply comes in with the parcelid, send out a ParcelInfoRequest
			LLHTTPClient::post(url, body, this);
		}
		else
		{
			llwarns << "LLLandmarkParcelRequestResponder::startParcelInfoRequest(); no remote parcel request url found" << llendl;
		}
	}

	// this gets called by LLHTTPClient as a response to our post requesting the ParcelId for the specified landmark
	// LLHTTPClient also deletes this object after calling result()
	// so we will have to detach the observer else where
	virtual void result(const LLSD& content)
	{
		LLUUID parcel_id = content["parcel_id"];
		llinfos << "LLLandmarkParcelRequestResponder::result(); content[\"parcel_id\"] = " << content["parcel_id"] << llendl;

		// tell our landmark floater the parcel id
		// so that it can filter the results of this reply
		// we cant do it because this object will be deleted after the function returns :(
		if( mObserver )
		{
			mObserver->onReceiveParcelId( parcel_id );

			// hook into the LLPanelPlace subject to receive ParcelInfo responses (to the message we send out next)
			LLVFGenericMultiplexer<LLParcelInfo>::getInstance()->addObserver( mObserver );
		} else
		{
			llinfos << "LLLandmarkParcelRequestResponder::result(); looks like the floater peaced-out; aborting parcel info request" << llendl;
			return;
		}

		// send out a parcel info request
		// LLPanelPlace hooks into the message system and receives responses to parcel info requests
		// we have a subject setup in LLPanelPlace to which we attach to get back our reply
		LLMessageSystem *msg = gMessageSystem;
		msg->newMessage("ParcelInfoRequest");
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("ParcelID", parcel_id);
		gAgent.sendReliableMessage();

		llinfos << "LLLandmarkParcelRequestResponder::result(); sent request for parcel info" << llendl;
	}

	// error getting parcel id from server
	virtual void error(U32 status, const std::string& reason)
	{
		llwarns << "LLLandmarkParcelRequestResponder::error(" << status << ": " << reason << ")" << llendl;
	}

	// used to explicitly nullify the observer when closing the observer before this object is finished doing
	// all the round trips to the server (needed for cases like when a server cant reply within a few seconds)
	void setObserver( LLLandmarkParcelInfoObserver* observer ) {
		llinfos << "LLLandmarkParcelRequestResponder::setObserver(); new observer = " << observer << llendl;
		mObserver = observer;
	}
};

class LLFloaterCommonLandmark : public LLFloater, public LLLandmarkParcelInfoObserver
{
public:

	// general data that we show on our add/edit/receive landmark floaters
	struct LLLandData
	{
		std::string mRating;
		LLVector3 mRegionPosition;
		LLVector3d mGlobalPosition;
		std::string mRegionName;
		LLUUID mSnapshotID;
		int mTraffic, mArea; // area in square miles
		std::string mDescription;
		std::string mShortDescription;

		// returns a common format for all landmark floaters
		std::string getFormatedLandDetails();
	};

	LLFloaterCommonLandmark( std::string name, std::string rectangle_ctrl, std::string title );
	virtual ~LLFloaterCommonLandmark();

	// called from LLPanelPlace::processParcelInfoReply() notifyObservers()
	// which is indirectly called by the LLLandmarkParcelRequestResponder class which starts the async parcel info request
	virtual BOOL onObserverUpdate( LLParcelInfo& Data );

	// overrides LLLandmarkParcelInfoObserver::onReceiveParcelId()
	// received from LLLandmarkParcelRequestResponder::result()
	// after this call returns, the caller object will be destroyed
	// so we have to make sure we release the pointer to it
	// we keep a pointer to LLLandmarkParcelRequestResponder() in order to unsubscribe from it if the user
	// closes a floater before mParcelResponder's result() callback is called
	virtual void onReceiveParcelId( LLUUID parcel_id );

	// manage the folder selector over add/edit/receive landmark floaters
	virtual void openFolderSelector();
	virtual void closeFolderSelector();

	// the methods that need to be implemented in any derived class
	virtual void toggleInteractiveContent( BOOL Enable ) = 0;
	virtual void showLandInfo() = 0;

	virtual void refreshCombo( LLUUID selected_folder_uuid );
	virtual void populateCombo();

	// gets filename of the client-side metadata file for this landmark
	std::string getLandmarksMetaDataFile( LLUUID landmark_uuid );

	virtual LLSD loadLandmarkMetaData( LLUUID landmark_uuid );
	virtual BOOL updateLandmarksMetaData( LLSD landmark_entry, LLUUID landmark_uuid );
	virtual BOOL saveLandmarkMetaData();

	// teleport flyout menu functions
	void copyUrlToClipboard();
	void showOnMap();

	void setLandmark(LLLandmark* landmark) {
		mLandmark = landmark;
	}

	void setInventoryItem(LLInventoryItem *item) {
		mInventoryItem = item;
	}

	// determine if any landmark info was changed
	virtual BOOL isDirty();

protected:

	// event callbacks
	virtual void onFolderSelected(LLFloaterSelectFolder::LLSelectedFolder);
	virtual void onCloseFolderSelector();
	
	// snapshot callbacks
	static void onClickDefaultSnapshot(void* data);
	static void onChangeSnapshot(LLUICtrl* ctrl, void* userdata);

	virtual void populateComboRecurse( LLInventoryCategory* parentFolder );

	LLLandData mLandData;
	LLLandmark* mLandmark; // used by edit/add dialogs, not so much by the received dialog
	LLInventoryItem* mInventoryItem;

	// fetches any data needed for the landmark
	LLLandmarkParcelRequestResponder* mParcelResponder;

	// for choosing what folder to save a landmark to
	LLFloaterSelectFolder* mSelectFolder;

	// common landmark editor controls
	LLLineEditor* mEditorName;
	LLTextEditor* mEditorDetails;

	LLComboBox* mComboboxFolders;
	LLTextureCtrl* mSnapshotCtrl;
	LLTextEditor* mEditorNotes;
	LLButton* mButtonDefaultSnapshot;
};

#endif

