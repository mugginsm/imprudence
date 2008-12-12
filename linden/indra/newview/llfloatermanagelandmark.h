/** 
* @file llfloatermanagelandmark.h
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
#include "llfloater.h"
#include "lluictrl.h"
#include "llfolderview.h"
#include "llpermissionsflags.h"
#include "lltexturectrl.h"
#include "lltextbox.h"
#include <vector>
#include "llinventory.h"
#include "llinventoryview.h"
#include "llinventorymodel.h"
#include "lllandmark.h"
#include "llcombobox.h"

class LLButton;
class LLFloaterTexturePicker;
class LLInventoryItem;
class LLTextBox;
class LLViewBorder;
class LLFolderViewItem;
class LLSearchEditor;
class LLInventoryPanel;
class LLSaveFolderState;
class LLViewerImage;

class LLFloaterManageLandmark: public LLFloater, public LLFloaterSingleton<LLFloaterManageLandmark>, LLEditMenuHandler
{
 public:
	LLFloaterManageLandmark(const LLSD& seed);
	~LLFloaterManageLandmark(void);

	virtual BOOL postBuild();
	
	static void refreshAll();

	static void onDeleteDialog(S32 option, void* data);

	static bool getInDrag() { return m_DraggingItem; }
	static void setInDrag( bool Dragging ) { m_DraggingItem = Dragging; }

	virtual void setFocus( BOOL new_state );
	virtual void onFocusReceived();

	void clearSearch();

	static BOOL floaterOpened;

protected:

	// gui event callbacks
	static void onSearchEdit(const LLString& search_string, void* user_data);
	static void onClearSearch(void* user_data);
	static void onClickNewFolder(void* data);
	static void onClickEditLandmark(void* data);
	static void	onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action, void* data);
	static void onClickDelete(void* data);
	static void onTeleportFlyoutAction(LLUICtrl* ctrl, void* user_data);

	// indicates if a folder or item are being drag and dropped from the landmark manager
	// used to bypass activating the inventory panel during drag and drop
	static BOOL m_DraggingItem;

	// if set to true, calls startRenamingSelectedItem()
	// used to delay renaming a new folder into the next frame
	BOOL mInSearchMode;
	LLString mSelectedFolder;
    
	LLInventoryPanel* mLandmarkPanel;
	LLSearchEditor* mSearchEdit;
	LLSaveFolderState* mSavedFolderState;
	LLViewerInventoryCategory* mSelectedFolderInvItem;
	LLViewerInventoryItem* mLandmarkInvItem;
	LLLandmark* mLandmark;
	LLInventoryPanel* mActivePanel;
	LLUUID mSelectedFolderId;

	LLButton* mButtonEdit;
	LLButton* mButtonDelete;
	LLButton* mButtonCreateNewFolder;
	LLFlyoutButton* mFlyoutButtonTeleport;

	BOOL isCategoryProtected( LLViewerInventoryCategory* inventory_folder ) const;
	BOOL isItemProtected( LLViewerInventoryItem* inventory_folder ) const;

	typedef std::deque<LLFolderViewItem*> selected_items_t;
	selected_items_t mSelectedItems;
	LLHandle<LLFloater> mFinderHandle;

	// implement edit menu functionality handlers
	virtual void doDelete();
	virtual BOOL canDoDelete() const;
};
