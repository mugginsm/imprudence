/** 
* @file llfloatermanagelandmark.cpp
* @author Vectorform
* @brief Floater to manage your landmarks
 $LicenseInfo:firstyear=2006&license=viewergpl$
 
 Copyright (c) 2006-2008, Linden Research, Inc.
 
 Second Life Viewer Source Code
 The source code in this file ("Source Code") is provided by Linden Lab
 to you under the terms of the GNU General Public License, version 2.0
 ("GPL"), unless you have obtained a separate licensing agreement
 ("Other License"), formally executed by you and Linden Lab.  Terms of
 the GPL can be found in doc/GPL-license.txt in this distribution, or
 online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 
 There are special exceptions to the terms and conditions of the GPL as
 it is applied to this Source Code. View the full text of the exception
 in the file doc/FLOSS-exception.txt in this software distribution, or
 online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
 
 By copying, modifying or distributing this software, you acknowledge
 that you have read and understood your obligations described above,
 and agree to abide by those obligations.
 
 ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 COMPLETENESS OR PERFORMANCE.
 $/LicenseInfo$
 */


#include "llviewerprecompiledheaders.h"
#include "llfloatermanagelandmark.h"
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
#include "llinventoryview.h"
#include "llfloatereditlandmark.h"
#include "llfloateraddlandmark.h"
#include "lllandmarkcommon.h"

// helpers called from the landmark manager or inventory context menu to copy selected inventory item's slurl to clipboard
extern void copy_slurl(LLInventoryItem* inv_item);
extern void show_on_map(LLInventoryItem* inv_item);
extern void edit_landmark(LLInventoryItem* inv_item);

const LLString LL_MANAGE_LANDMARK_FLOATER_MENU_ITEM("Copy SLURL");
const LLString LL_MANAGE_LANDMARK_FLOATER_MAP_ITEM("Show on Map");

BOOL LLFloaterManageLandmark::floaterOpened = FALSE;
BOOL LLFloaterManageLandmark::m_DraggingItem = FALSE;

LLFloaterManageLandmark::LLFloaterManageLandmark(const LLSD& seed)
:	LLFloater("floater_manage_landmark"), 
	mLandmarkPanel(NULL), mSelectedFolderInvItem( NULL ),
	mSearchEdit(NULL), mSelectedFolder( "" ),
	mLandmarkInvItem(NULL), mInSearchMode( FALSE ),
	mLandmark( NULL ), mActivePanel( NULL )
{
	LLFloaterManageLandmark::floaterOpened = TRUE;
	LLUICtrlFactory::getInstance()->buildFloater( this, "floater_manage_landmark.xml", FALSE );
}

LLFloaterManageLandmark::~LLFloaterManageLandmark(void)
{
	LLFloaterManageLandmark::floaterOpened = FALSE;

	gFocusMgr.releaseFocusIfNeeded( this ); // calls onCommit()

	// restore edit commands handler
	if( gEditMenuHandler == this )
		gEditMenuHandler = NULL;
}

class LLVFLandmarkLoader : public LLInventoryFetchDescendentsObserver
{
public:

	LLVFLandmarkLoader() {
	}
	~LLVFLandmarkLoader() {
	}

	// called once inventory finishes fetching everything in the requested category/folder
	virtual void done()
	{
		llinfos << "LLVFLandmarkLoader::done(); loaded landmarks!" << llendl;

		// contents done loading
		gInventory.removeObserver(this);
		delete this;
	}
};

