/** 
* @file LLFloaterReceiveLandmark.cpp
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


#include "llviewerprecompiledheaders.h"

#include "llfloater.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llinventorymodel.h"
#include <sstream>
#include <time.h>
#include "llfloaterland.h"
#include "llsdserialize.h"
#include "llfloaterselectfolder.h"
#include "llworldmap.h"
#include "llfloaterworldmap.h"
#include "llappviewer.h"
#include "llfloaterreceivelandmark.h"
#include "lllandmarklist.h"
#include "llfloatereditlandmark.h"
#include "lllandmarkcommon.h"

const std::string LL_RECEIVE_LANDMARK_FLOATER_FOLDER_FILE_NAME("Folders.xml");
const std::string LL_RECEIVE_LANDMARK_FLOATER_LINEEDITOR_NAME("Name");
const std::string LL_RECEIVE_LANDMARK_FLOATER_TEXTURE_SNAPSHOT("snapshot_ctrl");
const std::string LL_RECEIVE_LANDMARK_FLOATER_COMBOBOX_FOLDER("landmarks_combobox");
const std::string LL_RECEIVE_LANDMARK_FLOATER_TEXT_NOTES("Mynotes");
const std::string LL_RECEIVE_LANDMARK_FLOATER_TEXT_LOCATION("location_value");
const std::string LL_RECEIVE_LANDMARK_FLOATER_TEXT_TRAFFIC("traffic_value");
const std::string LL_RECEIVE_LANDMARK_FLOATER_TEXT_AREA("area_value");
const std::string LL_RECEIVE_LANDMARK_FLOATER_TEXT_DESCRIPTION("description_value");
const std::string LL_RECEIVE_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT("default_snapshot");
const std::string LL_RECEIVE_LANDMARK_FLOATER_MENU_ITEM("Copy SLURL");
const std::string LL_RECEIVE_LANDMARK_FLOATER_MAP_ITEM("Show on Map");

const char *SNAPSHOT_FILE_NAME = "location";
const char *FILE_EXTN = ".bmp";

LLParcelSelectionObserver* LLFloaterReceiveLandmark::sObserver = NULL;

BOOL LLFloaterReceiveLandmark::sLandmarkAdded = FALSE;

// callback for when the landmark is finally received
// can be used to scroll to and highlight item in landmark manager, etc
class LLReceivedLandmarkCallback : public LLInventoryCallback
{
public:
	
	LLReceivedLandmarkCallback() {}

	void fire(const LLUUID &inventory_item_uuid) {
		llinfos << "LLReceivedLandmarkCallback::fire(); our landmark was added to the inventory! " << inventory_item_uuid << llendl;

	}
};


LLFloaterReceiveLandmark::LLFloaterReceiveLandmark()
:	LLFloaterCommonLandmark("floater_recv_landmark", "ReceiveLandmarkRect", "")
{
	
}

LLFloaterReceiveLandmark::~LLFloaterReceiveLandmark()
{
	LLVFGenericMultiplexer<LLParcelInfo>::getInstance()->removeObserver( this );
	LLLandmarkFloaterFactory<LLFloaterReceiveLandmark>::getInstance()->removeFloater( this );

	if(mInventoryItem)
	{
		gInventory.deleteObject(mInventoryItem->getUUID());
		mInventoryItem = NULL;
	}
}

// starts building the floater view model after its been configured
void LLFloaterReceiveLandmark::startBuilding()
{
	// postBuild() will get called from within buildFloater()
	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_receive_landmark.xml");
}

const std::string LL_RECEIVE_LANDMARK_FLOATER_BUTTON_SAVE( "save" );
const std::string LL_RECEIVE_LANDMARK_FLOATER_BUTTON_CANCEL( "cancel" );

// inventory item and landmark must be set before receive landmark gets built
BOOL LLFloaterReceiveLandmark::postBuild()
{
	mComboboxFolders = getChild<LLComboBox>(LL_RECEIVE_LANDMARK_FLOATER_COMBOBOX_FOLDER);
	mEditorName = getChild<LLLineEditor>(LL_RECEIVE_LANDMARK_FLOATER_LINEEDITOR_NAME);
	mEditorDetails = getChild<LLTextEditor>(LL_RECEIVE_LANDMARK_FLOATER_TEXT_DESCRIPTION);
	mSnapshotCtrl = getChild<LLTextureCtrl>( LL_RECEIVE_LANDMARK_FLOATER_TEXTURE_SNAPSHOT );
	mEditorNotes = getChild<LLTextEditor>(LL_RECEIVE_LANDMARK_FLOATER_TEXT_NOTES);
	mButtonDefaultSnapshot = getChild<LLButton>(LL_RECEIVE_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT);

	setTitle( "Receiving Landmark.." );

	childSetAction(LL_RECEIVE_LANDMARK_FLOATER_TEXTURE_SNAPSHOT, onClickTakeSnapshot, this);
	childSetAction("create_folder", onClickCreateFolder, this);
	childSetCommitCallback("teleport", onSelectItem, this);

	mSaved = FALSE;

	childSetAction(LL_RECEIVE_LANDMARK_FLOATER_BUTTON_SAVE, onClickSave, this);
	childSetAction(LL_RECEIVE_LANDMARK_FLOATER_BUTTON_CANCEL, onClickCancel, this);
	childSetAction(LL_RECEIVE_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, onClickDefaultSnapshot, this);
	childSetVisible(LL_RECEIVE_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, FALSE);

	// setup default config for the snapshot control
	mSnapshotCtrl->setCommitCallback( onChangeSnapshot );
	mSnapshotCtrl->setCallbackUserData( this );
	mSnapshotCtrl->setImageAssetID( LLUUID::null );

	setTitle("Receive Landmark: Downloading Landmark..");
	toggleInteractiveContent( FALSE );
	populateCombo();

	new LLLandmarkParcelRequestResponder( this, mLandmark, mInventoryItem->getAssetUUID() );

	centerWithin(gViewerWindow->getRootView()->getRect());

	LLUUID folder_id;
	folder_id = gInventory.findSubCatUUID(LLAssetType::AT_RECEIVE_LANDMARK);
	if( folder_id.isNull())
		{
			LLUUID root_id_received = gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);
			if(root_id_received.notNull())
				{
					folder_id = gInventory.createNewCategory(root_id_received, LLAssetType::AT_RECEIVE_LANDMARK, "Received Landmarks");
					LLViewerInventoryCategory* item = gInventory.getCategory(folder_id);
					gInventory.updateCategory(item);
					gInventory.notifyObservers();
					populateCombo();
				}
   		}	
			
	mComboboxFolders->selectByValue(folder_id);
	
	return TRUE;
}

void LLFloaterReceiveLandmark::toggleInteractiveContent( BOOL Enable )
{
	childSetEnabled( LL_RECEIVE_LANDMARK_FLOATER_LINEEDITOR_NAME, Enable );
	childSetEnabled( LL_RECEIVE_LANDMARK_FLOATER_BUTTON_SAVE, Enable );
	childSetEnabled( LL_RECEIVE_LANDMARK_FLOATER_BUTTON_CANCEL, Enable );
	childSetEnabled( LL_RECEIVE_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, Enable );
	childSetEnabled( LL_RECEIVE_LANDMARK_FLOATER_TEXTURE_SNAPSHOT, Enable );
	childSetEnabled( LL_RECEIVE_LANDMARK_FLOATER_TEXT_NOTES, Enable );
	childSetEnabled( LL_RECEIVE_LANDMARK_FLOATER_COMBOBOX_FOLDER, Enable );

}

// static
void LLFloaterReceiveLandmark::onGetNewLandmarkInfo( LLVFS *vfs, const LLUUID& uuid, LLAssetType::EType type, void* user_data, S32 status, LLExtStat ext_status )
{
	llinfos << "LLFloaterReceiveLandmark::onGetNewLandmarkInfo(); calling processGetAssetReply().." << llendl;
	
	LLLandmarkList::processGetAssetReply( vfs, uuid, type, user_data, status, ext_status );

	LLLandmark* landmark = gLandmarkList.getAsset( uuid );

	llinfos << "LLFloaterReceiveLandmark::onGetNewLandmarkInfo(); landmark = " << landmark << llendl;

	if( landmark )
	{
		LLViewerInventoryItem* inventoryItem = (LLViewerInventoryItem*) user_data;

		llinfos << "LLFloaterReceiveLandmark::onGetNewLandmarkInfo(); got landmark '" << inventoryItem->getName() << "' showing receive-landmark floater with inventoryItem = " << inventoryItem << llendl;

		// spawn a new floater
		LLFloaterReceiveLandmark* floater = LLLandmarkFloaterFactory<LLFloaterReceiveLandmark>::getInstance()->createFloater();

		// configure it
		floater->setInventoryItem( inventoryItem );
		floater->setLandmark( landmark );
		
		// now begin building the floater
		floater->startBuilding();
	}
}

void LLFloaterReceiveLandmark::showLandInfo()
{
	mEditorName->setText( mInventoryItem->getName() );
	mEditorDetails->setText(mLandData.getFormatedLandDetails());

	// hide the "restore default image" button
	childSetVisible(LL_RECEIVE_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, FALSE);

	if(mLandData.mSnapshotID.notNull())
	{
		mSnapshotCtrl->setImageAssetID(mLandData.mSnapshotID);
	}

	setTitle( "Receive Landmark" );
	toggleInteractiveContent( TRUE );
}

#include "llinventorybridge.h"

void LLFloaterReceiveLandmark::onClickSave(void *data)
{
	LLFloaterReceiveLandmark *self = (LLFloaterReceiveLandmark*)data;

	if( self->mSaved )
		return;

	//self->mSafeToClose = TRUE;

	self->setTitle( "Receive Landmark: Saving.." );
	self->toggleInteractiveContent( FALSE );

	if( ! gAgent.getAvatarObject())
		return;

	std::string location_name = self->mEditorName->getText();
	if( location_name == "" )
		location_name = self->mLandData.mRegionName;

	LLUUID folder_id = self->mComboboxFolders->getSelectedValue().asUUID();
	
	

	// update current landmark image
	self->mLandData.mSnapshotID = self->mSnapshotCtrl->getImageAssetID();
	
	std::string pos_string;
	gAgent.buildLocationString(pos_string);

	LLPointer<LLInventoryCallback> on_landmark_added = new LLReceivedLandmarkCallback();

	copy_inventory_item(gAgent.getID(), self->mInventoryItem->getPermissions().getOwner(), self->mInventoryItem->getUUID(),
						folder_id, location_name, on_landmark_added);

	LLViewerInventoryItem* viewer_item = dynamic_cast<LLViewerInventoryItem*>( self->mInventoryItem );
	if( ! viewer_item )
	{
		llwarns << "LLFloaterReceiveLandmark::onClickSave(); invalid inventory item = " << llendl;
		return;
	}

	// writes out the notes/snapshot data to a local folder
	self->saveLandmarkMetaData();

	LLDynamicArray<LLUUID> folders_to_delete;
	LLDynamicArray<LLUUID> items_to_delete;
	items_to_delete.push_back( self->mInventoryItem->getUUID() );

	gInventory.deleteFromServer( folders_to_delete, items_to_delete );
	gInventory.deleteObject(self->mInventoryItem->getUUID());
	
	LLInventoryView* view = LLInventoryView::getActiveInventory();
	if(view)
	{
		view->getPanel()->setSelection(folder_id,TRUE);
	}


	self->mSaved = TRUE;
	self->close();

//	std::string::format_map_t args;
//	gViewerWindow->alertXml("SaveSuccess", args);

	/* // this should be deleted, only here for reference
	if(gAgent.getAvatarObject())
	{
		LLLineEditor* name =  self->getChild<LLLineEditor>(LL_RECEIVE_LANDMARK_FLOATER_LINEEDITOR_NAME);
		std::string locationName = name->getText();

		LLComboBox* folder = self->getChild<LLComboBox>(LL_RECEIVE_LANDMARK_FLOATER_COMBOBOX_FOLDER);
		LLUUID folder_id = folder->getSelectedValue().asUUID();

		if(folder_id==LLUUID::null)
		{
			folder_id= gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);
		}

		LLTextEditor* notes = self->getChild<LLTextEditor>(LL_RECEIVE_LANDMARK_FLOATER_TEXT_NOTES);
		std::string notes_val = notes->getText();
		LLTextureCtrl* snapShot = self->getChild<LLTextureCtrl>("snapshot_ctrl");

		llinfos << "LLFloaterReceiveLandmark::onClickSave(); notes = " << notes_val << llendl;

		LLViewerInventoryCategory* object = gInventory.getCategory(folder_id);			
		std::string pos_string;
		gAgent.buildLocationString(pos_string);	
		
		if(object)
		{
			std::string title = "Landmark";
			LLPointer<LLInventoryCallback> cb = new LandmarkCallback(title);
			copy_inventory_item(gAgent.getID(), self->currentItem->getPermissions().getOwner(), self->currentItem->getUUID(),object->getUUID(),locationName,cb);

			self->mSaved = TRUE;
			self->mCancel = FALSE;

			llinfos << "LLFloaterReceiveLandmark::onClickSave(); currentItem->getAssetUUID() = " << self->currentItem->getAssetUUID() << llendl;

			sLandmarkAdded = FALSE;	
			self->writeToFile(self->currentItem->getAssetUUID().asString(), locationName, notes_val, snapShot->getImageAssetID(), TRUE);
			gInventory.deleteObject(self->currentItem->getUUID());
			self->close();
			
			std::string::format_map_t args;
			gViewerWindow->alertXml("SaveSuccess", args);
		}
	}
	*/

	
}

