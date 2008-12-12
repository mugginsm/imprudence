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


#include "llviewerprecompiledheaders.h"

#include "llfloater.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"

#include "llcombobox.h"
#include "llagent.h"
#include "llvoavatar.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llstring.h"
#include "lluiconstants.h"
#include "llurldispatcher.h"
#include "llworldmap.h"

#include "llinventory.h"
#include "llinventoryview.h"
#include "llinventorymodel.h"
#include "lllandmark.h"
#include "lllandmarklist.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "lltexteditor.h"
#include "llparcel.h"
#include "llfloatermanagelandmark.h"

#include "llparcel.h"
#include "message.h"

#include "llfloateravatarpicker.h"
#include "llbutton.h"
#include "llradiogroup.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llnotify.h"
#include "llpanellandmedia.h"
#include "llradiogroup.h"
#include "llscrolllistctrl.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "lluiconstants.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"
#include "llinventorymodel.h"
#include <sstream>
#include <time.h>
#include "llsdserialize.h"
#include "llworldmap.h"
#include "llfloaterworldmap.h"
#include "llpanelplace.h"
#include "llappviewer.h"
#include "llfloaterselectfolder.h"
#include "llfloateraddlandmark.h"
#include "lllandmarklist.h"
#include "llfloatereditlandmark.h"
#include "lllandmarkcommon.h"

const LLString LL_PATH_LANDMARK_FILE("landmarks.xml");
const LLString LL_ADD_LANDMARK_FLOATER_FOLDER_FILE_NAME("Folders.xml");
const LLString LL_ADD_LANDMARK_FLOATER_LINEEDITOR_NAME("Name");
const LLString LL_ADD_LANDMARK_FLOATER_TEXTURE_SNAPSHOT("snapshot_ctrl");
const LLString LL_ADD_LANDMARK_FLOATER_COMBOBOX_FOLDER("landmarks_combobox");
const LLString LL_ADD_LANDMARK_FLOATER_TEXT_NOTES("Mynotes");
const LLString LL_ADD_LANDMARK_FLOATER_TEXT_LOCATION("location_value");
const LLString LL_ADD_LANDMARK_FLOATER_TEXT_TRAFFIC("traffic_value");
const LLString LL_ADD_LANDMARK_FLOATER_TEXT_AREA("area_value");
const LLString LL_ADD_LANDMARK_FLOATER_TEXT_DESCRIPTION("description_value");
const LLString LL_ADD_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT("default_snapshot");
const LLString LL_ADD_LANDMARK_FLOATER_MENU_ITEM("Copy SLURL");
const LLString LL_EDIT_LANDMARK_FLOATER_MAP_ITEM("Show on Map");
const LLString LL_ADD_LANDMARK_FLOATER_BUTTON_SAVE( "save" );
const LLString LL_ADD_LANDMARK_FLOATER_BUTTON_CANCEL( "cancel" );
const LLString LL_ADD_LANDMARK_FLOATER_BUTTON_CREATE_FOLTER( "create_folder" );

BOOL LLFloaterAddLandmark::sLandmarkAdded = FALSE;

// helps LLFloaterAddLandmark create an inventory item
// sends the newly created inventory item to the floater after it arrives from the server
class LLAddLandmarkCallback : public LLInventoryCallback
{
public:
	LLAddLandmarkCallback( LLFloaterAddLandmark* floater )
	{
		mLandmarkFloater = floater;
	}

	// the landmark we wanted got added to the inventory!
	void fire( const LLUUID &inventory_item_uuid )
	{
		llinfos << "LLAddLandmarkCallback::fire(); our landmark was added to the inventory! " << inventory_item_uuid << llendl;

		// tell our floater what the inventory id is
		LLInventoryItem* inventory_item = gInventory.getItem( inventory_item_uuid );
		if( ! inventory_item )
		{
			llinfos << "LLAddLandmarkCallback::fire(); failed to fetch inventory item for uuid: " << inventory_item_uuid << llendl;
			return;
		}

		mLandmarkFloater->setInventoryItem( inventory_item );

		// now fetch the actual landmark data for this inventory item
		// and redirect the callback to the floater that requested to add that item to the inventory :)
		gAssetStorage->getAssetData( inventory_item->getAssetUUID(), LLAssetType::AT_LANDMARK, LLFloaterAddLandmark::onLandmarkCreatedWrapper, mLandmarkFloater );
	}

private:

	LLFloaterAddLandmark* mLandmarkFloater;
};


// virtual
void LLFloaterAddLandmark::onClose(bool app_quitting)
{
	llinfos << "LLFloaterAddLandmark::onClose();" << llendl;

	if( !app_quitting && !mClosingFromNavbar && !mSafeToClose )
	{
		llinfos << "LLFloaterAddLandmark::onClose(); showing add landmark alert" << llendl;
		gViewerWindow->alertXml("AddLandmark",LLFloaterAddLandmark::onSaveDialogResponse, this);
	}
	else
	{	
		llinfos << "LLFloaterAddLandmark::onClose(); closing the dialog" << llendl;
		sLandmarkAdded = FALSE;	

		closeFolderSelector();
		destroy();
	}
}


