/** 
* @file llnavbar.cpp
* @author Vectorform
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

#include "llviewerprecompiledheaders.h"

#include "llnavbar.h"

#include "imageids.h"
#include "llfontgl.h"
#include "llrect.h"
#include "llparcel.h"

#include "llagent.h"
#include "llbutton.h"
#include "llfocusmgr.h"
#include "llviewercontrol.h"
#include "llmenucommands.h"
#include "llimview.h"
#include "lluiconstants.h"
#include "llvoavatar.h"
#include "lltooldraganddrop.h"
#include "llinventoryview.h"
#include "llfloaterchatterbox.h"
#include "llfloaterfriends.h"
#include "llfloatersnapshot.h"
#include "lltoolmgr.h"
#include "llui.h"
#include "llviewermenu.h"
#include "llfirstuse.h"
#include "llviewerparcelmgr.h"
#include "lluictrlfactory.h"
#include "lltoolgrab.h"
#include "llcombobox.h"
#include "llfloaterchat.h"
#include "llfloatermute.h"
#include "llimpanel.h"
#include "llscrolllistctrl.h"
#include "llfloaterland.h"

#include "llviewerregion.h"
#include "lltexteditor.h"
#include "lluiconstants.h"
#include "lllandmark.h"
#include "lllandmarklist.h"
#include "llurldispatcher.h"
#include "locationinfo.h"
#include "llurlsimstring.h"
#include "llview.h"

#include "llvoavatar.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "lltexteditor.h"
#include "lluiconstants.h"
#include "lllandmark.h"
#include "lllandmarklist.h"
#include "llurldispatcher.h"
#include "locationinfo.h"
#include "llurlsimstring.h"
#include "llview.h"
#include "llsdserialize.h"
#include "llscrolllistctrl.h"
#include "llworldmap.h"
#include "llcommandhandler.h"
#include "llfloaterurldisplay.h"
#include "llfloaterdirectory.h"
#include "llfloaterhtml.h"
#include "llfloaterworldmap.h"
#include "llfloaterhtmlhelp.h"
#include "llpanellogin.h"
#include "llstartup.h"			// gStartupState
#include "llurlsimstring.h"
#include "llviewerwindow.h"		// alertXml()
#include "llweb.h"
#include "llworldmap.h"
#include "lllineeditor.h"
#include "llnavigationlist.h"
#include "llviewermessage.h"
#include "llappviewer.h"
#include "llinventoryclipboard.h"
#include <ctime>
#include "llstatusbar.h"
#include "llviewerregion.h"
#include "llfloateraddlandmark.h"

class LLLineEditor;

typedef LLMemberListener<LLNavBar> navbar_listener_t;

//Local Constants
const std::string LL_NAV_BAR_COMBOBOX_NAME("Slurl_edit");
const std::string LL_NAV_BAR_SCROLLLIST_NAME("location_list");
const std::string LL_PATH_HISTORY_FILE("locationhistory.xml");

const std::string LL_NAV_BAR_HISTORY_LIST("more_btn");

const std::string LL_NAVBAR_PERSISTENT_DATA( "navbar_history.xml" );
const int LL_NAVBAR_PERSISTENT_HISTORY_SIZE = 5;

// *TODO: This needs to be localized so we need to move it into the 
// localized string resources.

unsigned int LLNavBar::counter = 0;

BOOL LLNavBar::opened = FALSE;
BOOL isFileOpen = FALSE;
BOOL LLNavBar::is_moving = FALSE;
std::string addressToAdd = "";
double LLNavBar::sCullTime = 4; //default four days
LLSD LLNavBar::history;
LLSD LLNavBar::teleportHistory;
LLSD LLNavBar::searchHistory;

LLImageFlyoutButton* LLNavBar::mFlyout = NULL; 

static void onClickParcelInfo(void*);

//////////////////////////////////////////////////////////////////////////
// LLLocallyPersistentHelper
// helper used to easily load/save data to a local xml file
// works well as a base class

class LLLocallyPersistentHelper
{
public:

	LLLocallyPersistentHelper( std::string Path )
	{
		mPath = Path;
	}

	virtual ~LLLocallyPersistentHelper()
	{
	}

	virtual BOOL load()
	{
		llifstream file( mPath.c_str() );
		if( ! file.is_open() )
		{
			// TODO: add an alertxml() call here
			llwarns << "LLLocallyPersistentHelper::load(); error opening file: " << mPath << llendl;
			return FALSE;
		}

		LLSDSerialize::fromXML( mData, file );
		file.close();

		llinfos << "LLLocallyPersistentHelper::load(); loaded file: " << mPath << llendl;
		return TRUE;
	}

	virtual BOOL save()
	{
		// this shouldnt ever fail, unless the file is being used by a diff app that is claiming exclusive rights to it
		llofstream file( mPath.c_str() );
		if( ! file.is_open() )
		{
			// TODO: add an alertxml() call here
			llwarns << "LLLocallyPersistentHelper::save(); error opening file: " << mPath << llendl;
			return FALSE;
		}
		
		LLSDSerialize::toPrettyXML(mData, file);
		file.close();

		llinfos << "LLLocallyPersistentHelper::save(); saved file: " << mPath << llendl;

		return TRUE;
	}

	LLSD operator[](const std::string& key)
	{
		return mData.get( key );
	}

	bool hasValue( std::string name )
	{
		return mData.has( name );
	}

protected:

	std::string mPath;
	LLSD mData;
};

// navbar specific persistent data helper
class LLNavBarPersistentData : public LLLocallyPersistentHelper
{
public:

	struct LLNavBarAddressEntry
	{
		std::string Address;
		LLDate Date;
	};

	LLNavBarPersistentData() : LLLocallyPersistentHelper( LLNavBar::getLocalStoragePath() )
	{
		load();
	}
	
	virtual ~LLNavBarPersistentData()
	{
	}

	// gets navbar input field history
	LLSD getBackLocations()
	{
		return mData.get("navbar_backtrack");
	}

	void updateBackLocations( LLSD locations )
	{
		mData["navbar_backtrack"] = locations;
	}

	void printBackLocations()
	{
		LLSD back_locations = getBackLocations();

		llinfos << "visit history has " << back_locations.size() << " entries" << llendl;
		for( int i = 0; i < back_locations.size(); i ++ )
		{
			llinfos << i << " = " << back_locations[i]["address"].asString() << " : " << back_locations[i]["date"] << llendl;
		}
	}

	// removes entries from the back locations -- effectively clearing out the addressbar history
	// to have this reflected in our xml file, we need to call save() afterwards
	void clearBackLocations()
	{
		mData["navbar_backtrack"] = LLSD::emptyArray();
	}
	
	// maintain a respectable length
	// UPDATES XML
	void trimBackLocations()
	{
		LLSD back_locations = getBackLocations();

		int MaxAge = gSavedSettings.getU32("AddressBarMaxHistoryAge" );

		llinfos << "MaxAge = " << MaxAge << " num of locations: " << back_locations.size() << llendl;

		F64 right_now_seconds = totalTime() / 1e6;

		LLSD filtered_locations;

		//for( LLSD::map_iterator iter = back_locations.beginMap(); iter != back_locations.endMap(); )
		for( int i = 0; i < back_locations.size(); i ++ )
		{
			LLSD entry = back_locations.get(i);

			F64 age_in_seconds = right_now_seconds - entry["date"].asDate().secondsSinceEpoch();

			//llinfos << entry["address"] << " from " << entry["date"] << " is " << age_in_seconds / 86400 << " days old" << llendl;

			if( age_in_seconds > MaxAge * 86400 )
			{	// time to remove this entry!
				//llinfos << "  -> removing entry" << llendl;
			} else
			{
				//llinfos << "  -> keeping entry" << llendl;
				filtered_locations.append( back_locations[i] );
			}
		}

		updateBackLocations( filtered_locations );
	}

	// UPDATES XML
	void addBackLocation( std::string address )
	{
		LLSD back_locations = getBackLocations();

		LLDate now( totalTime() / 1e6 );

		LLSD entry;
		entry["address"] = address;
		entry["date"] = now.asString();

		back_locations.append( entry );

		updateBackLocations( back_locations );
	}

protected:

};

//////////////////////////////////////////////////////////////////////////
// LLNavbarComboBox
//

// register the widget so it can be accessed via xui
static LLRegisterWidget<LLNavbarComboBox> r1("navbar_combo_box");

LLNavbarComboBox::~LLNavbarComboBox()
{

}

LLNavbarComboBox::LLNavbarComboBox(	const std::string& name, const LLRect &rect, const std::string& label,
									void (*commit_callback)(LLUICtrl*,void*), void *callback_userdata )
:	LLComboBox( name, rect, label, commit_callback, callback_userdata ), mContextMenuCallback(NULL),
	showSlurl( FALSE )
{
	// since there is no empty default constructor, we redo everything that the LLComboBox ctr did
	removeChild( mButton, TRUE );
	removeChild( mList, TRUE );

	// this is going to be treated as a 20x20 image
	mButton = new LLButton(label, LLRect(),  LLStringUtil::null, NULL, this);
	mButton->setImageUnselected("navbar_input_field_chevron.tga");
	mButton->setImageSelected("navbar_input_field_chevron.tga");
	mButton->setImageDisabled("navbar_input_field_chevron.tga");
	mButton->setImageDisabledSelected("navbar_input_field_chevron.tga");
	mButton->setScaleImage(TRUE);
	mButton->setDropShadowedText(FALSE);

	mButton->setMouseDownCallback(onButtonDown);
	mButton->setFont(LLFontGL::sSansSerifSmall);
	mButton->setFollows(FOLLOWS_BOTTOM | FOLLOWS_LEFT);
	mButton->setHAlign(LLFontGL::RIGHT);
	addChild(mButton);

	// disallow multiple selection
	mList = new LLScrollListCtrl( "ComboBox", LLRect(), &LLNavbarComboBox::onItemSelected, this, FALSE);
	mList->setVisible(FALSE);
	mList->setBgWriteableColor( LLColor4(1,1,1,1) );
	mList->setCommitOnKeyboardMovement(FALSE);
	addChild(mList);

	// setButtonVisible() makes use of mArrowImage, and its not a virtual function so the best we can do is hide it
	// but a polymorphic call to it will find mArrowImage as a null ptr and cause a crash
	// so we keep mArrowImage around, but make no use of it
	mArrowImage = LLUI::sImageProvider->getUIImage("combobox_arrow.tga");

//	mButton->setImageOverlay("combobox_arrow.tga", LLFontGL::RIGHT);

	updateLayout();

	// hook up a pre-arrange function that gets called before the mList is evaluted for length and showList() called
	// this allows us to load/sort the data we need before the dropdown is activated
	setCallbackUserData( this );
	setPrearrangeCallback( onPrearrange );
}

bool mTeleportInitiated = false;

void LLNavbarComboBox::onItemSelected( LLUICtrl* item, void *userdata )
{
	if( mTeleportInitiated )
		return;

	LLComboBox::onItemSelected( item, userdata );

	// does a region info lookup if necessary otherwise teleports to slurl
	gNavBar->teleportToCurrentSLURL();
}

//static
void LLNavbarComboBox::onPrearrange( LLUICtrl* ctrl, void* userdata )
{
	// check if we are logged in, abort if we are not.
	if( LLStartUp::getStartupState() < STATE_WORLD_INIT )
	{
		return;
	}

	LLNavbarComboBox* self = dynamic_cast<LLNavbarComboBox*>( ctrl );

	static bool history_loaded = false;

	// if we got here, we are logged in, and will have the linden user dir set, so loadHistory() will work
	if( ! history_loaded )
	{
		history_loaded = true;
		self->loadHistory();
	}
}

void LLNavbarComboBox::updateLayout()
{
	LLRect rect = getLocalRect();
	if (acceptsTextInput())
	{
		mButton->setRect(LLRect( rect.mRight - 25, rect.mTop-1, rect.mRight - 5, rect.mTop-21));
		mButton->setTabStop(FALSE);

		if (!mTextEntry)
		{
			LLRect text_entry_rect(0, getRect().getHeight()-1, getRect().getWidth()-20, 0);
			text_entry_rect.mRight -= llmax(8,mArrowImage->getWidth()) + 2 * LLUI::sConfigGroup->getS32("DropShadowButton");
			
			std::string cur_label = mButton->getLabelSelected();
			
			mTextEntry = new LLLineEditor(
				"combo_text_entry", text_entry_rect, "", LLFontGL::sSansSerifSmall, 
				256, onTextCommit, onTextEntry, NULL, this, NULL,
				LLViewBorder::BEVEL_NONE, LLViewBorder::STYLE_LINE, 0
			);
			
			mTextEntry->setSelectAllonFocusReceived(TRUE); // when clicking on the LineEditor, text will be selected
			mTextEntry->setHandleEditKeysDirectly(TRUE);
			mTextEntry->setCommitOnFocusLost(FALSE);
			mTextEntry->setText(cur_label);
			mTextEntry->setIgnoreTab(TRUE);
			mTextEntry->setFollowsAll();

			// when text is selected, the inverse of this color will be used for drawing the selected text
			LLColor4 normalTextColor( .1f, .1f, .1f, 1 ); // a nice gray

			// ok so we want a transparent background but we also want to be able to select text
			// when the selected text is painted, the LineEditor control uses the inverse of the background color
			// the background of the navbar input field will be white, so we make this control have
			// a white background with zero alpha, meaning its transparent but the rbg values are 1,1,1
			// whereas LLColor4::transparent will have all zeroes, so when text is selected, the background
			// for it becomes white, and the white text becomes invisible or hard to read
			// selected text background color will be (1-r,1-g,1-b,1) relative to inverseOfSelectedBGColor
			LLColor4 inverseOfSelectedBGColor( .8f, .8f, .8f, 0 );

			mTextEntry->setWriteableBgColor( inverseOfSelectedBGColor );
			mTextEntry->setFocusBgColor( inverseOfSelectedBGColor );
			mTextEntry->setTentativeFgColor( normalTextColor );
			mTextEntry->setFgColor( normalTextColor );

			addChild(mTextEntry);
		}
		else
		{
			mTextEntry->setVisible(TRUE);
			mTextEntry->setMaxTextLength(256);
		}

		// no label needed for our image button
		setLabel(LLStringUtil::null);
	}
	else
	{
		mButton->setRect(rect);
		mButton->setTabStop(TRUE);

		if (mTextEntry)
		{
			mTextEntry->setVisible(FALSE);
		}
		mButton->setFollowsAll();
	}
}

BOOL LLNavbarComboBox::handleKeyHere(KEY key, MASK mask)
{
	BOOL result = FALSE;
	if (hasFocus())
	{
		//give list a chance to pop up and handle key
		LLScrollListItem* last_selected_item = mList->getLastSelectedItem();
		if (last_selected_item)
		{
			// highlight the original selection before potentially selecting a new item
			mList->highlightNthItem(mList->getItemIndex(last_selected_item));
		}
		result = mList->handleKeyHere(key, mask);
		
		// we dont show list here (unlike the normal combobox)

		if(KEY_ESCAPE == key)
		{
			llinfos << "LLNavbarComboBox::handleKeyHere(); caught esc key; setting show sl url bool.." << llendl;
			showSlurl = TRUE;
			result = TRUE;
		}
	}

	return result;
}

BOOL LLNavbarComboBox::handleUnicodeCharHere(llwchar uni_char)
{
	BOOL result = FALSE;
	if (gFocusMgr.childHasKeyboardFocus(this))
	{
		// space bar just shows the list
		if (' ' != uni_char )
		{
			LLScrollListItem* last_selected_item = mList->getLastSelectedItem();
			if (last_selected_item)
			{
				// highlight the original selection before potentially selecting a new item
				mList->highlightNthItem(mList->getItemIndex(last_selected_item));
			}
			
			result = mList->handleUnicodeCharHere(uni_char);
			
			if (mList->getLastSelectedItem() != last_selected_item)
			{
				showList();
			}
		}
	}
	return result;
}

BOOL LLNavbarComboBox::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	llinfos << "LLNavbarComboBox::handleRightMouseDown(); show context menu!" << llendl;

	if( ! mContextMenuCallback )
	{
		llwarns << "LLNavbarComboBox::handleRightMouseDown(); mContextMenuCallback is not defined!" << llendl;
		return LLUICtrl::handleRightMouseDown(x, y, mask);
	}

	// dont show context menu if the combobox droplist is visible
	if( mList->getVisible() )
		return TRUE;

	static LLMenuGL* menu = NULL;

	if( menu )
	{	// menu previously created
		LLMenuGL::sMenuContainer->removeChild( menu, TRUE );
	}

//	if( ! menu )
	menu = this->mContextMenuCallback( this, mCallbackUserData );

	LLMenuGL::showPopup(this, menu, x, y);

	return TRUE;
}

void LLNavbarComboBox::setFocus(BOOL b)
{
	LLUICtrl::setFocus(b);

	if (b)
	{
		if (mList->getVisible())
		{
			mList->setFocus(TRUE);
		}
	}
}

S32 MAX_NAVBARCOMBOBOX_WIDTH = 500;

void LLNavbarComboBox::loadHistory()
{
	// load in last session's history into the dropdown
	// latest entries on top
	LLNavBarPersistentData data;
	data.printBackLocations();
	data.trimBackLocations();
	LLSD back_locations = data.getBackLocations();

	mList->clear();
	for( int i = 0; i < back_locations.size(); i ++ )
	{
		std::string address = back_locations[i]["address"].asString();
		//llinfos << "adding address to navbar: " << address << llendl;
		add(address, ADD_TOP);
	}

	data.save();
}

void LLNavbarComboBox::clearHistory()
{
	// clear address history in stored data
	LLNavBarPersistentData data;
	data.clearBackLocations();
	data.save();
	
	// clear in live list
	removeall();
}

void LLNavbarComboBox::showList()
{
	// Make sure we don't go off top of screen.
	LLCoordWindow window_size;
	getWindow()->getSize(&window_size);
	//HACK: shouldn't have to know about scale here
	mList->fitContents( 192, llfloor((F32)window_size.mY / LLUI::sGLScaleFactor.mV[VY]) - 50 );
	mList->calcColumnWidths();

	// Make sure that we can see the whole list
	LLRect root_view_local;
	LLView* root_view = getRootView();
	root_view->localRectToOtherView(root_view->getLocalRect(), &root_view_local, this);

	LLRect rect = mList->getRect();

	S32 min_width = getRect().getWidth() - 20;
	S32 max_width = llmax(min_width, MAX_NAVBARCOMBOBOX_WIDTH);
	min_width = 344;

	S32 list_width = llclamp(mList->getMaxContentWidth(), min_width, max_width);

	if (mListPosition == BELOW)
	{
		if (rect.getHeight() <= -root_view_local.mBottom)
		{
			// Move rect so it hangs off the bottom of this view
			rect.setLeftTopAndSize(0, 0, list_width, rect.getHeight() );
		}
		else
		{	
			// stack on top or bottom, depending on which has more room
			if (-root_view_local.mBottom > root_view_local.mTop - getRect().getHeight())
			{
				// Move rect so it hangs off the bottom of this view
				rect.setLeftTopAndSize(0, 0, list_width, llmin(-root_view_local.mBottom, rect.getHeight()));
			}
			else
			{
				// move rect so it stacks on top of this view (clipped to size of screen)
				rect.setOriginAndSize(0, getRect().getHeight(), list_width, llmin(root_view_local.mTop - getRect().getHeight(), rect.getHeight()));
			}
		}
	}
	else // ABOVE
	{
		if (rect.getHeight() <= root_view_local.mTop - getRect().getHeight())
		{
			// move rect so it stacks on top of this view (clipped to size of screen)
			rect.setOriginAndSize(0, getRect().getHeight(), list_width, llmin(root_view_local.mTop - getRect().getHeight(), rect.getHeight()));
		}
		else
		{
			// stack on top or bottom, depending on which has more room
			if (-root_view_local.mBottom > root_view_local.mTop - getRect().getHeight())
			{
				// Move rect so it hangs off the bottom of this view
				rect.setLeftTopAndSize(0, 0, list_width, llmin(-root_view_local.mBottom, rect.getHeight()));
			}
			else
			{
				// move rect so it stacks on top of this view (clipped to size of screen)
				rect.setOriginAndSize(0, getRect().getHeight(), list_width, llmin(root_view_local.mTop - getRect().getHeight(), rect.getHeight()));
			}
		}
	}
	mList->setOrigin(rect.mLeft, rect.mBottom);
	mList->reshape(rect.getWidth(), rect.getHeight());
	mList->translateIntoRect(root_view_local, FALSE);

	// Make sure we didn't go off bottom of screen
	S32 x, y;
	mList->localPointToScreen(0, 0, &x, &y);

	if (y < 0)
	{
		mList->translate(0, -y);
	}

	// NB: this call will trigger the focuslost callback which will hide the list, so do it first
	// before finally showing the list

	mList->setFocus(TRUE);

	// register ourselves as a "top" control
	// effectively putting us into a special draw layer
	// and not affecting the bounding rectangle calculation
	gFocusMgr.setTopCtrl(this);

	// Show the list and push the button down
	mButton->setToggleState(TRUE);
	mList->setVisible(TRUE);
	setUseBoundingRect(TRUE);
}

LLScrollListItem* LLNavbarComboBox::add(const std::string& name, EAddPosition pos, BOOL enabled)
{
	//llinfos << "LLNavbarComboBox::add(); calling the overriden version of add()! :D" << llendl;
	LLScrollListItem* item = mList->addSimpleElement(name, pos);
	return item;
}

// static
LLView* LLNavbarComboBox::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	llinfos << "LLNavbarComboBox::fromXML();" << llendl;

	std::string name("navbar_combo_box");
	node->getAttributeString("name", name);

	std::string label("");
	node->getAttributeString("label", label);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	BOOL allow_text_entry = TRUE;
	node->getAttributeBOOL("allow_text_entry", allow_text_entry);

	S32 max_chars = 20;
	node->getAttributeS32("max_chars", max_chars);

	LLUICtrlCallback callback = NULL;

	llinfos << "creating LLNavbarComboBox of size = " << rect.getWidth() << " x " << rect.getHeight() << llendl;

	LLNavbarComboBox* combo_box = new LLNavbarComboBox(name, rect,  label, callback, NULL);
	combo_box->setAllowTextEntry(allow_text_entry, max_chars);
	combo_box->initFromXML(node, parent);

	const std::string& contents = node->getValue();

	if (contents.find_first_not_of(" \n\t") != contents.npos)
	{
		llerrs << "Legacy combo box item format used! Please convert to <combo_item> tags!" << llendl;
	}
	else
	{
		LLXMLNodePtr child;
		for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
		{
			if (child->hasName("combo_item"))
			{
				std::string label = child->getTextContents();

				std::string value = label;
				child->getAttributeString("value", value);

				combo_box->add(label, LLSD(value) );
			}
		}
	}

	combo_box->selectFirstItem();

	return combo_box;
}


//////////////////////////////////////////////////////////////////////////
// LLImageFlyoutButton
//

static LLRegisterWidget<LLImageFlyoutButton> r3("image_flyout_button");

const S32 IMAGE_FLYOUT_BUTTON_ARROW_WIDTH = 20;

LLImageFlyoutButton::LLImageFlyoutButton(
	const std::string& name, 
	const LLRect &rect,
	const std::string& label,
	void (*commit_callback)(LLUICtrl*, void*) ,
	void *callback_userdata)
:	LLFlyoutButton(name, rect, LLStringUtil::null, commit_callback, callback_userdata),
	mImageButton(NULL), mListState( FALSE ), nthItem( 0 )
{

	/*
	removeChild( mButton, TRUE );

	mImageButton = new LLButton("", LLRect(), LLStringUtil::null, NULL, this);
	mImageButton->setScaleImage(FALSE);

	mImageButton->setClickedCallback(onActionButtonClick);
	mImageButton->setFollowsAll();
	mImageButton->setHAlign( LLFontGL::HCENTER );
	mImageButton->setLabel(label);
	addChild(mImageButton);
	*/

	mActionButton->setVisible( FALSE );
	mActionButton->setTabStop( FALSE );

	mExpanderButtonImage = LLUI::getUIImage("navbar_btn_history_default_25.tga");
	mExpanderButtonImageSelected = LLUI::getUIImage("navbar_btn_history_pullD_open.tga");

	mButton->setImageSelected(mExpanderButtonImageSelected);
	mButton->setImageUnselected(mExpanderButtonImage);

	mButton->setHoverImages("navbar_btn_history_over_25.tga", "navbar_btn_history_over_25.tga");
	mButton->setRightHPad(6);