void LLFloaterReceiveLandmark::onClickTakeSnapshot(void* data)
{
	LLViewerRegion* region = gAgent.getRegion();
	std::string location = region->getName();

	std::string snap_filename = gDirUtilp->getLindenUserDir() + gDirUtilp->getDirDelimiter() + SNAPSHOT_FILE_NAME + location + FILE_EXTN;
	gViewerWindow->saveSnapshot(snap_filename, 200, 200, FALSE, FALSE);
}

void LLFloaterReceiveLandmark::onClickCancel(void* data)
{
	LLFloaterReceiveLandmark *self = static_cast<LLFloaterReceiveLandmark*>( data );
	self->mSaved = TRUE;
	
	gInventory.deleteObject(self->mInventoryItem->getUUID());
	gInventory.notifyObservers();
	self->close();
}

void LLFloaterReceiveLandmark::onClickCreateFolder(void* data)
{
	LLFloaterReceiveLandmark* self = static_cast<LLFloaterReceiveLandmark*>( data );
	self->openFolderSelector();
}

/* // should be deleted, only here for reference
void LLFloaterReceiveLandmark::writeToFile(std::string filename, std::string name, std::string notes, LLUUID snapshot_id, BOOL is_received )
{
	time_t date; // Make a time_t object that'll hold the date

	time(&date); //  Set the date variable to the current date
	LLUUID uid = LLUUID(filename);
	LLSD landmark;
	filename = filename + ".xml";
	landmark["landmark"]["notes"] = notes;
	landmark["landmark"]["snapshot"] = snapshot_id;
	landmark["landmark"]["createdAt"] = ctime(&date); 
	llofstream file;

	std::string dir;
	dir = gDirUtilp->getLindenUserDir();
	dir += gDirUtilp->getDirDelimiter();
	dir += "Landmarks";

	LLFile::mkdir(dir.c_str());

	dir += gDirUtilp->getDirDelimiter();

	std::string pathName =dir+filename;

	file.open(pathName.c_str());

	if(file.is_open())
	{ 
		LLSDSerialize::toPrettyXML(landmark, file);
	}

	if(is_received)
	{
		LLSD landmark;
		filename = "received" ;
		llofstream file;
		llifstream infile;
		std::string dir;
		dir = gDirUtilp->getLindenUserDir();

		dir += gDirUtilp->getDirDelimiter();

		std::string pathName =dir+filename+ ".xml";
		infile.open(pathName.c_str());
		if(!infile.is_open())
		{
			file.open(pathName.c_str());
			file.close();
			infile.open(pathName.c_str());
		}
	
		if(infile.is_open())
		{ 
			LLVector3d pos;
			mLandmark->getGlobalPos(pos);

			S32 agent_x = llround( (F32)fmod( pos.mdV[VX], (F64)REGION_WIDTH_METERS ) );
			S32 agent_y = llround( (F32)fmod( pos.mdV[VY], (F64)REGION_WIDTH_METERS ) );
			S32 agent_z = llround( (F32)pos.mdV[VZ] );
			LLURI slurl = LLURLDispatcher::buildSLURL(mRegionName, agent_x, agent_y, agent_z);

			LLSDSerialize::fromXML(landmark, infile);
			int size = landmark.size();
			int count = size++;
			landmark[count]["received"]["name"] = name;
			landmark[count]["received"]["landmarkid"] = currentItem->getAssetUUID();
			landmark[count]["received"]["time"] = ctime(&date); 
			
			file.open(pathName.c_str());
			if(file.is_open())
			{
				LLSDSerialize::toPrettyXML(landmark, file);
				std::string::format_map_t args;
			}
		}
	}
}
*/