// static wrapper for linking create_inventory_item() to onLandmarkCreated()
void LLFloaterAddLandmark::onLandmarkCreatedWrapper( LLVFS *vfs, const LLUUID& uuid, LLAssetType::EType type, void* user_data, S32 status, LLExtStat ext_status )
{
	llinfos << "LLFloaterAddLandmark::onLandmarkCreatedWrapper(); calling processGetAssetReply().." << llendl;
	LLLandmarkList::processGetAssetReply( vfs, uuid, type, user_data, status, ext_status );

	LLLandmark* landmark = gLandmarkList.getAsset( uuid );

	llinfos << "LLFloaterAddLandmark::onLandmarkCreatedWrapper(); landmark = " << landmark << llendl;

	if( ! landmark )
	{
		llwarns << "LLFloaterAddLandmark::onLandmarkCreatedWrapper(); could not get landmark!" << llendl;
		return;
	}

	LLFloaterAddLandmark* target = static_cast<LLFloaterAddLandmark*>( user_data );

	if( ! target )
	{
		llwarns << "LLFloaterAddLandmark::onLandmarkCreatedWrapper(); missing source floater" << llendl;
		return;
	}

	target->onLandmarkCreated( landmark );
}

// create_inventory_item() hits up LLAddLandmarkCallback::fire() once an inventory item is create for our new landmark
// which then hits LLFloaterAddLandmark::onLandmarkCreatedWrapper() which then sends the new landmark here
void LLFloaterAddLandmark::onLandmarkCreated( LLLandmark* landmark )
{
	if( ! landmark )
	{
		llwarns << "LLFloaterAddLandmark::onLandmarkCreated(); invalid arguments" << llendl;
		return;
	}

	mLandmark = landmark;

	// now that we have received the inventory item from the server, we can save some metadata locally :)
	saveLandmarkMetaData();

	close();
	LLFloaterAddLandmark::sLandmarkAdded = FALSE;
}


LLFloaterAddLandmark::LLFloaterAddLandmark()
:	LLFloaterCommonLandmark( "floater_add_landmark", "CreateLandmarkRect", "" ),
	mClosingFromNavbar( FALSE ),
	mSafeToClose( FALSE )
{
}

LLFloaterAddLandmark::~LLFloaterAddLandmark()
{
	llinfos << "LLFloaterAddLandmark::~LLFloaterAddLandmark();" << llendl;
	if( mParcelResponder )
		mParcelResponder->setObserver( NULL );

	LLVFGenericMultiplexer<LLParcelInfo>::getInstance()->removeObserver( this );
	LLLandmarkFloaterFactory<LLFloaterAddLandmark>::getInstance()->removeFloater( this );

	llinfos << "LLFloaterAddLandmark::~LLFloaterAddLandmark(); done" << llendl;
}

void LLFloaterAddLandmark::beginBuild()
{
	LLUICtrlFactory::getInstance()->buildFloater( this, "floater_add_landmark.xml" );
}

void LLFloaterAddLandmark::toggleInteractiveContent( BOOL Enable )
{
	childSetEnabled( LL_ADD_LANDMARK_FLOATER_LINEEDITOR_NAME, Enable );
 	childSetEnabled( LL_ADD_LANDMARK_FLOATER_BUTTON_SAVE, Enable );
	childSetEnabled( LL_ADD_LANDMARK_FLOATER_BUTTON_CANCEL, Enable );
	childSetEnabled( LL_ADD_LANDMARK_FLOATER_BUTTON_CREATE_FOLTER, Enable );
	childSetEnabled( LL_ADD_LANDMARK_FLOATER_TEXTURE_SNAPSHOT, Enable );
	childSetEnabled( LL_ADD_LANDMARK_FLOATER_COMBOBOX_FOLDER, Enable );
	childSetEnabled( LL_ADD_LANDMARK_FLOATER_TEXT_NOTES, Enable );
   
}

