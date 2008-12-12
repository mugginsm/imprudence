/** 
* @file lllandmarkcommon.cpp
* @author Vectorform
* @brief Common base for landmark edit/add/receive floaters
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
#include "llviewerprecompiledheaders.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llfloaterworldmap.h"
#include "llsdserialize.h"
#include "llgenericmultiplexer.h"
#include "lllandmarkcommon.h"


//////////////////////////////////////////////////////////////////////////
// common landmark floater data structure

// returns a common format for all landmark floaters
LLString LLFloaterCommonLandmark::LLLandData::getFormatedLandDetails() 
{
	return llformat(
		"Location: %s %d, %d, %d (%s)\n"
		"Traffic: %6d    Area: %6d sq. m.\n\n%s",
		mRegionName.c_str(), llround(mRegionPosition.mV[0]), llround(mRegionPosition.mV[1]), llround(mRegionPosition.mV[2]), mRating.c_str(),
		mTraffic, mArea, mDescription.c_str()
	);
}

//////////////////////////////////////////////////////////////////////////
// common landmark floater functionality

LLFloaterCommonLandmark::LLFloaterCommonLandmark( LLString name, LLString rectangle_ctrl, LLString title )
:	LLFloater(name,rectangle_ctrl,title), mParcelResponder( NULL ), mLandmark( NULL ),
	mEditorName( NULL ), mEditorDetails( NULL ), mSelectFolder( NULL ), mInventoryItem(NULL)
{

}

LLFloaterCommonLandmark::~LLFloaterCommonLandmark()
{

}

// called from LLPanelPlace::processParcelInfoReply() notifyObservers()
// which is indirectly called by the LLLandmarkParcelRequestResponder class which starts the async parcel info request
BOOL LLFloaterCommonLandmark::onObserverUpdate( LLParcelInfo& Data )
{
	if( Data.parcel_id != mParcelId )
	{
		llinfos << "LLFloaterReceiveLandmark::onObserverUpdate(); our parcel id = " << mParcelId << " got data for " << Data.parcel_id << " -- ignoring" << llendl;
		return FALSE;
	}

	// this duplicates some LLPanelPlace::processParcelInfoReply() functionality

	mLandData.mRating = LLViewerRegion::accessToString(SIM_ACCESS_PG);
	if (Data.flags & 0x1)
		mLandData.mRating = LLViewerRegion::accessToString(SIM_ACCESS_MATURE);

	std::string pos_string;
	gAgent.buildLocationString(pos_string);
	mLandData.mShortDescription = pos_string;

	// If the region position is zero, grab position from the global
	if( mLandData.mRegionPosition.isExactlyZero() )
	{
		mLandData.mRegionPosition.mV[0] = llround( Data.global_x ) % REGION_WIDTH_UNITS;
		mLandData.mRegionPosition.mV[1] = llround( Data.global_y ) % REGION_WIDTH_UNITS;
		mLandData.mRegionPosition.mV[2] = llround( Data.global_z );
	}

	mLandData.mSnapshotID = Data.snapshot_id;
	mLandData.mArea = Data.actual_area;
	mLandData.mDescription = Data.desc;
	mLandData.mTraffic = Data.dwell;
	mLandData.mRegionName = Data.sim_name;
	mLandData.mGlobalPosition = LLVector3d( Data.global_x, Data.global_y, Data.global_z );

	showLandInfo();

	return TRUE; // we got what we want, remove us from the parcel-info update list!
}


LLString LLFloaterCommonLandmark::getLandmarksMetaDataFile( LLUUID landmark_uuid )
{
	LLString filename = landmark_uuid.asString() + ".xml";
	LLString path = gDirUtilp->getLindenUserDir() + gDirUtilp->getDirDelimiter() + "Landmarks";

	// make sure we have a folder for storing notes and snapshots
	LLFile::mkdir( path.c_str() );

	return path + gDirUtilp->getDirDelimiter() + filename;
}

// loads a landmark's notes/snapshot data using its asset uuid
LLSD LLFloaterCommonLandmark::loadLandmarkMetaData( LLUUID landmark_uuid )
{
	llifstream fileIn;
	LLSD landmark_data;

	// add the filename onto the path
	LLString path = getLandmarksMetaDataFile( landmark_uuid );

	fileIn.open( path.c_str() );
	if( fileIn.is_open() )
		LLSDSerialize::fromXML( landmark_data, fileIn );
	fileIn.close();

	llinfos << "LLFloaterCommonLandmark::loadLandmarkMetaData(); path = " << path << " landmark_data = " << landmark_data << llendl;

	return landmark_data;
}

BOOL LLFloaterCommonLandmark::updateLandmarksMetaData( LLSD landmark_entry, LLUUID landmark_uuid )
{
	LLString path = getLandmarksMetaDataFile( landmark_uuid );

	BOOL saved = FALSE;

	llofstream file;
	file.open(path.c_str());
	if(file.is_open())
	{
		LLSDSerialize::toPrettyXML(landmark_entry, file);
		saved = TRUE;
	}
	file.close();

	llinfos << "LLFloaterCommonLandmark::updateLandmarksMetaData(); path = " << path << " landmark_entry = " << landmark_entry << llendl;

	return saved;
}

BOOL LLFloaterCommonLandmark::saveLandmarkMetaData()
{
	time_t date;
	time(&date);

	if( ! mInventoryItem )
	{
		llwarns << "LLFloaterCommonLandmark::saveLandmarkMetaData(); no inventory item" << llendl;
		return FALSE;
	}

	LLUUID landmark_uuid = mInventoryItem->getAssetUUID();

	// import the notes xml file
	LLSD landmark_entry = loadLandmarkMetaData( landmark_uuid );

	// update it with new data
	landmark_entry["landmark"]["notes"] = mEditorNotes->getText();
	landmark_entry["landmark"]["modifiedAt"] = ctime(&date);
	landmark_entry["landmark"]["snapshot"] = mSnapshotCtrl->getImageAssetID();

	// save it
	return updateLandmarksMetaData( landmark_entry, landmark_uuid );
}

void LLFloaterCommonLandmark::onReceiveParcelId( LLUUID parcel_id )
{
	// call parent to set parcel id
	LLLandmarkParcelInfoObserver::onReceiveParcelId( parcel_id );

	// LLLandmarkParcelRequestResponder object that called us will be deleted after we return
	// so reset the pointer to it
	mParcelResponder = NULL;
}

// teleport flyout menu options
void LLFloaterCommonLandmark::copyUrlToClipboard()
{
	LLURI slurl = build_sl_url( mLandData.mRegionName, mLandmark );
	gViewerWindow->mWindow->copyTextToClipboard( utf8str_to_wstring( slurl.asString() ) );

	// show copy slurl dialog
	LLString::format_map_t args;
	args["[SLURL]"] = slurl.asString();
	LLAlertDialog::showXml("CopySLURL", args);
}

void LLFloaterCommonLandmark::showOnMap()
{
	LLVector3d pos;
	mLandmark->getGlobalPos(pos);

	if(!pos.isExactlyZero())
	{
		gFloaterWorldMap->trackLocation(pos);
		LLFloaterWorldMap::show(NULL, TRUE);
	}
}

//////////////////////////////////////////////////////////////////////////
// folder selection functionality

// manage the folder selector over add/edit/receive landmark floaters
void LLFloaterCommonLandmark::openFolderSelector()
{
	closeFolderSelector();

	LLFloaterSelectFolder::FolderSelectCallback refreshFunctor = std::bind1st( std::mem_fun( &LLFloaterCommonLandmark::onFolderSelected ), this );
	LLFloaterSelectFolder::OnExitCallback exitFunctor = boost::bind( &LLFloaterCommonLandmark::onCloseFolderSelector, this );

	mSelectFolder = new LLFloaterSelectFolder();
	mSelectFolder->setOnFolderSelectCallback( refreshFunctor );
	mSelectFolder->setOnExitCallback( exitFunctor );
	mSelectFolder->centerWithin( getRect() );
}

void LLFloaterCommonLandmark::closeFolderSelector()
{
	if( mSelectFolder )
	{
		delete mSelectFolder;
		mSelectFolder = NULL;
	}
}

// event callback
void LLFloaterCommonLandmark::onCloseFolderSelector()
{
	mSelectFolder = NULL;
}

void LLFloaterCommonLandmark::onFolderSelected(LLFloaterSelectFolder::LLSelectedFolder Folder)
{
	// folder selector destroys itself after this callback, so clear the pointer to it
	mSelectFolder = NULL;

	if( Folder.folder_chosen )
	{	// user chose a folder (instead of hitting cancel)
		refreshCombo( Folder.folder_id );
	}
}

void LLFloaterCommonLandmark::refreshCombo( LLUUID selected_folder_uuid )
{
	// repopulate the combo
	populateCombo();

	// update selected folder
	mComboboxFolders->selectByValue( selected_folder_uuid );
}

// cleans combobox and starts off the recursive list building process
void LLFloaterCommonLandmark::populateCombo()
{
	LLInventoryCategory* root_landmark_folder = gInventory.getCategory( gInventory.findCategoryUUIDForType( LLAssetType::AT_LANDMARK ) );

	mComboboxFolders->removeall();
	mComboboxFolders->add( root_landmark_folder->getName(), root_landmark_folder->getUUID() );

	// default selection to the first item on the list
	mComboboxFolders->selectFirstItem();

	populateComboRecurse( root_landmark_folder );
}

// recursive function that adds sub-folders of the passed parameter and adds them into the combobox control
void LLFloaterCommonLandmark::populateComboRecurse( LLInventoryCategory* parentFolder )
{
	static int folderDepth = 0;
	folderDepth ++;

	LLString spaces = "   ";
	for( int i = 0; i < folderDepth; i ++ )
		spaces += "   ";

	LLInventoryModel::cat_array_t* folders;
	LLInventoryModel::item_array_t* items;

	gInventory.getDirectDescendentsOf( parentFolder->getUUID(), folders, items );

	if( ! items || ! folders )
	{
		llinfos << "LLFloaterAddLandmark::populateComboRecurse(); items = " << items << " folders = " << folders << llendl;
		return;
	}

	S32 count = folders->count();
	for( S32 i = count - 1; i >= 0 ; --i )
	{
		LLInventoryCategory* folder = folders->get(i);
		mComboboxFolders->add( spaces + folder->getName(), folder->getUUID() );

		// add any children
		populateComboRecurse( folder );
	}

	folderDepth --;
}

//static
void LLFloaterCommonLandmark::onClickDefaultSnapshot(void* userdata)
{
	LLFloaterCommonLandmark* self = static_cast<LLFloaterCommonLandmark*>(userdata);

	self->mSnapshotCtrl->setImageAssetID( self->mLandData.mSnapshotID );
	self->mButtonDefaultSnapshot->setVisible( FALSE );
}

void LLFloaterCommonLandmark::onChangeSnapshot(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterCommonLandmark* self = static_cast<LLFloaterCommonLandmark*>(userdata);

	bool have_new_snapshot = self->mLandData.mSnapshotID != self->mSnapshotCtrl->getImageAssetID();
	self->mButtonDefaultSnapshot->setVisible( have_new_snapshot );

	// snapshot was changed, landmark needs to be saved
	//self->mIsDirty = TRUE;
}

// derived classes should override this
BOOL LLFloaterCommonLandmark::isDirty()
{
	BOOL isDirty = FALSE;

	bool have_new_snapshot = mLandData.mSnapshotID != mSnapshotCtrl->getImageAssetID();


	isDirty = have_new_snapshot;
	return isDirty;
}