/** 
 * @file llfloaterselectfolder.h
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

#ifndef LL_FLOATERSELECTFOLDER_H
#define LL_FLOATERSELECTFOLDER_H

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

// multiple select folder objects can be created for each floater

class LLFloaterSelectFolder : public LLFloater
{
public:

	struct LLSelectedFolder
	{
		BOOL folder_chosen;	// whether folder was chosen or user hit cancel
		LLUUID folder_id;
		LLString folder_name;
	};

	// called when a folder is selected
	typedef boost::function1<void,LLSelectedFolder> FolderSelectCallback;
	// called when this dialog is closing (to allow the parent to clean up)
	typedef boost::function0<void> OnExitCallback;

	LLFloaterSelectFolder();
	~LLFloaterSelectFolder();

	void setOnFolderSelectCallback( FolderSelectCallback Callback );
	void setOnExitCallback( OnExitCallback Callback );

    virtual BOOL postBuild();
	virtual void onClose(bool app_quitting);

private:
	
	static void onClickAdd(void* data);
	static void onClickCancel(void* data);
	static void onClickSelect(void* data);
	static void	onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action, void* data);
    
	LLInventoryPanel* mLandmarkPanel;
	LLInventoryCategory* mSelectedFolder;

	FolderSelectCallback mFolderSelectCallback;
	OnExitCallback mOnExitCallback;
};

#endif