//	setMouseOpaque( FALSE );

	updateLayout();
}

//static 
LLView* LLImageFlyoutButton::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string name = "image_flyout_button";
	node->getAttributeString("name", name);

	std::string label("");
	node->getAttributeString("label", label);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	LLUICtrlCallback callback = NULL;

	LLImageFlyoutButton* image_flyout_button = new LLImageFlyoutButton(name,
		rect, 
		label,
		callback,
		NULL);

	std::string list_position;
	node->getAttributeString("list_position", list_position);
	if (list_position == "below")
	{
		image_flyout_button->mListPosition = BELOW;
	}
	else if (list_position == "above")
	{
		image_flyout_button->mListPosition = ABOVE;
	}


	image_flyout_button->initFromXML(node, parent);

	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		if (child->hasName("image_flyout_button_item"))
		{
			std::string label = child->getTextContents();

			std::string value = label;
			child->getAttributeString("value", value);

			image_flyout_button->add(label, LLSD(value) );
		}
	}

	image_flyout_button->updateLayout();

	return image_flyout_button;
}

void LLImageFlyoutButton::updateLayout()
{
	LLComboBox::updateLayout();

	mButton->setName("More");
	mButton->setOrigin(getRect().getWidth() - IMAGE_FLYOUT_BUTTON_ARROW_WIDTH, 0);
	mButton->reshape(IMAGE_FLYOUT_BUTTON_ARROW_WIDTH, getRect().getHeight());
	mButton->setFollows(FOLLOWS_RIGHT | FOLLOWS_TOP | FOLLOWS_BOTTOM);
	mButton->setTabStop(FALSE);
	mButton->setImageOverlay(mListPosition == BELOW ? "" : "", LLFontGL::RIGHT);

//	mImageButton->setOrigin(0, 0);
//	mImageButton->reshape(getRect().getWidth() - IMAGE_FLYOUT_BUTTON_ARROW_WIDTH, 0);
//	mImageButton->setVisible(TRUE);

	setUseBoundingRect( FALSE );
	updateBoundingRect();
}