BOOL LLFloaterManageLandmark::postBuild()
{
	// our controls
	mSearchEdit = getChild<LLSearchEditor>("inventory search editor");
	mLandmarkPanel = getChild<LLInventoryPanel>("landmark_panel");
	mButtonCreateNewFolder = getChild<LLButton>( "New_Folder" );
	mButtonEdit = getChild<LLButton>( "Edit_landmark" );
	mButtonDelete = getChild<LLButton>( "Delete" );
	mFlyoutButtonTeleport = getChild<LLFlyoutButton>( "teleport" );

	// bind controls
	mSearchEdit->setSearchCallback( onSearchEdit, this );
	mButtonDelete->setClickedCallback( onClickDelete, this );
	mButtonCreateNewFolder->setClickedCallback( onClickNewFolder, this );
	mButtonEdit->setClickedCallback( onClickEditLandmark, this );
	
	mFlyoutButtonTeleport->setCommitCallback( onTeleportFlyoutAction );
	mFlyoutButtonTeleport->setCallbackUserData( this );
	
	// here we can setup the filter to match against landmark items
	// and optionally folders (in case the user wants a folder called detroit, with all the hot places to party)
	U32 filter_types = 0x0;
	filter_types |= 0x1 << LLInventoryType::IT_LANDMARK;
	filter_types |= 0x1 << LLInventoryType::IT_CATEGORY; // enable this to also match folders
	LLUUID categoryId ;
	categoryId = gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);

	// all buttons are disabled by default
	mButtonCreateNewFolder->setEnabled( FALSE );
	mFlyoutButtonTeleport->setEnabled( FALSE );
	mButtonDelete->setEnabled( FALSE );
	mButtonEdit->setEnabled( FALSE );

	mLandmarkPanel->setFilterTypes(filter_types);
	//mLandmarkPanel->setFilterPermMask(getFilterPermMask());  //Commented out due to no-copy texture loss.
	mLandmarkPanel->setSelectCallback(onSelectionChange, this);
	mLandmarkPanel->setAllowMultiSelect(TRUE);
	
	// store this filter as the default one
	mLandmarkPanel->getRootFolder()->getFilter()->markDefault();

	// opens the inventory > landmarks folder
	mLandmarkPanel->openDefaultFolderForType(LLAssetType::AT_LANDMARK);
	mLandmarkPanel->setShowFolderState(LLInventoryFilter::SHOW_ALL_LANDMARK_FOLDERS);
	
	mSavedFolderState = new LLSaveFolderState();
	mSavedFolderState->setApply(FALSE);

	// open the library > landmarks folder
	LLFolderViewFolder* LibraryFolder = mLandmarkPanel->getRootFolder()->getChild<LLFolderViewFolder>( "Library" );
	if( LibraryFolder )
	{
		LibraryFolder->setOpen();
		LLFolderViewFolder* Inv = LibraryFolder->getChild<LLFolderViewFolder>( "Landmarks" );
		if( Inv )
			Inv->setOpen( TRUE );
	}

	// start the landmark fetch, in case they werent loaded before
	LLVFLandmarkLoader* fetch = new LLVFLandmarkLoader();
	LLInventoryFetchDescendentsObserver::folder_ref_t folders;
	LLViewerInventoryCategory* category = gInventory.getCategory( categoryId );
	
	folders.push_back( category->getUUID() );
	fetch->fetchDescendents(folders);
	
	if(fetch->isEverythingComplete())
	{
		llinfos << "LLFloaterManageLandmark::postBuild(); landmarks are already loaded" << llendl;
		fetch->done();
	} else
		gInventory.addObserver(fetch);

	return TRUE;
}

/////////////////////////////////////////////////
// landmark manipulation helpers

LLViewerInventoryCategory* find_root_folder( LLViewerInventoryCategory* folder )
{
	LLUUID parent_uuid = folder->getParentUUID();
	while( parent_uuid.notNull() )
	{
		folder = gInventory.getCategory( parent_uuid );
		parent_uuid = folder->getParentUUID();
	}
	return folder;
}

