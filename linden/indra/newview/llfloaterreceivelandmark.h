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

#ifndef LL_FLOATERRECEIVELANDMARK_H
#define LL_FLOATERRECEIVELANDMARK_H

#include "llfloater.h"
#include "lluictrl.h"
#include "llviewerimagelist.h"
#include "lllandmark.h"
#include <set>
#include <vector>
#include <list>
#include "lllandmarklist.h"
#include "llinventorymodel.h"
#include "lllandmarkcommon.h"
#include "llgenericmultiplexer.h"
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


class LLPanelLandmarkDetails;
class LLPanelPlace;

class LLFloaterReceiveLandmark : public LLFloaterCommonLandmark
{     
public:

    LLFloaterReceiveLandmark();
    ~LLFloaterReceiveLandmark();

	void startBuilding();

    virtual BOOL postBuild();
    virtual void onClose(bool app_quitting);
    
    static void onSaveDialog(S32 option, void* data);
   	
    static BOOL sLandmarkAdded;
	BOOL mSaved;
	
	void showLandInfo();

	static void onGetNewLandmarkInfo( LLVFS *vfs, const LLUUID& uuid, LLAssetType::EType type, void* user_data, S32 status, LLExtStat ext_status );

	/// setters and getters

	//LLUUID getLandmarkAssetUUID(); // gets the asset id of the landmark (multiple inventory items can point to the same asset uuid)
	LLUUID getInventoryItemUUID(); // unique per inventory item -- gets the unique id of the temporary inventory item we are editing

	void toggleInteractiveContent( BOOL Enable );

private:

    static void onClickTakeSnapshot(void* data);
    static void onClikUseSnapshot(void* data);
    static void onClickSave(void* data);
    static void onClickCancel(void* data);
    static void onClickCreateFolder(void* data);
    static void onSelectItem(LLUICtrl* ctrl, void* user_data);

protected:

    static LLParcelSelectionObserver* sObserver;
};

#endif