BOOL LLImageFlyoutButton::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// avoid recursion; gNavBar->handleMouseDown might end us up back here
	static bool bPassedToParent = false;

	if( bPassedToParent )
		return FALSE;

	if( mList->getBoundingRect().pointInRect( x, y ) || mButton->getRect().pointInRect( x, y ) )
	{
		return LLView::handleMouseDown( x, y, mask );
	}

	// ah, since we cant really violate LLView's bounding-rect pointInView() checks in LLViewerWindow
	// we can just pass along missed clicks to the navbar to see if it can make use of them
	// not a completely horrible alternative

	bPassedToParent = true;
	gNavBar->handleMouseDown( x + getRect().mLeft, y + getRect().mBottom, mask );
	bPassedToParent = false;

	return FALSE;
}

void LLImageFlyoutButton::hideList() {
	LLComboBox::hideList();
	mListState = FALSE;
}

S32 MAX_NAVBAR_MINIHISTORY_WIDTH = 500;

void LLImageFlyoutButton::showList()
{
	// Make sure we don't go off top of screen.
	LLCoordWindow window_size;
	getWindow()->getSize(&window_size);
	//HACK: shouldn't have to know about scale here
	mList->fitContents( 192, llfloor((F32)window_size.mY / LLUI::sGLScaleFactor.mV[VY]) - 50 );
	mList->calcColumnWidths();

	// Make sure that we can see the whole list
	LLRect root_view_local;
	LLView* root_view = getRootView();
	root_view->localRectToOtherView(root_view->getLocalRect(), &root_view_local, this);

	LLRect rect = mList->getRect();

	S32 min_width = getRect().getWidth() - 20;
	S32 max_width = llmax(min_width, MAX_NAVBAR_MINIHISTORY_WIDTH);
	min_width = 344;

	S32 list_width = llclamp(mList->getMaxContentWidth(), min_width, max_width);

	if (mListPosition == BELOW)
	{
		if (rect.getHeight() <= -root_view_local.mBottom)
		{
			// Move rect so it hangs off the bottom of this view
			rect.setLeftTopAndSize(0, 0, list_width, rect.getHeight() );
		}
		else
		{	
			// stack on top or bottom, depending on which has more room
			if (-root_view_local.mBottom > root_view_local.mTop - getRect().getHeight())
			{
				// Move rect so it hangs off the bottom of this view
				rect.setLeftTopAndSize(0, 0, list_width, llmin(-root_view_local.mBottom, rect.getHeight()));
			}
			else
			{
				// move rect so it stacks on top of this view (clipped to size of screen)
				rect.setOriginAndSize(0, getRect().getHeight(), list_width, llmin(root_view_local.mTop - getRect().getHeight(), rect.getHeight()));
			}
		}

		mListState=TRUE;
		if(nthItem!=0 && nthItem>0)
		{
			selectNthItem(nthItem);
		}
	}
	else // ABOVE
	{
		if (rect.getHeight() <= root_view_local.mTop - getRect().getHeight())
		{
			// move rect so it stacks on top of this view (clipped to size of screen)
			rect.setOriginAndSize(0, getRect().getHeight(), list_width, llmin(root_view_local.mTop - getRect().getHeight(), rect.getHeight()));
		}
		else
		{
			// stack on top or bottom, depending on which has more room
			if (-root_view_local.mBottom > root_view_local.mTop - getRect().getHeight())
			{
				// Move rect so it hangs off the bottom of this view
				rect.setLeftTopAndSize(0, 0, list_width, llmin(-root_view_local.mBottom, rect.getHeight()));
			}
			else
			{
				// move rect so it stacks on top of this view (clipped to size of screen)
				rect.setOriginAndSize(0, getRect().getHeight(), list_width, llmin(root_view_local.mTop - getRect().getHeight(), rect.getHeight()));
			}
		}
		mListState=FALSE;
	}
	mList->setOrigin(rect.mLeft, rect.mBottom);
	mList->reshape(rect.getWidth(), rect.getHeight());
	mList->translateIntoRect(root_view_local, FALSE);

	// Make sure we didn't go off bottom of screen
	S32 x, y;
	mList->localPointToScreen(0, 0, &x, &y);

	if (y < 0)
	{
		mList->translate(0, -y);
	}

	// NB: this call will trigger the focuslost callback which will hide the list, so do it first
	// before finally showing the list

	mList->setFocus(TRUE);

	// register ourselves as a "top" control
	// effectively putting us into a special draw layer
	// and not affecting the bounding rectangle calculation
	gFocusMgr.setTopCtrl(this);

	// Show the list and push the button down
	mButton->setToggleState(TRUE);
	mList->setVisible(TRUE);
	setUseBoundingRect(TRUE);
}