BOOL LLFloaterManageLandmark::isCategoryProtected( LLViewerInventoryCategory* inventory_folder ) const
{
	LLDynamicArray<LLUUID> mProtectedFolders;
	mProtectedFolders.push_back( gInventory.findCategoryUUIDForType( LLAssetType::AT_LANDMARK ) );
	mProtectedFolders.push_back( gInventory.findCategoryUUIDForType( LLAssetType::AT_ROOT_CATEGORY ) );
	mProtectedFolders.push_back( gAgent.getInventoryRootID() );

	if( mProtectedFolders.find( inventory_folder->getUUID() ) != LLDynamicArray<LLUUID>::FAIL )
	{
		llinfos << "LLFloaterManageLandmark::isCategoryProtected(); " << inventory_folder->getName() << " is a protected special folder!" << llendl;
		return TRUE;
	}

	// check if its the special magical "received landmarks" folder
	if( inventory_folder->getName() == "Received Landmarks" && inventory_folder->getParentUUID() == gInventory.findCategoryUUIDForType( LLAssetType::AT_LANDMARK ) )
	{
		llinfos << "LLFloaterManageLandmark::isCategoryProtected(); " << inventory_folder->getName() << " is a protected magical folder!" << llendl;
		return TRUE;
	}

	LLViewerInventoryCategory* root_folder = find_root_folder( inventory_folder );

	// this is the real "My Inventory" folder
	if( root_folder == inventory_folder && inventory_folder->getName() == "My Inventory" )
	{
		llinfos << "LLFloaterManageLandmark::isCategoryProtected(); " << inventory_folder->getName() << " is a protected special folder!" << llendl;
		return TRUE;
	}

	// see if its a child of "Library > Landmarks"
	if( root_folder->getName() != "My Inventory" )
	{
		llinfos << "LLFloaterManageLandmark::isCategoryProtected(); " << inventory_folder->getName() << " is a protected library folder!" << llendl;
		return TRUE;
	}

	llinfos << "LLFloaterManageLandmark::isCategoryProtected(); " << inventory_folder->getName() << " is NOT a protected folder!" << llendl;
	return FALSE;
}

BOOL LLFloaterManageLandmark::isItemProtected( LLViewerInventoryItem* inventory_item ) const
{
	// see if its a child of "Library > Landmarks"
	LLViewerInventoryCategory* root_folder = find_root_folder( gInventory.getCategory( inventory_item->getParentUUID() ) );

	if( root_folder->getName() != "My Inventory" )
	{
		llinfos << "LLFloaterManageLandmark::isItemProtected(); " << inventory_item->getName() << " is a protected library item!" << llendl;
		return TRUE;
	}

	llinfos << "LLFloaterManageLandmark::isItemProtected(); " << inventory_item->getName() << " is NOT a protected item!" << llendl;
	return FALSE;
}


/////////////////////////////////////////////////
// gui callbacks

void LLFloaterManageLandmark::onSearchEdit(const LLString& search_string, void* user_data)
{
	LLFloaterManageLandmark* self = (LLFloaterManageLandmark*)user_data;

	if( search_string == "" )
	{
		self->mSelectedFolder = "";
		self->mInSearchMode = FALSE;
	}
	else
	{
		self->mInSearchMode = TRUE;
		self->mSelectedFolder = "";
	}

	std::string upper_case_search_string = search_string;
	LLString::toUpper(upper_case_search_string);

	llinfos << "search edited: " << search_string << llendl;

	// if new search string is empty (end search mode)
	if (upper_case_search_string.empty())
	{
		if (self->mLandmarkPanel->getFilterSubString().empty())
		{
			// current filter and new filter empty, do nothing
			return;
		}

		llinfos << "opening up folders with selection.." << llendl;

		self->mSavedFolderState->setApply(TRUE);
		self->mLandmarkPanel->getRootFolder()->applyFunctorRecursively(*self->mSavedFolderState);
		
		// add folder with current item to list of previously opened folders
		LLOpenFoldersWithSelection opener;
		self->mLandmarkPanel->getRootFolder()->applyFunctorRecursively(opener);
		self->mLandmarkPanel->getRootFolder()->scrollToShowSelection();
		
	}
	// if new string is not empty, but old search string WAS empty (begin search mode)
	else
	if (self->mLandmarkPanel->getFilterSubString().empty())
	{
		llinfos << "starting new search.." << llendl;

		// first letter in search term, save existing folder open state
		if (!self->mLandmarkPanel->getRootFolder()->isFilterModified())
		{
			self->mSavedFolderState->setApply(FALSE);
			self->mLandmarkPanel->getRootFolder()->applyFunctorRecursively(*self->mSavedFolderState);
		}
	}

	// deselect everything
	self->mLandmarkPanel->clearSelection();

	self->mLandmarkPanel->setFilterSubString(upper_case_search_string);
}

