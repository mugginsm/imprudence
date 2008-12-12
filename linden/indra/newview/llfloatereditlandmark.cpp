/** 
 * @file llfloatereditlandmark.cpp
 * @author Vectorform
 * @brief Floater to Edit your landmarks
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
#include "llviewerparcelmgr.h"
#include "llcachename.h"
#include "llfocusmgr.h"
#include "llparcel.h"
#include "message.h"
#include "llfloateravatarpicker.h"
#include "llbutton.h"
#include "llradiogroup.h"
#include "llfloaterauction.h"
#include "llfloateravatarinfo.h"
#include "llfloatergroups.h"
#include "llfloatergroupinfo.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llnotify.h"
#include "llpanellandmedia.h"
#include "llselectmgr.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "lluiconstants.h"
#include "lluictrlfactory.h"
#include "llviewermessage.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "lllandmarklist.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"
#include "roles_constants.h"
#include "llinventorymodel.h"
#include <sstream>
#include <ctime>
#include "llfloaterland.h"
#include "llsdserialize.h"
#include "llpanelplace.h"
#include "llworldmap.h"
#include "llfloaterworldmap.h"
#include "llappviewer.h"
#include "lldir.h"

#include "llfloatermanagelandmark.h"
#include "llfloatereditlandmark.h"
#include "lllandmarkcommon.h"

const LLString LL_EDIT_LANDMARK_FLOATER_LINEEDITOR_NAME("Name");
const LLString LL_EDIT_LANDMARK_FLOATER_TEXTURE_SNAPSHOT("snapshot_ctrl");
const LLString LL_EDIT_LANDMARK_FLOATER_TEXT_NOTES("Mynotes");
const LLString LL_EDIT_LANDMARK_FLOATER_TEXT_DESCRIPTION("Details");
const LLString LL_EDIT_LANDMARK_FLOATER_MAP_ITEM("Show on Map");
const LLString LL_EDIT_LANDMARK_FLOATER_SLURL_ITEM("Copy SLURL");
const LLString LL_EDIT_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT("default_snapshot");
const LLString LL_EDIT_LANDMARK_FLOATER_LOADING_MESSAGE( "Loading.." );
const LLString LL_EDIT_LANDMARK_FLOATER_BUTTON_SAVE( "Apply_Changes" );
const LLString LL_EDIT_LANDMARK_FLOATER_BUTTON_CANCEL( "Discard_Changes" );

LLFloaterEditLandmark::LLFloaterEditLandmark()
:	LLFloaterCommonLandmark( "floater_edit_landmark", "CreateLandmarkRect", "" ),
	mSafeToClose(FALSE)
{
}

// must specify the landmark and inventory item before calling beginBuild()
void LLFloaterEditLandmark::beginBuild()
{
	LLUICtrlFactory::getInstance()->buildFloater( this, "floater_edit_landmark.xml", false );
}


BOOL LLFloaterEditLandmark::postBuild()
{
	mEditorName = getChild<LLLineEditor>(LL_EDIT_LANDMARK_FLOATER_LINEEDITOR_NAME);
	mEditorDetails = getChild<LLTextEditor>(LL_EDIT_LANDMARK_FLOATER_TEXT_DESCRIPTION);
	mEditorNotes = getChild<LLTextEditor>(LL_EDIT_LANDMARK_FLOATER_TEXT_NOTES);
	mSnapshotCtrl = getChild<LLTextureCtrl>( LL_EDIT_LANDMARK_FLOATER_TEXTURE_SNAPSHOT );
	mButtonDefaultSnapshot = getChild<LLButton>(LL_EDIT_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT);

	if( ! mLandmark || ! mInventoryItem )
	{
		llwarns << "LLFloaterEditLandmark::postBuild(); landmark or inventory item not set!" << llendl;
		return FALSE;
	}

	mParcelResponder = new LLLandmarkParcelRequestResponder( this, mLandmark, mInventoryItem->getAssetUUID() );	

	childSetAction(LL_EDIT_LANDMARK_FLOATER_BUTTON_SAVE, onClickApply, this);
	childSetAction(LL_EDIT_LANDMARK_FLOATER_BUTTON_CANCEL, onClickDiscard, this);
	childSetAction(LL_EDIT_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, onClickDefaultSnapshot, this);
    childSetVisible(LL_EDIT_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, FALSE);
	// setup default config for the snapshot control
	mSnapshotCtrl->setCommitCallback( onChangeSnapshot );
	mSnapshotCtrl->setCallbackUserData( this );
	mSnapshotCtrl->setImageAssetID( LLUUID::null );

	setTitle("Edit Landmark: Fetching Info..");
	toggleInteractiveContent( FALSE );

	// fill in locally stored data
	LLSD landmark_data = loadLandmarkMetaData( mInventoryItem->getAssetUUID() );

	llinfos << "LLFloaterEditLandmark::postBuild(); landmark_data = " << landmark_data << llendl;

	mEditorName->setText( mInventoryItem->getName() );
	mEditorNotes->setText( landmark_data["landmark"]["notes"].asString() );
	snapshot_previous = landmark_data["landmark"]["snapshot"].asUUID();
	mSnapshotCtrl->setImageAssetID( snapshot_previous );

	mSavedEditorNotes = mEditorNotes->getText();

	centerWithin(gViewerWindow->getRootView()->getRect());
	childSetCommitCallback("teleport", onSelectItem, this);

	return TRUE;
}

void LLFloaterEditLandmark::toggleInteractiveContent( BOOL Enable )
{
	childSetEnabled( LL_EDIT_LANDMARK_FLOATER_LINEEDITOR_NAME, Enable );
	childSetEnabled( LL_EDIT_LANDMARK_FLOATER_BUTTON_SAVE, Enable );
	childSetEnabled( LL_EDIT_LANDMARK_FLOATER_BUTTON_CANCEL, Enable );
	childSetEnabled( LL_EDIT_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, Enable );
	childSetEnabled( LL_EDIT_LANDMARK_FLOATER_TEXTURE_SNAPSHOT, Enable );
	childSetEnabled( LL_EDIT_LANDMARK_FLOATER_TEXT_NOTES, Enable );
	
}

LLFloaterEditLandmark::~LLFloaterEditLandmark()
{
	llinfos << "LLFloaterEditLandmark::~LLFloaterEditLandmark();" << llendl;
	if( mParcelResponder )
		mParcelResponder->setObserver( NULL );

	LLVFGenericMultiplexer<LLParcelInfo>::getInstance()->removeObserver( this );
	LLLandmarkFloaterFactory<LLFloaterEditLandmark>::getInstance()->removeFloater( this );

	llinfos << "LLFloaterEditLandmark::~LLFloaterEditLandmark(); remove floater.. done" << llendl;
}

BOOL LLFloaterEditLandmark::saveLandmark()
{
	//LLFloaterEditLandmark* self = static_cast<LLFloaterEditLandmark*>( data );
	
	llinfos << "LLFloaterEditLandmark::saveLandmark();" << llendl;
	BOOL saved_metadata = saveLandmarkMetaData();

	if( saved_metadata )
	{
		// texteditor controls dont have a dirty reset flag
		// and makePristine() does totally the wrong thing
		//mEditorNotes->makePristine();
		//mEditorDetails->makePristine();
		mEditorName->resetDirty();
		
		mSafeToClose = TRUE;

		//LLString::format_map_t args;
		//args["[FILE_NAME]"] = "Landmark";
		//gViewerWindow->alertXml("SaveSuccess", args);
	}

	// perform some sort of name validation? dont let user create empty named landmarks?
	// check if there is already a landmark with the same name?
	LLString new_landmark_name = mEditorName->getText();

	if( new_landmark_name == "" )
		new_landmark_name = mLandData.mRegionName;

	LLViewerInventoryItem* viewer_item = dynamic_cast<LLViewerInventoryItem*>( mInventoryItem );
	if( ! viewer_item )
	{
		llwarns << "LLFloaterEditLandmark::saveLandmark(); invalid inventory item" << llendl;
		return FALSE;
	}

	viewer_item->rename( new_landmark_name );
	viewer_item->updateServer( FALSE );

	// notify inventory observers that there was a name change
	gInventory.updateItem( viewer_item );
	gInventory.addChangedMask( LLInventoryObserver::LABEL, viewer_item->getUUID() );
	
	LLLineEditor* nameBox = getChild<LLLineEditor>(LL_EDIT_LANDMARK_FLOATER_LINEEDITOR_NAME);
	mInventoryItem->rename(nameBox->getText());
	mInventoryItem->updateServer(FALSE);
	gInventory.addChangedMask( LLInventoryObserver::LABEL, mInventoryItem->getUUID());
	gInventory.notifyObservers();
		
	return TRUE;
}

// called once, on receiving parcel data from the server
void LLFloaterEditLandmark::showLandInfo()
{
	mEditorDetails->setText(mLandData.getFormatedLandDetails());

	mSavedEditorDetails = mEditorDetails->getText();

	// if we dont already have a snapshotid loaded from our landmark notes
	// set the received snapshotid
	if( mSnapshotCtrl->getImageAssetID().isNull() && mLandData.mSnapshotID.notNull() )
	{
		mSnapshotCtrl->setImageAssetID(mLandData.mSnapshotID);
		childSetVisible(LL_EDIT_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, FALSE);
	}

		// hide the "restore default image" button
	else if(snapshot_previous == mLandData.mSnapshotID)
        childSetVisible(LL_EDIT_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, FALSE);
	else
		childSetVisible(LL_EDIT_LANDMARK_FLOATER_BUTTON_RESET_SNAPSHOT, TRUE);

	setTitle( "Edit Landmark" );
	toggleInteractiveContent( TRUE );
}

//static
void LLFloaterEditLandmark::onClickApply(void* data)
{
	LLFloaterEditLandmark* self = static_cast<LLFloaterEditLandmark*>( data );
	
	self->saveLandmark();
	self->close();
}

//static
void LLFloaterEditLandmark::onClickDiscard(void* data)
{
	LLFloaterEditLandmark* self = static_cast<LLFloaterEditLandmark*>( data );
	self->mSafeToClose = TRUE;
	self->close();
}

// static callback from confirmation dialog that pops up when a user closes the floater without saving changes
void LLFloaterEditLandmark::onSaveDialogResponse(S32 option, void* data)
{
	enum { SAVE_ALL = 0, DONT_SAVE, CANCEL };

	LLFloaterEditLandmark* self = static_cast<LLFloaterEditLandmark*>( data );

	llinfos << "LLFloaterEditLandmark::onSaveDialogResponse(); option = " << option << llendl;
	
	switch( option )
	{
		case SAVE_ALL:
		{
			onClickApply(data);
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
	llinfos << "LLFloaterEditLandmark::onSaveDialogResponse(); out" << llendl;
}

// static callback from calling close() on the floater
void LLFloaterEditLandmark::onClose(bool app_quitting)
{
	closeFolderSelector();

	// calculate dirtiness
	bool Clean = true;
	if( ! mSafeToClose )
		Clean = mEditorNotes->getText() == mSavedEditorNotes && mEditorDetails->getText() == mSavedEditorDetails && ! mEditorName->isDirty() && ! mSnapshotCtrl->isDirty();

	// if something was edited by the user, confirm if she wants to save before closing the dialog
	if( ! mSafeToClose && ! Clean )
	{	// we are dirty and NOT discarding new info
		// confirm with user
		gViewerWindow->alertXml("SaveLandmarkChanges", &onSaveDialogResponse, this);
	}
	else
	{	// we are either clean or are discarding new info
		this->destroy();
	}
}

// static callback from selecting an option from the teleport flyout
void LLFloaterEditLandmark::onSelectItem(LLUICtrl* ctrl, void* user_data)
{	
	LLFloaterEditLandmark* self = static_cast<LLFloaterEditLandmark*>( user_data );
	LLFlyoutButton* flyout = static_cast<LLFlyoutButton*>( ctrl );

	if(LL_EDIT_LANDMARK_FLOATER_SLURL_ITEM == flyout->getSelectedItemLabel())
		self->copyUrlToClipboard();
	else
	if(LL_EDIT_LANDMARK_FLOATER_MAP_ITEM == flyout->getSelectedItemLabel())
		self->showOnMap();
	else
		gAgent.teleportViaLandmark(self->mInventoryItem->getAssetUUID());
}
