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


#include "llfloater.h"
#include "lluictrl.h"
#include "llviewerimagelist.h"
#include "lllandmark.h"
#include <set>
#include <vector>
#include "lllandmarklist.h"
#include "llinventorymodel.h"
#include "lllandmarkcommon.h"
#include "llfloaterselectfolder.h"

typedef std::set<LLUUID, lluuid_less> uuid_list_t;


class LLButton;
class LLCheckBoxCtrl;
class LLRadioGroup;
class LLComboBox;
class LLNameListCtrl;
class LLSpinCtrl;
class LLLineEditor;
class LLRadioGroup;
class LLParcelSelectionObserver;
class LLTabContainer;
class LLTextBox;
class LLTextEditor;
class LLTextureCtrl;
class LLViewerTextEditor;
class LLParcelSelection;


class LLPanelLandDetails;
class LLPanelLandInformation;
class LLPanelPlace;

class LLFloaterAddLandmark : public LLFloaterCommonLandmark
{     
public:
	
	LLFloaterAddLandmark();
	virtual ~LLFloaterAddLandmark();
	
	void beginBuild();
	virtual BOOL postBuild();

	virtual void onClose(bool app_quitting);
		
	static void onSaveDialogResponse(S32 option, void* data);

	static BOOL sLandmarkAdded;

	BOOL mClosingFromNavbar;

	// callback; routes event to onLandmarkCreated()
	static void onLandmarkCreatedWrapper( LLVFS *vfs, const LLUUID& uuid, LLAssetType::EType type, void* user_data, S32 status, LLExtStat ext_status );
	// saves the landmark after it has been created and fetched from the server
	void onLandmarkCreated( LLLandmark* landmark );

	virtual void toggleInteractiveContent( BOOL Enable );
	virtual void showLandInfo();

private:

	static void onClickTakeSnapshot(void* data);
	static void onClikUseSnapshot(void* data);
	static void onClickSave(void* data);
	static void onClickCancel(void* data);
	static void onClickCreateFolder(void* data);

	LLParcelSelectionObserver* sObserver;

protected:

	BOOL mSafeToClose; // if this is TRUE, don't show a confirmation dialog when attempting to close the floater
};