void LLImageFlyoutButton::draw()
{
	LLFlyoutButton::draw();

	if( mListState )
	{
		mButton->setImageUnselected(mExpanderButtonImageSelected);
	}
	else
	{
		mButton->setImageUnselected(mExpanderButtonImage);
	}
}

//
// Globals
//

LLNavBar *gNavBar = NULL;
S32 NAV_BAR_HEIGHT = 20;

//
// Functions
//


LLNavBar::LLNavBar()
: LLPanel(), mMinSearchChars(3), mFoundRegionHandle(0), mFoundRegionURL(""), mIgnoreNextTeleport(false)
{
	setIsChrome(TRUE);
	setFocusRoot(TRUE);
}


BOOL LLNavBar::postBuild()
{
	init_navbar_actions(this);

	childSetAction("home_btn", onClickHome, this);
	childSetAction("back_btn", onClickBack, this);
	childSetAction("next_btn", onClickNext, this);

	childSetAction("teleport_btn",onClickTeleport,this);
	childSetEnabled("teleport_btn", TRUE);
	childSetCommitCallback("more_btn", onClickMore, this);
	//childSetCommitCallback("Slurl_edit", onChangeSlurlEdit, this); // dont need this any more, onItemSelected gets called instead
	childSetAction("land_btn", onClickParcelInfo, this );

	setDefaultBtn("teleport_btn");
	
	LLScrollListCtrl* history_list = getChild<LLScrollListCtrl>(LL_NAV_BAR_SCROLLLIST_NAME);
	history_list->setVisible(FALSE);
	sCullTime = gSavedSettings.getU32("HistoryDays");

	mSlurlEdit = getChild<LLNavbarComboBox>( LL_NAV_BAR_COMBOBOX_NAME );
	mSlurlEdit->setTextEntryCallback(slurlEditTextEntryCallback);
	mSlurlEdit->setContextMenuCallback(slurlContextMenuCallback);

	mFlyout = getChild<LLImageFlyoutButton>( "more_btn" );

	// disable button sounds on the navbar
	for (child_list_const_iter_t child_iter = getChildList()->begin();
		child_iter != getChildList()->end(); ++child_iter)
	{
		LLView *view = *child_iter;
		LLButton* buttonp = dynamic_cast<LLButton*>( view );
		if(buttonp)
		{
			buttonp->setSoundFlags(LLView::SILENT);
		}
	}
	is_moving = FALSE;

	layoutButtons();
	updateHistoryList();
	return TRUE;
}