// publically accessible function for clearing the search filter
void LLFloaterManageLandmark::clearSearch()
{
	LLFloater *finder = mFinderHandle.get();

	if (mActivePanel)
	{
		mActivePanel->setFilterSubString("");
		mActivePanel->setFilterTypes(0xffffffff);
	}

	if (finder)
	{
		LLInventoryViewFinder::selectAllTypes(finder);
	}

	// re-open folders that were initially open
	if (mActivePanel)
	{
		llinfos << "LLFloaterManageLandmark::clearSearch(); reopening folders that were initially open" << llendl;
		mSavedFolderState->setApply(TRUE);
		mActivePanel->getRootFolder()->applyFunctorRecursively(*mSavedFolderState);
		LLOpenFoldersWithSelection opener;
		mActivePanel->getRootFolder()->applyFunctorRecursively(opener);
		mActivePanel->getRootFolder()->scrollToShowSelection();
	}
}

// gui callback to clear search filter
void LLFloaterManageLandmark::onClearSearch(void* user_data)
{
	LLFloaterManageLandmark* self = static_cast<LLFloaterManageLandmark*>( user_data );
	if( ! self )
	{
		llwarns << "LLFloaterManageLandmark::onClearSearch(); invalid user data" << llendl;
		return;
	}

	self->clearSearch();
}

