/** 
* @file llfloaterselectFolder.cpp
* @author Vectorform
* @brief Floater provides UI to add folder
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
#include "llfloaterselectfolder.h"
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
#include "llviewerparcelmgr.h"

#include "llcachename.h"
#include "llfocusmgr.h"
#include "llparcel.h"
#include "message.h"

#include "llfloateravatarpicker.h"
#include "llbutton.h"
#include "llradiogroup.h"
#include "llcombobox.h"
#include "llfloaterauction.h"
#include "llfloateravatarinfo.h"
#include "llfloatergroups.h"
#include "llfloatergroupinfo.h"
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
#include "lluictrlfactory.h"
#include "llviewermessage.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"
#include "roles_constants.h"
#include "llinventorymodel.h"
#include <sstream>
#include <time.h>
#include "llfloaterland.h"
#include "llsdserialize.h"
#include "llfloaterreceivelandmark.h"
#include "llfloaterselectfolder.h"

const std::string LL_ADD_FOLDER_FLOATER_LINEEDITOR_NAME("folder_name_edit");
const std::string LL_ADD_FOLDER_FLOATER_FOLDER_FILE_NAME("Folders.xml");
LLUUID parentFolder=LLUUID::null;
std::string parentName("");
std::string rootFolderLabel("");

LLFloaterSelectFolder::LLFloaterSelectFolder()
:	LLFloater("floater_add_folder"), mSelectedFolder( NULL )
{
	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_add_folder.xml",  false);
}

LLFloaterSelectFolder::~LLFloaterSelectFolder()
{
}

BOOL LLFloaterSelectFolder::postBuild()
{
	childSetAction("save_btn", onClickAdd, this);
	childSetAction("cancel_btn", onClickCancel, this);
	childSetAction("select_btn", onClickSelect, this);

	mLandmarkPanel = getChild<LLInventoryPanel>("Folder_panel");

	U32 filter_types = 0x0;
	filter_types  |= 0x1 << LLInventoryType::IT_CATEGORY;
	LLUUID categoryId ;
	categoryId = gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);

	mLandmarkPanel->setFilterTypes(filter_types);
	mLandmarkPanel->setSelectCallback(onSelectionChange, this);
	mLandmarkPanel->setAllowMultiSelect(TRUE);

	// store this filter as the default one
	mLandmarkPanel->getRootFolder()->getFilter()->markDefault();
	mLandmarkPanel->openDefaultFolderForType(LLAssetType::AT_LANDMARK);
	mLandmarkPanel->setShowFolderState(LLInventoryFilter::SHOW_MY_LANDMARK_FOLDERS);

	return TRUE;
}

void LLFloaterSelectFolder::onClose(bool app_quitting)
{
	mOnExitCallback();
	destroy();
}

void LLFloaterSelectFolder::setOnFolderSelectCallback( FolderSelectCallback Callback ) {
	mFolderSelectCallback = Callback;
}

void LLFloaterSelectFolder::setOnExitCallback( OnExitCallback Callback ) {
	mOnExitCallback = Callback;
}

void LLFloaterSelectFolder::onClickCancel(void* data)
{
	LLFloaterSelectFolder* self = (LLFloaterSelectFolder*)data;

	LLSelectedFolder Folder;
	Folder.folder_chosen = FALSE;
	self->mFolderSelectCallback( Folder );

	self->close();
}

// adds a folder whose name is in the line editor to the inventory
void LLFloaterSelectFolder::onClickAdd(void* data)
{	
	LLFloaterSelectFolder* self = static_cast<LLFloaterSelectFolder*>( data );

	LLLineEditor* folderNameEditor = self->getChild<LLLineEditor>(LL_ADD_FOLDER_FLOATER_LINEEDITOR_NAME);
	std::string folderName = folderNameEditor->getText();

	if( ! self->mSelectedFolder )
		self->mSelectedFolder = gInventory.getCategory( gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK) );

	// create the new folder
	LLUUID new_folder_uuid = gInventory.createNewCategory(self->mSelectedFolder->getUUID(), LLAssetType::AT_NONE, folderName);
	LLViewerInventoryCategory* item = gInventory.getCategory(new_folder_uuid);
	gInventory.updateCategory(item);
	gInventory.notifyObservers();
	
	// update the "latest used folders" list

	LLSD folders;
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, LL_ADD_FOLDER_FLOATER_FOLDER_FILE_NAME);
	
	llifstream file;
	int counter=0;
	file.open(filename.c_str());

	if(file.is_open())
	{
		U32 retval = LLSDSerialize::fromXML(folders, file);
		if( retval > 0 )
			counter = folders.size();
	}
 
	counter++;

	folders[counter]["folders"]["name"] = folderName;
	folders[counter]["folders"]["lluuid"] = new_folder_uuid;
	std::string fullPath = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, LL_ADD_FOLDER_FLOATER_FOLDER_FILE_NAME); 
	llofstream file_write;
	file_write.open(fullPath.c_str());

	if(file_write.is_open())
	{
		LLSDSerialize::toPrettyXML(folders, file_write);		
	}

	self->mLandmarkPanel->setSelection(new_folder_uuid,TRUE);

}
void LLFloaterSelectFolder::onClickSelect(void* data)
{	
	LLFloaterSelectFolder* self = (LLFloaterSelectFolder*)data;
	
	if( ! self->mSelectedFolder )
		self->mSelectedFolder = gInventory.getCategory( gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK) );

	LLSelectedFolder Folder;
	Folder.folder_chosen = TRUE;
	Folder.folder_id = self->mSelectedFolder->getUUID();
	Folder.folder_name = self->mSelectedFolder->getName();

	self->mFolderSelectCallback( Folder );
	self->close();
}

// store the currently selected folder
void LLFloaterSelectFolder::onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action, void* data)
{
	if( items.size() < 1 )
	{	// nothing selected
		return;
	}

	LLFloaterSelectFolder* self = static_cast<LLFloaterSelectFolder*>( data );


	// multiple selection is disabled, so we only have one item, if that
	LLFolderViewItem* selected_item = items.front();

	LLUUID selected_folder_uuid = selected_item->getListener()->getUUID();

	// dont allow the user to select "My Inventory", default it to "My Inventory\Landmarks"
	if( selected_item->getName() == "My Inventory" )
		selected_folder_uuid = gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);

	self->mSelectedFolder = gInventory.getCategory( selected_folder_uuid );

	if( self->mLandmarkPanel )
	{
		// carries out item/folder renaming
		LLFolderView* fv = self->mLandmarkPanel->getRootFolder();
		if (fv->needsAutoRename()) // auto-selecting a new user-created asset and preparing to rename
		{
			fv->setNeedsAutoRename(FALSE);
			if (items.size()) // new asset is visible and selected
			{
				fv->startRenamingSelectedItem();
			}
		}
	}
}