LLNavBar::~LLNavBar()
{
}

// static
std::string LLNavBar::getLocalStoragePath()
{
	return gDirUtilp->getLindenUserDir() + gDirUtilp->getDirDelimiter() + LL_NAVBAR_PERSISTENT_DATA;
}

void LLNavBar::layoutButtons()
{
}


// virtual
void LLNavBar::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLPanel::reshape(width, height, called_from_parent);
	layoutButtons();
}


// Per-frame updates of visibility
void LLNavBar::refresh()
{   
	BOOL back_mode=TRUE;
	BOOL next_mode=TRUE;
	BOOL show = gSavedSettings.getBOOL("ShowNavBar");
	BOOL mouselook = gAgent.cameraMouselook();
	setVisible(show && !mouselook);

	if(LLAppViewer::instance()->quitRequested())
	{
		writeToFile();
	}
	std::string dir;
	dir = gDirUtilp->getLindenUserDir();

	if(!isFileOpen)
	{
		loadFromFile();
	}
	
	if(mSlurlEdit->showSlurl)
	{
		gNavBar->mSlurlEdit->setCursorToEnd();
		gNavBar->is_moving = FALSE;
		gNavBar->setAddressBar(gAgent.getNavSLURL());
	}
	
	if(gAgent.getRegion()){

		LLURI slurl = LLURLDispatcher::buildSLURL(gAgent.getRegion()->getName(), 10, 10, 10);
		if(gNavBar->mSlurlEdit->getSimple() == slurl.asString())
		{
			gNavBar->is_moving = FALSE;  
		}
	}
	if(gNavBar->regionBackHistory.empty())
	{
		back_mode=FALSE;
	}
	gSavedSettings.setBOOL("BackBtnState", back_mode);
	childSetEnabled("back_btn",back_mode);
	if(gNavBar->regionNextHistory.empty())
	{
		next_mode=FALSE;
	}
	gSavedSettings.setBOOL("NextBtnState", next_mode);
	childSetEnabled("next_btn",next_mode);

	updateHistoryList();

	if(mSlurlEdit->showSlurl)
	{
		gNavBar->mSlurlEdit->setCursorToEnd();
		mSlurlEdit->showSlurl = FALSE;
	}
}

