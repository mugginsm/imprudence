/** 
* @file llnavbar.h
* @brief Navigation buttons.
*
* $LicenseInfo:firstyear=2002&license=viewergpl$
* 
* Copyright (c) 2002-2008, Linden Research, Inc.
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
#include "locationinfo.h"
#include "llfolderview.h"
#include "llpermissionsflags.h"
#include "lltexturectrl.h"
#include "lltextbox.h"
#include <vector>
#include "llinventory.h"
#include "llinventoryview.h"
#include "llinventorymodel.h"
#include "lllandmark.h"
#include "llagent.h"
#include "lllineeditor.h"

#ifndef LL_LLNAVBAR_H
#define LL_LLNAVBAR_H

#include "llpanel.h"

#include "llframetimer.h"
#include "llcombobox.h"
#include "llresizehandle.h"
#include "llscrolllistctrl.h"

class LLMenuGL;

/*
* Navbar specific ComboBox implementation
* inherits all of LLComboBox's behavior
* overrides the mButton and mArrowImage related behaviors
* this provides for a more unique navbar
*/
class LLNavbarComboBox : public LLComboBox
{
public:

	LLNavbarComboBox(	const std::string& name, const LLRect &rect, const std::string& label,
		void (*commit_callback)(LLUICtrl*,void*), void *callback_userdata
		);

	virtual ~LLNavbarComboBox();

	virtual void updateLayout();

	LLScrollListItem* add(const std::string& name, EAddPosition pos = ADD_BOTTOM, BOOL enabled = TRUE);	// add item "name" to menu
	// unhide the other inherited overloads of add()
	using LLComboBox::add;

	void setFocus(BOOL b);

	void clearHistory();
	void loadHistory();
	void showList();

	static void onItemSelected(LLUICtrl* item, void *userdata);

	BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);
	BOOL handleKeyHere(KEY key, MASK mask);
	BOOL handleUnicodeCharHere(llwchar uni_char);

	static void onPrearrange( LLUICtrl* ctrl, void* userdata );

	void setCursor(S32 pos) {
		mTextEntry->setCursor(pos);
	}
	void setCursorToEnd() {
		mTextEntry->setCursorToEnd();
	}
	void cut() {
		mTextEntry->cut();
	}
	void copy() {
		mTextEntry->copy();
	}
	void paste() {
		mTextEntry->paste();
	}
	void setKeyboardFocus() {
		mTextEntry->setFocus(TRUE);
		mTextEntry->selectAll();
	}
	void deleteSelection() {
		mTextEntry->deleteSelection();
	}

	// check for the existance of the item with "name"
	BOOL isExist(const std::string& name)
	{
		BOOL found = FALSE;
		
		if ( mList->selectItemByLabel( name ) )
			found = TRUE;

		return found;
	}

	// context menu callback signature
	typedef LLMenuGL* (*ContextMenuCallback)( LLUICtrl*, void* );
	ContextMenuCallback mContextMenuCallback;
	void setContextMenuCallback( ContextMenuCallback Callback ) { mContextMenuCallback = Callback; }

	// static
	static LLView* fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

	BOOL showSlurl;
};

class LLImageFlyoutButton : public LLFlyoutButton
{
public:
	LLImageFlyoutButton(
		const std::string& name, 
		const LLRect &rect,
		const std::string& label,
		void (*commit_callback)(LLUICtrl*, void*) = NULL,
		void *callback_userdata = NULL);

	virtual void	updateLayout();
	virtual void	draw();

	void			showList();
	void			hideList();

	virtual BOOL	handleMouseDown(S32 x, S32 y, MASK mask);

	static LLView*	fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

	BOOL					mListState;
	S32						nthItem;

protected:
	LLButton*				mImageButton;
	BOOL					mToggleState;
};

class LLNavBar
	:	public LLPanel
{
public:
	LLNavBar();
	~LLNavBar();

	/*virtual*/ BOOL postBuild();

	/*virtual*/ void reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);

	void setFoundRegionInfo(U64 region_handle, const std::string& region_url);
	static void backButtonDialog(S32 option, void* data);
	static void nextButtonDialog(S32 option, void* data);
	static void homeButtonDialog(S32 option, void* data);

	// Move buttons to appropriate locations based on rect.
	void layoutButtons();

	// path to the xml file where we store some cross-session persistent data like navigation history
	static std::string getLocalStoragePath();

	// Per-frame refresh call
	void refresh();
        
	static void startToggle();
	static void stopToggle();
	static void onAgentTeleporting(LLAgent::ETeleportState teleport_state, const std::string& slurl);
	static void updateSLURL(const std::string& slurl);
	static void setBackList(LLSD backlist);
	static void getBackList(LLSD& backlist);
	static BOOL opened;
	static LLSD teleportHistory;
	void clearHistory();
	void cullHistory();
	void clearAddressBar();
	void clearAddressBarHistory();
	static BOOL is_moving;
	void setAddressBar(std::string str);
	std::string getAddressBar();
	LLNavbarComboBox* mSlurlEdit;

	// teleport/search for whatever is currently in the addressbar
	void teleportToCurrentSLURL();
     
private:
	static void onClickBack(void *userdata);
	static void onClickNext(void *userdata);
	static void onClickHome(void *userdata);
	static void onClickTeleport(void *userdata);
	static void onClickMore(LLUICtrl* ctrl, void* user_data); 
	static void onChangeSlurlEdit(LLUICtrl* ctrl, void* user_data);
	static void slurlEditTextEntryCallback(LLLineEditor* caller, void* user_data);
	static LLMenuGL* slurlContextMenuCallback(LLUICtrl* caller, void* user_data);
	static void namedRegionRequestCallback(U64 region_handle, const std::string& url, const LLUUID& snapshot_id, bool teleport);
	static void onHaveRegionInfoFinishTeleport(U64 region_handle, const std::string& url, const LLUUID& snapshot_id, bool teleport);

	void onAgentTeleportMoving();
	void updateHistoryList();

	void writeToFile(std::string name, LLURI slurl, LLUUID regionId);
	void writeToFile();
	void loadFromFile();
    void teleportCore(const std::string& address, bool from_history = false);
	std::vector<LocationInfo> regionBackHistory;
	std::vector<LocationInfo> regionNextHistory;
	bool getCurrentLocationInfo(LocationInfo& info) const;

	static unsigned int counter;
	static double sCullTime;
    static double mCullTime;
	static LLSD history;
    static LLSD searchHistory;
	int getDate(std::string dateString);
protected:
	static LLImageFlyoutButton* mFlyout;
	
	U32	mMinSearchChars;
	U64 mFoundRegionHandle;
	std::string mFoundRegionURL;

	// If we teleport from Next/Back buttons, we have to ignore it
	// To keep the bookkeeping correct.
	bool mIgnoreNextTeleport;
};

extern LLNavBar *gNavBar;
void init_navbar_actions(LLNavBar *navbar);

#endif