extern void copy_slurl(LLInventoryItem* inv_item);
extern void show_on_map(LLInventoryItem* inv_item);

void LLFloaterReceiveLandmark::onSelectItem(LLUICtrl* ctrl, void* user_data)
{	
	LLFloaterReceiveLandmark* self = (LLFloaterReceiveLandmark*)user_data;
	LLFlyoutButton* flyout = (LLFlyoutButton*)ctrl;

	if(LL_RECEIVE_LANDMARK_FLOATER_MENU_ITEM == flyout->getSelectedItemLabel())
	{
		copy_slurl( self->mInventoryItem );
		
	}
	else if(LL_RECEIVE_LANDMARK_FLOATER_MAP_ITEM == flyout->getSelectedItemLabel())
	{
		show_on_map( self->mInventoryItem );
	}
	else
	{
		gAgent.teleportViaLandmark(self->mInventoryItem->getAssetUUID());
	}
}

// virtual
void LLFloaterReceiveLandmark::onClose(bool app_quitting)
{
	if( !app_quitting  && !mSaved )
	{
		gViewerWindow->alertXml("AddLandmark",LLFloaterReceiveLandmark::onSaveDialog, this);
	}
	else
	{		
		closeFolderSelector();
		destroy();
	}
	
}

void LLFloaterReceiveLandmark::onSaveDialog(S32 option, void* data)
{
	LLFloaterReceiveLandmark* self = static_cast<LLFloaterReceiveLandmark*>( data );
	switch( option )
	{
	case 0:  // "Save All"
		onClickSave( self );
		break;

	case 1:  // "Don't Save"
		self->mSaved = TRUE;
		onClickCancel( self );
		break;

	case 2: // "Cancel"
	default:
		break;
	}	
}



LLUUID LLFloaterReceiveLandmark::getInventoryItemUUID()
{
	if( mInventoryItem )
		return mInventoryItem->getUUID();

	return LLUUID::null;
}