void LLNavBar::onClickBack(void *userdata)
{
	gViewerWindow->alertXml("TeleportFromNavbar", LLNavBar::backButtonDialog, userdata);
}

void LLNavBar::backButtonDialog(S32 option, void* data)
{
	LLNavBar* navbar = (LLNavBar*)data;
	if (option == 0)
	{
		if(!navbar->regionBackHistory.empty())
		{
			LocationInfo current_info;
			if(navbar->getCurrentLocationInfo(current_info) != false)
			{
				navbar->regionNextHistory.push_back(current_info);
				mFlyout->selectNthItem(navbar->getCurrentLocationInfo(current_info));
			}

			LocationInfo back_info = navbar->regionBackHistory.back();
			navbar->regionBackHistory.pop_back();
			navbar->teleportCore(back_info.getSLURL(), true);
		}
	}
}

void LLNavBar::onClickNext(void *userdata)
{
gViewerWindow->alertXml("TeleportFromNavbar",
	LLNavBar::nextButtonDialog, userdata);
}

void LLNavBar::nextButtonDialog(S32 option, void* data)
{
	LLNavBar* navbar = (LLNavBar*)data;
	if (option == 0)
	{
		if(!navbar->regionNextHistory.empty())
		{
			LocationInfo current_info;
			if(navbar->getCurrentLocationInfo(current_info) != false)
			{
				navbar->regionBackHistory.push_back(current_info);
			}

			LocationInfo next_info = navbar->regionNextHistory.back();
			navbar->regionNextHistory.pop_back();
			navbar->teleportCore(next_info.getSLURL(), true);
		}
	}
}



void LLNavBar::onClickHome(void *userdata)
{		
	gViewerWindow->alertXml("TeleportFromNavbar", LLNavBar::homeButtonDialog, userdata);
}

void LLNavBar::homeButtonDialog(S32 option, void* data)
{
	if (option == 0)
	{
		gAgent.teleportHome();
	}
}

void LLNavBar::teleportCore(const std::string& address, bool from_history)
{
	if( ! from_history && address != "" )
	{
		// dont add duplicates
		if( ! mSlurlEdit->isExist( address ) )
		{
			llinfos << "LLNavBar::teleportCore(); saving new address to address bar history" << llendl;

			mSlurlEdit->add( address, ADD_TOP );
			addressToAdd="";

			// got new address
			LLNavBarPersistentData data;

			data.addBackLocation( address );
			data.trimBackLocations();
			data.printBackLocations();
			data.save();
		}
	}

	if(LLURLDispatcher::isSLURL(address))
	{
		mIgnoreNextTeleport = from_history;
		gAgent.teleportViaSLURL(address);
	}
	else if(mFoundRegionHandle != 0)
	{
		mIgnoreNextTeleport = from_history;
		U64 tmpFoundRegionHandle = mFoundRegionHandle;
		mFoundRegionHandle = 0;
		gAgent.teleportRequest(tmpFoundRegionHandle, LLVector3(128, 128, 0));
	}
	else
	{
		// If no one handles the text, treat it as a search string.
		LLFloaterDirectory::showFindAll(address);
	}
}

// called when an entry is selected from the address bar history list
// and a new slurl is set within the combobox
void LLNavBar::teleportToCurrentSLURL()
{
	std::string address = mSlurlEdit->getSimple();
	LLStringUtil::trim(address);
	if( address == "" )
	{	// ignore empty lines -- there shouldnt be any in the address bar history list anyway
		return;
	}

	if(LLURLDispatcher::isSLURL(address))
	{	// its an slurl, teleport there directly
		teleportCore(address);
		return;
	}

	// not an slurl, perform a region info lookup on it and then teleport to that region once the info returns from server

	LLWorldMap* worldMap = LLWorldMap::getInstance();
	// we could do an immediate teleport.. but instead we route to a local function
	// this allows for easy stat counting; maybe give hints to the teleport history to make it easy
	// for a user find the areas she visits the most.
	llinfos << "LLNavBar::teleportToCurrentSLURL(); url = " << address << llendl;
	worldMap->sendNamedRegionRequest(address, onHaveRegionInfoFinishTeleport, address, false);
}

//static
void LLNavBar::onHaveRegionInfoFinishTeleport(U64 region_handle,
										  const std::string& url,
										  const LLUUID& snapshot_id,
										  bool teleport)
{

	gNavBar->mFoundRegionHandle = region_handle;
	gNavBar->mFoundRegionURL = url;

	llinfos << "LLNavBar::onHaveRegionInfoFinishTeleport(); url = " << url << llendl;

	std::string addressText = gNavBar->mSlurlEdit->getSimple();
	gNavBar->teleportCore(addressText);
}

void LLNavBar::onClickTeleport(void *userdata)
{
	LLNavBar* navbar = (LLNavBar*)userdata;
	std::string addressText = navbar->mSlurlEdit->getSimple();

	LLStringUtil::trim(addressText);
	navbar->teleportCore(addressText);
}
void LLNavBar::writeToFile(std::string name, LLURI slurl, LLUUID regionId)
{
	time_t date; // Make a time_t object that'll hold the date
	time(&date); //  Set the date variable to the current date
	
	history[counter]["historyentry"]["name"] = name;
	history[counter]["historyentry"]["slurl"] = slurl;
	history[counter]["historyentry"]["regionId"] = regionId;
	history[counter]["historyentry"]["time"] = LLDate( date );
	std::string dir;
	dir = gDirUtilp->getLindenUserDir();
	dir += gDirUtilp->getDirDelimiter();
	std::string filename = dir + LL_PATH_HISTORY_FILE ;
	llofstream file;
	file.open(filename.c_str());

	if(file.is_open())
	{
		LLSDSerialize::toPrettyXML(history, file);
	}
	++counter;
	file.close();
}

void LLNavBar::writeToFile()
{
	time_t now;
	time(&now);

	LocationInfo info;
	std::string dir;
	dir = gDirUtilp->getLindenUserDir();
	dir += gDirUtilp->getDirDelimiter();
	std::string filename = dir + LL_PATH_HISTORY_FILE ;
	llofstream file;
	file.open(filename.c_str());

	if(file.is_open())
	{
		LLSDSerialize::toPrettyXML(teleportHistory, file);
	}
	file.close();

}