BOOL LLFloaterAddLandmark::postBuild()
{
	mComboboxFolders = getChild<LLComboBox>(LL_ADD_LANDMARK_FLOATER_COMBOBOX_FOLDER);
	mEditorName = getChild<LLLineEditor>(LL_ADD_LANDMARK_FLOATER_LINEEDITOR_NAME);
	mEditorDetails = getChild<LLTextEditor>(LL_ADD_LANDMARK_FLOATER_TEXT_DESCRIPTION);
	mSnapshotCtrl = getChild<LLTextureCtrl>( LL_ADD_LANDMARK_FLOATER_TEXTURE_SNAPSHOT );
	mEditorNotes = getChild<LLTextEditor>(LL_ADD_LANDMARK_FLOATER_TEXT_NOTES);
	mButtonDefaultSnapshot = getChild<LLButton>(LL_ADD_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT);
	
	// bind callbacks
	childSetAction(LL_ADD_LANDMARK_FLOATER_BUTTON_SAVE, onClickSave, this);
	childSetAction(LL_ADD_LANDMARK_FLOATER_BUTTON_CANCEL, onClickCancel, this);
	childSetAction(LL_ADD_LANDMARK_FLOATER_BUTTON_CREATE_FOLTER, onClickCreateFolder, this);
	childSetAction(LL_ADD_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, onClickDefaultSnapshot, this);
	childSetVisible(LL_ADD_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, FALSE);

	// the only piece of external information we need is the global position of the character
	// we use that to get the parcelid and then the parcel data for the land we are on
	mLandData.mGlobalPosition = gAgent.getPositionGlobal();
	mParcelResponder = new LLLandmarkParcelRequestResponder( this, mLandData.mGlobalPosition );

	// setup default config for the snapshot control
	mSnapshotCtrl->setCommitCallback( onChangeSnapshot );
	mSnapshotCtrl->setCallbackUserData( this );
	mSnapshotCtrl->setImageAssetID( LLUUID::null );

	setTitle("Add Landmark: Fetching Info..");
	toggleInteractiveContent( FALSE );
	populateCombo();

	centerWithin(gViewerWindow->getRootView()->getRect());

	return TRUE;
}

void LLFloaterAddLandmark::showLandInfo()
{
	mEditorName->setText(mLandData.mRegionName);
	mEditorDetails->setText(mLandData.getFormatedLandDetails());

	// hide the "restore default image" button
	childSetVisible(LL_ADD_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, FALSE);

	// display landmark snapshot if we have one
	if( mLandData.mSnapshotID.notNull() )
		mSnapshotCtrl->setImageAssetID( mLandData.mSnapshotID );

	setTitle( "Add Landmark" );
	toggleInteractiveContent( TRUE );
}

void LLFloaterAddLandmark::onClickSave(void *data)
{
	LLFloaterAddLandmark *self = (LLFloaterAddLandmark*)data;

	self->mSafeToClose = TRUE;

	self->setTitle( "Add Landmark: Saving.." );
	self->toggleInteractiveContent( FALSE );

	if( ! gAgent.getAvatarObject())
		return;

	LLString location_name = self->mEditorName->getText();
	if( location_name == "" )
		location_name = self->mLandData.mRegionName;

	LLUUID folder_id = self->mComboboxFolders->getSelectedValue().asUUID();

	// if no folder is selected, save by default into the main landmarks folder
	if( folder_id == LLUUID::null)
	{
		llinfos << "LLFloaterAddLandmark::onClickSave(); no folder chosen; saving into default Landmarks folder" << llendl;
		folder_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);
	}

	self->mLandData.mSnapshotID = self->mSnapshotCtrl->getImageAssetID();

	LLViewerInventoryCategory* targetFolder = gInventory.getCategory(folder_id);
	
	std::string pos_string;
	gAgent.buildLocationString(pos_string);

	llinfos << "LLFloaterAddLandmark::onClickSave(); " << location_name << " pos_string = " << pos_string << llendl;

	if( !targetFolder)
	{
		llwarns << "LLFloaterAddLandmark::onClickSave(); invalid target folder!" << llendl;
		targetFolder = gInventory.getCategory( gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK) );
	}

	LLPointer<LLInventoryCallback> landmark_added_callback = new LLAddLandmarkCallback( self );
	llinfos << "LLFloaterAddLandmark::onClickSave(); sending request to create inventory item for object " << targetFolder->getUUID() << llendl;

	// sends out a message to create the inventory item
	// when server reply comes back, our callback gets called which routes the message back to this object :)
	create_inventory_item(gAgent.getID(), gAgent.getSessionID(), targetFolder->getUUID(), LLTransactionID::tnull,
		location_name, pos_string, LLAssetType::AT_LANDMARK, LLInventoryType::IT_LANDMARK, NOT_WEARABLE, PERM_ALL, landmark_added_callback);

	// we finish saving after the inventory item has been created
	sLandmarkAdded = TRUE;
}

void LLFloaterAddLandmark::onClickCancel(void* data)
{
	LLFloaterAddLandmark *self = static_cast<LLFloaterAddLandmark*>( data );
	self->mSafeToClose = TRUE;
	self->close();
}


void LLFloaterAddLandmark::onClickCreateFolder(void* data)
{
	LLFloaterAddLandmark* self = static_cast<LLFloaterAddLandmark*>( data );
	self->openFolderSelector();
}

void LLFloaterAddLandmark::onSaveDialogResponse(S32 option, void* data)
{
	enum { SAVE_ALL = 0, DONT_SAVE, CANCEL };

	LLFloaterAddLandmark* self = (LLFloaterAddLandmark*)data;
	
	switch( option )
	{
		case SAVE_ALL:
		{
			onClickSave(data); // this is async, once the inventory item comes back from the server, floater will close
		} break;

		case DONT_SAVE:
		{
			self->mSafeToClose = TRUE;
			self->close();
		} break;

		case CANCEL:
		default:
		break;
	}
}