// user selected an item or folder in our panel
void LLFloaterManageLandmark::onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action, void* data)
{
	LLFloaterManageLandmark* self = static_cast<LLFloaterManageLandmark*>( data );
	//self->mInSearchMode = FALSE;
	if( ! self )
	{
		llwarns << "LLFloaterManageLandmark::onSelectionChange(); invalid self ptr" << llendl;
		return;
	}

	// in llfolderview.cpp's sanitizeSelection():
	// > nothing selected to start with, so pick "My Inventory" as best guess
	// > new_selection = getItemByID(gAgent.getInventoryRootID());
	// so items.size() is always >= 1

	llinfos << "selection changed: num of items = " << items.size() << " and search mode = " << self->mInSearchMode << llendl;
	
	// indicates if we have multiple things selected (folders/items)
	bool multiple_selection = items.size() != 1;

	// if no items are selected -- doesnt happen, but just in case..
	if( ! items.size() )
	{
		llinfos << "LLFloaterManageLandmark::onSelectionChange(); nothing selected!" << llendl;
		return;
	}

	LLFolderViewItem* selected_item = items.front();

	if ( LLViewerInventoryCategory* inventory_folder = gInventory.getCategory( selected_item->getListener()->getUUID() ) ) // this is a folder
	{
		self->mSelectedFolderId = inventory_folder->getUUID();
		self->mSelectedFolder = inventory_folder->getName();
		self->mInSearchMode = FALSE; // no longer in search mode; we have a selected folder
		
		// no landmark item is selected
		self->mLandmarkInvItem = NULL;
		self->mSelectedFolderInvItem = inventory_folder;

		// disable teleport button for folders
		self->mFlyoutButtonTeleport->setEnabled( FALSE );
	
		// disable delete button for certain folders
		BOOL allow_deletion = ! self->isCategoryProtected( inventory_folder );
		self->mButtonDelete->setEnabled( allow_deletion );
		self->mButtonEdit->setEnabled( allow_deletion );

		// cant create folders inside Library, or when we have multiple items selected
		// or when we are searching for something that doesnt exist, and llfolderview sets selection to "My Inventory"
		if( find_root_folder( inventory_folder )->getName() == "Library" || inventory_folder->getName() == "My Inventory" )
			self->mButtonCreateNewFolder->setEnabled( FALSE );
		else
			self->mButtonCreateNewFolder->setEnabled( TRUE );
			
	}
	else
	if ( LLViewerInventoryItem* inventory_item = gInventory.getItem( selected_item->getListener()->getUUID() ) ) // this is an item
	{
		self->mSelectedFolderId = inventory_item->getParentUUID();	
		self->mSelectedFolder = "";
		self->mInSearchMode = FALSE;

		// a landmark is selected
		self->mLandmarkInvItem = inventory_item;
		self->mSelectedFolderInvItem = NULL;

		// enable teleport and edit buttons for landmarks
		self->mFlyoutButtonTeleport->setEnabled( TRUE );

		// disable delete/edit button for certain items
		BOOL allow_modification = ! self->isItemProtected( inventory_item );
		self->mButtonDelete->setEnabled( allow_modification );
		self->mButtonEdit->setEnabled( allow_modification );

		// cant create folders inside Library
		if( find_root_folder( gInventory.getCategory( inventory_item->getParentUUID() ) )->getName() == "Library" )
			self->mButtonCreateNewFolder->setEnabled( FALSE );
		else
			self->mButtonCreateNewFolder->setEnabled( TRUE );			
	} 
	else 
	{
		llwarns << "Invalid selection" << llendl;
		return;
	}

	// only enable teleport/edit buttons when exactly one item/folder is selected
	// this overrides the settings set above
	if ( multiple_selection ) {
		self->mFlyoutButtonTeleport->setEnabled( FALSE );
		self->mButtonEdit->setEnabled( FALSE );
		self->mButtonCreateNewFolder->setEnabled( FALSE );
	}

	// needed to do inline item/fodlder renaming
	if( self->mLandmarkPanel )
	{
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

void LLFloaterManageLandmark::onClickDelete( void* user_data )
{
	gViewerWindow->alertXml( "ManageLandmark", LLFloaterManageLandmark::onDeleteDialog, user_data );
}

void LLFloaterManageLandmark::onDeleteDialog(S32 option, void* data)
{
	LLFloaterManageLandmark* self = (LLFloaterManageLandmark*)data;
	switch( option )
	{
		case 0:  // "ok"
		{
			self->doDelete();
		} break;

		case 1:  // "cancel"
		break;

		default:
		break;
	}	
}

/////////////////////////////////////////////////
// edit menu functionality
// delete/copy/cut/paste

BOOL LLFloaterManageLandmark::canDoDelete() const
{
	// check if we have an item to delete
	if( mLandmarkInvItem )
		return ! isItemProtected( mLandmarkInvItem );

	// check if we have a folder to delete
	if( mSelectedFolderInvItem )
		return ! isCategoryProtected( mSelectedFolderInvItem );

	return FALSE;
}

//
// a way to easily lock an inventory item so its not removed/edited/moved by the user
// useful for editing landmarks 
class LLInventoryItemLocks : public LLSingleton<LLInventoryItemLocks>
{
	LLInventoryItemLocks()
	{

	}

	void Lock( LLUUID id )
	{
		S32 inv_item_idx = mLockedUUIDs.find( id );
		if( inv_item_idx >= 0 )
		{	// already locked
			llinfos << "already locked" << llendl;
			return;
		}

		llinfos << "locking " << id << llendl;
		mLockedUUIDs.push_back( id );
	}

	void Unlock( LLUUID id )
	{
		S32 inv_item_idx = mLockedUUIDs.find( id );

		if( inv_item_idx != LLDynamicArray<LLUUID>::FAIL )
		{
			llinfos << "unlocking " << id << llendl;
			mLockedUUIDs.remove( inv_item_idx );
		}
	}

	bool IsLocked( LLUUID id )
	{
		S32 inv_item_idx = mLockedUUIDs.find( id );

		return inv_item_idx != LLDynamicArray<LLUUID>::FAIL;
	}

	LLDynamicArray<LLUUID> mLockedUUIDs;
};

void LLFloaterManageLandmark::doDelete()
{
	if( ! canDoDelete() )
		return;

	// check that the selected file is not being edited/received
	LLFolderView* folders = mLandmarkPanel->getRootFolder();
	folders->removeSelectedItems();
}

void LLFloaterManageLandmark::onFocusReceived()
{
	LLFloater::onFocusReceived();
}

// use the focusing of the landmark manager as an event to hook into the edit menu handling functionality
// otherwise any kb shortcuts will get either handled by the folderview, which will delete
// our landmarks without flinching
void LLFloaterManageLandmark::setFocus( BOOL new_state )
{
	BOOL old_state = hasFocus();

	// Don't change anything if the focus state didn't change
	if (new_state == old_state) return;

	LLFloater::setFocus( new_state );

	if( new_state )
	{
		// Route menu to this class
		gEditMenuHandler = this;
	}
	else
	{
		// Route menu back to the default
		if( gEditMenuHandler == this )
			gEditMenuHandler = NULL;
	}
}


void LLFloaterManageLandmark::refreshAll()
{
	//LLFloaterManageLandmark::getInstance()->refresh();
}

std::string build_sl_url( LLString region_name, LLLandmark* landmark )
{
	LLVector3d pos;
	if( ! landmark->getGlobalPos(pos) )
	{	// this landmark doesnt have a global position!? should probably throw or something
		llwarns << "create_sluri(); landmark does not have a valid global position!" << llendl;
		return "";
	}

	S32 agent_x = llround( (F32)fmod( pos.mdV[VX], (F64)REGION_WIDTH_METERS ) );
	S32 agent_y = llround( (F32)fmod( pos.mdV[VY], (F64)REGION_WIDTH_METERS ) );
	S32 agent_z = llround( (F32)pos.mdV[VZ] );

	return LLURLDispatcher::buildSLURL( region_name, agent_x, agent_y, agent_z );
}

// static gui callbacks

//static
void LLFloaterManageLandmark::onClickEditLandmark(void* data)
{
	LLFloaterManageLandmark* self = static_cast<LLFloaterManageLandmark*>( data );

	if( self->mLandmarkInvItem )
	{
		edit_landmark( self->mLandmarkInvItem );
	}
	else
	{
		LLFolderView* folders = self->mLandmarkPanel->getRootFolder();
		folders->startRenamingSelectedItem();
	}
}

//static
void LLFloaterManageLandmark::onClickNewFolder(void* data)
{	
	LLFloaterManageLandmark* self = (LLFloaterManageLandmark*)data;

	if( self->mSelectedFolderId == LLUUID::null )
		self->mSelectedFolderId = gInventory.findCategoryUUIDForType( LLAssetType::AT_LANDMARK );
		
	LLUUID category = gInventory.createNewCategory( self->mSelectedFolderId, LLAssetType::AT_NONE, NULL );
	gInventory.notifyObservers();

	self->mLandmarkPanel->unSelectAll();
	self->mLandmarkPanel->setSelection( category, TRUE );
	self->mLandmarkPanel->getRootFolder()->setNeedsAutoRename( TRUE );
}

void LLFloaterManageLandmark::onTeleportFlyoutAction(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterManageLandmark* self = static_cast<LLFloaterManageLandmark*>( user_data );
	LLFlyoutButton* flyout = static_cast<LLFlyoutButton*>( ctrl );

	if( self->mInSearchMode )
		return;	

	if(self->mLandmarkInvItem)
	{
		if(LL_MANAGE_LANDMARK_FLOATER_MENU_ITEM == flyout->getSelectedItemLabel())
			copy_slurl( self->mLandmarkInvItem );
		else
		if(LL_MANAGE_LANDMARK_FLOATER_MAP_ITEM == flyout->getSelectedItemLabel())
			show_on_map( self->mLandmarkInvItem );
		else
			gAgent.teleportViaLandmark( self->mLandmarkInvItem->getAssetUUID() );
	}
}