void LLNavBar::loadFromFile()
{

	std::string dir;
	dir = gDirUtilp->getLindenUserDir();
	dir += gDirUtilp->getDirDelimiter();
	std::string filename = dir + LL_PATH_HISTORY_FILE ;
	llifstream file;
	sCullTime = gSavedSettings.getU32("HistoryDays");
	file.open(filename.c_str());

	if(file.is_open())
	{

		U32 retval = LLSDSerialize::fromXML(history, file);
		int  count = 0;
		time_t now;
		time(&now);
		double tempCullTime = 0.0;

		LLSD temp;
		std::string buffer;
		std::vector<std::string> tokens; 


		if(retval>0)
		{
			tempCullTime = sCullTime + 1.0;
			for(;count< history.size(); count++ )
			{	
				time_t historyDate = history[count]["historyentry"]["time"].asInteger();
				double diff =  difftime(now, historyDate);//historyDate - currDate;
				double timeDiff = diff/(3600*24);
				timeDiff = ceil(timeDiff);
				if(timeDiff > tempCullTime) 
				{
					history.erase(count);					
					if(count==history.size()-1)
					{
						int temp = count+1;
						history.erase(temp);
					}
				}
				else	
				{
					teleportHistory.append(history[count]);
				}				
			}
			counter = count;
		}
		isFileOpen = TRUE;
	}
	file.close();
	if(sCullTime==0)
	{			
		history.clear();
		LLFlyoutButton* flyout = gNavBar->mFlyout;
		if(flyout)
		{
			flyout->removeall();
			
		}
	}



}

int LLNavBar::getDate(std::string dateString)
{
	std::string buffer;
	int date = -1;
	std::vector<std::string> tokens; 
	buffer.clear();
	tokens.clear();
	std::string strdate = dateString;
	std::stringstream ss(strdate); // Insert the string into a stream


	while (ss >> buffer)
		tokens.push_back(buffer);

	if(tokens.size() > 0)
	{
		std::string historyDateStr = tokens[2];
		date  = atoi(historyDateStr.c_str());
	}
	return date;
}
void LLNavBar::cullHistory()
{
	int  count = 0;
	time_t now;
	time(&now);
	LLSD temp;
	std::string buffer;
	std::vector<std::string> tokens; 
	sCullTime = gSavedSettings.getU32("HistoryDays");

		teleportHistory.clear();
		sCullTime = sCullTime+1.0;
		for(;count< history.size(); count++ )
		{	
			time_t historyDate = history[count]["historyentry"]["time"].asInteger();
			double diff =  difftime(now, historyDate);//historyDate - currDate;
			double timeDiff = diff/(3600*24);
			timeDiff = ceil(timeDiff);

			if(timeDiff > sCullTime) 
			{
				history.erase(count);
				if(count==history.size()-1)
				{
					int temp = count+1;
					history.erase(temp);
				}
			
			}
			else	
			{
				teleportHistory.append(history[count]);				
			}	
		}
		counter = count;

		
}


void LLNavBar::clearHistory()
{
	history.clear();
	LLFlyoutButton* flyout = gNavBar->mFlyout;

	flyout->removeall();
	
	gNavBar->regionBackHistory.clear();
	gNavBar->regionNextHistory.clear();
	
	teleportHistory.clear();
	LLNavigationList::setBackList(teleportHistory);
	
	LLNavigationList::hasTeleported = TRUE;
	LLNavigationList ::history.clear();
	LLNavigationList::refreshNavList();
}






// static
void LLNavBar::onClickMore(LLUICtrl* ctrl, void* user_data)
{	
	LLNavBar* navbar = (LLNavBar*)user_data;
	LLFlyoutButton* flyout = (LLFlyoutButton*)ctrl;

	
		LLSD selected_value = flyout->getSelectedValue();

		// Adjust the next and back histories to sync up with
		// the new teleport.
		std::vector<LocationInfo> allHistory;
		allHistory.insert(allHistory.begin(), navbar->regionBackHistory.rbegin(), navbar->regionBackHistory.rend());

		LocationInfo current_info;
		if(navbar->getCurrentLocationInfo(current_info))
			allHistory.insert(allHistory.begin(), current_info);

		allHistory.insert(allHistory.begin(), navbar->regionNextHistory.begin(), navbar->regionNextHistory.end());

		navbar->regionBackHistory.clear();
		navbar->regionNextHistory.clear();

		bool selected_location_found = false;
		for(std::vector<LocationInfo>::iterator it = allHistory.begin(); it != allHistory.end(); ++it)
		{

			// If we have found the selected iterator, then all iterators after
			// go into the back list.
			if(selected_location_found == true)
			{
				navbar->regionBackHistory.push_back(*it);
			}
			else
			{
				// If we haven't found the current history yet, then these will
				// go into the next history.
				// If we find the location, it doesn't go back into the list because
				// it'll be our current location.
				LocationInfo info = *it;
				if(info.getSLURL() == selected_value.asString())
				{
					selected_location_found = true;
					continue;
				}

				navbar->regionNextHistory.push_back(*it);
			}
		}
		LocationInfo sameInfo;
		navbar->getCurrentLocationInfo(sameInfo);
		if(sameInfo.getSLURL() == selected_value.asString())
		{
			navbar->teleportCore(selected_value.asString(), false);
		}
		else
		{
		navbar->teleportCore(selected_value.asString(), true);
		}
	

}

bool LLNavBar::getCurrentLocationInfo(LocationInfo& info) const
{
	if(gAgent.getAvatarObject())
	{
		LLViewerRegion *region = gAgent.getRegion();
		if(region != NULL)
		{
			// Save the location history information in memory
			info.setRegionName(region->getName());
			info.setRegionPosition(LLVector3d(gAgent.getPositionAgent()));
			info.setSLURL(gAgent.getSLURL());
			return true;
		}
	}
	return false;
}

void LLNavBar::onAgentTeleporting(LLAgent::ETeleportState teleport_state, const std::string& slurl)
{
	//const std::string& temp="";
	//gNavBar->mSlurlEdit->setTextEntry(temp);
	//gNavBar->childSetEnabled("teleport_btn", FALSE);

	if(teleport_state == LLAgent::TELEPORT_MOVING)
	{
		// If we're ignoring the next teleport, don't save the location.
		// This can only be set in our next and back functions.
		if(!gNavBar->mIgnoreNextTeleport)
		{
			gNavBar->onAgentTeleportMoving();

			// Clear the forward history because we've broken the chain.
			gNavBar->regionNextHistory.clear();

			LLNavigationList::addToHistory();
			reload_recent_places_menu();
		}
		gNavBar->mIgnoreNextTeleport = false;
		gNavBar->updateSLURL(slurl);

	}
	else if(teleport_state == LLAgent::TELEPORT_ARRIVING)
	{
		gNavBar->updateSLURL(slurl);
		gNavBar->updateHistoryList();
		gNavBar->childSetEnabled("teleport_btn", TRUE);
	}

	gNavBar->mSlurlEdit->setLabel(gAgent.getSLURL());
	
	// close open add-landmark dialogs -- leave receive-landmark dialogs
	for( LLLandmarkFloaterFactory<LLFloaterAddLandmark>::iterator iter = LLLandmarkFloaterFactory<LLFloaterAddLandmark>::getInstance()->getIterStart();
		iter != LLLandmarkFloaterFactory<LLFloaterAddLandmark>::getInstance()->getIterEnd(); iter ++ )
	{
		(*iter)->mClosingFromNavbar = true;
		(*iter)->close();
	}
}


// static
void LLNavBar::updateSLURL(const std::string& slurl)
{
	gNavBar->mSlurlEdit->setLabel(slurl);
	//gNavBar->childSetEnabled("teleport_btn", FALSE);
}

// static
void LLNavBar::onAgentTeleportMoving()
{
	LocationInfo info;
	if(getCurrentLocationInfo(info))
	{
		regionBackHistory.push_back(info);
	}
}

// static
void LLNavBar::updateHistoryList()
{
	// Rewrite the history list that drops down from the "more" flyout image button
	LLFlyoutButton* flyout = gNavBar->mFlyout;

	flyout->removeall();

	// Add the next history first
	for(int i = (int)gNavBar->regionNextHistory.size() - 1; i >= 0 ; --i)
	{
		LocationInfo info = gNavBar->regionNextHistory[i];
		flyout->add(info.getRegionName(), LLSD(info.getSLURL()));
	}

	// Add the current history and mark it as selected
	LocationInfo current_info;
	if( gNavBar->getCurrentLocationInfo(current_info) )
	{
		flyout->add(current_info.getRegionName(), LLSD(current_info.getSLURL()));
	}

	// Add the back history
	for(int i = (int)gNavBar->regionBackHistory.size() - 1; i >= 0; --i)
	{
		LocationInfo info = gNavBar->regionBackHistory[i];
		flyout->add(info.getRegionName(), LLSD(info.getSLURL()));
	}

	// Add a final separator and the location history entry
	

	// Select the current item
	flyout->selectNthItem(gNavBar->regionNextHistory.size());
	mFlyout->nthItem = gNavBar->regionNextHistory.size();
}

void LLNavBar::setBackList(LLSD backlist) {
	history = backlist;
}

void LLNavBar::getBackList(LLSD& backlist) {
	backlist = teleportHistory;
}

void LLNavBar::setFoundRegionInfo(U64 region_handle, const std::string& region_url)
{
	mFoundRegionHandle = region_handle;
	mFoundRegionURL = region_url;
}

//static
void LLNavBar::onChangeSlurlEdit(LLUICtrl* ctrl, void* user_data)
{
	KEY key = gKeyboard->currentKey();
	if (key == KEY_RETURN)
	{
		LLNavBar* navbar = (LLNavBar*)user_data;
		addressToAdd = navbar->mSlurlEdit->getSimple();
		LLStringUtil::trim(addressToAdd);

		// if the address typed into the address bar is the same as we already have in the address bar history
		// then the address bar history functionality will be activated to teleport there
		// so we suppress the enter-key teleportation

		if( gNavBar->mSlurlEdit->getSelectedItemLabel() != addressToAdd )
		{
			onClickTeleport(user_data);
		}
	}
}

// TODO: currently only used to enable teleport button on text entry
// used to prefetch region info while user was typing in a region name
// static
void LLNavBar::slurlEditTextEntryCallback(LLLineEditor *caller, void *user_data)
{
	std::string text = caller->getText();
	gNavBar->childSetEnabled("teleport_btn", TRUE);

	if( 0 && text.length() >= gNavBar->mMinSearchChars)
	{
		// Pass the text to the region name validator.
		LLWorldMap* worldMap = LLWorldMap::getInstance();
		worldMap->sendNamedRegionRequest(text, namedRegionRequestCallback, text, false);
	}
}

// static
LLMenuGL* LLNavBar::slurlContextMenuCallback(LLUICtrl *caller, void *user_data)
{
	LLNavBar* navbarp = (LLNavBar*)user_data;
	std::string addressBarText = navbarp->mSlurlEdit->getSimple();
	LLStringUtil::trim(addressBarText);
	
	// make the popup menu available
	LLMenuGL* menu1 = LLUICtrlFactory::getInstance()->buildMenu("menu_navbar_addressbar.xml", navbarp);
	
	if (!menu1)
		menu1 = new LLMenuGL("");

	menu1->setVisible(FALSE);
	LLHandle<LLView> mPopupMenuHandle = menu1->getHandle();
	LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandle.get();
	const LLView::child_list_t *list = menu->getChildList();
	LLView::child_list_t::const_iterator menu_itor;
	for (menu_itor = list->begin(); menu_itor != list->end(); ++menu_itor)
	{
		(*menu_itor)->setVisible(TRUE);
		if(addressBarText=="" && (*menu_itor)->getName()!="Paste")
		{
			(*menu_itor)->setEnabled(FALSE);
		}
		else
		{
			(*menu_itor)->setEnabled(TRUE);
		}
	}

	menu->arrange();
	menu->updateParent(LLMenuGL::sMenuContainer);
	menu->setFollowsBottom();
	navbarp->mSlurlEdit->setKeyboardFocus();
	navbarp->childSetEnabled("teleport_btn", TRUE);
	return menu;
}
// static
void LLNavBar::namedRegionRequestCallback(U64 region_handle,
										  const std::string& url,
										  const LLUUID& snapshot_id,
										  bool teleport)
{
	gNavBar->setFoundRegionInfo(region_handle, url);
}

void LLNavBar::startToggle()
{	
	gNavBar->setVisible(TRUE);	
	gSavedSettings.setBOOL("NavVisible", TRUE);
	gNavBar->mSlurlEdit->setFocus(TRUE);
	gNavBar->mSlurlEdit->setKeyboardFocus();
}

// static
void LLNavBar::stopToggle()
{
	gNavBar->setVisible(FALSE);
	gSavedSettings.setBOOL("NavVisible", FALSE);
}

// this simply clears the current address bar edit field
void LLNavBar::clearAddressBar()
{
	mSlurlEdit->clear();
}
// this clears the address bar history dropdown list
void LLNavBar::clearAddressBarHistory()
{
	mSlurlEdit->clearHistory();
}

void LLNavBar::setAddressBar(std::string str)
{
	mSlurlEdit->setLabel(str);
}

std::string LLNavBar::getAddressBar() {
	return mSlurlEdit->getSimple();
}

static void onClickParcelInfo(void* data)
{
	if (LLViewerParcelMgr::getInstance()->selectionEmpty())
		LLViewerParcelMgr::getInstance()->selectParcelAt(gAgent.getPositionGlobal());

	LLFloaterLand::showInstance();
}

class LLDoToSelectedNavBar : public navbar_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string action = userdata.asString();

		if ("copy" == action)
		{	
			mPtr->mSlurlEdit->copy();
		}
		else if("delete" == action)
		{
			std::string textToDelete = "";
			mPtr->setAddressBar(textToDelete);
		}
		else if("cut" == action)
		{
			std::string text = "";
			LLView::getWindow()->copyTextToClipboard(utf8str_to_wstring(mPtr->getAddressBar()));
			mPtr->setAddressBar(text);
		}
		else if("paste" == action)
		{
			LLWString str;
			LLView::getWindow()->pasteTextFromClipboard(str);
			std::string str2 = wstring_to_utf8str(str);
			mPtr->setAddressBar(str2);
			mPtr->mSlurlEdit->setCursorToEnd();
		}

		return true;
	}
};

void init_navbar_actions(LLNavBar *navbar)
{
	(new LLDoToSelectedNavBar())->registerListener(navbar, "Navbar.DoToSelected");
}
