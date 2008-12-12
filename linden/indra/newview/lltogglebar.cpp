/** 
* @file lltogglebar.cpp
* @brief LLToggleBar class implementation
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

#include "lltogglebar.h"

#include "audioengine.h"
#include "llrender.h"
#include "llagent.h"
#include "llbutton.h"
#include "llchatbar.h"
#include "llnavbar.h"
#include "llfocusmgr.h"
#include "llimview.h"
#include "llmediaremotectrl.h"
#include "llpanelaudiovolume.h"
#include "llparcel.h"
#include "lltextbox.h"
#include "llui.h"
#include "llviewercontrol.h"
#include "llviewerimagelist.h"
#include "llviewermedia.h"
#include "llviewermenu.h"	// handle_reset_view()
#include "llviewermedia.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llvoiceclient.h"
#include "llvoavatar.h"
#include "llvoiceremotectrl.h"
#include "llwebbrowserctrl.h"
#include "llselectmgr.h"

//
// Globals
//

LLToggleBar *gToggleBar = NULL;

//
// Functions
//

void* LLToggleBar::createNavBar(void* userdata)
{
	gNavBar = new LLNavBar();
	return gNavBar;
}


LLToggleBar::LLToggleBar()
:	LLPanel()
{
	setMouseOpaque(FALSE);
	setIsChrome(TRUE);
	mBuilt = false;

	LLCallbackMap::map_t factory_map1;
	factory_map1["navbar"] = LLCallbackMap(LLToggleBar::createNavBar, this);

	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_togglebar.xml", &factory_map1);
}

BOOL LLToggleBar::postBuild()
{
	childSetVisible("navbar", gSavedSettings.getBOOL("NavVisible"));

	llinfos << "LLToggleBar::postBuild();" << llendl;
	
	setFocusRoot(TRUE);

	mBuilt = true;
	layoutButtons();
	return TRUE;
}

LLToggleBar::~LLToggleBar()
{
}

// virtual
void LLToggleBar::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLView::reshape(width, height, called_from_parent);

	if (mBuilt) 
	{
		layoutButtons();
	}

}

void LLToggleBar::layoutButtons()
{
	LLView* state_buttons_panel = getChildView("state_buttons");

	if (state_buttons_panel->getVisible())
	{
		LLViewQuery query;
		LLWidgetTypeFilter<LLButton> widget_filter;
		query.addPreFilter(LLEnabledFilter::getInstance());
		query.addPreFilter(&widget_filter);

		child_list_t button_list = query(state_buttons_panel);

		const S32 MAX_BAR_WIDTH = 600;
		S32 bar_width = llclamp(state_buttons_panel->getRect().getWidth(), 0, MAX_BAR_WIDTH);

		// calculate button widths
		const S32 MAX_BUTTON_WIDTH = 150;
		S32 segment_width = llclamp(lltrunc((F32)(bar_width) / (F32)button_list.size()), 0, MAX_BUTTON_WIDTH);
		S32 btn_width = segment_width - gSavedSettings.getS32("StatusBarPad");

		// Evenly space all buttons, starting from left
		S32 left = 0;
		S32 bottom = 1;

		for (child_list_reverse_iter_t child_iter = button_list.rbegin();
			child_iter != button_list.rend(); ++child_iter)
		{
			LLView *view = *child_iter;
			LLRect r = view->getRect();
			r.setOriginAndSize(left, bottom, btn_width, r.getHeight());
			view->setRect(r);
			left += segment_width;
		}
	}
}

// Per-frame updates of visibility
void LLToggleBar::refresh()
{
	BOOL buttons_changed = FALSE;
	// always let user toggle into and out of navbar
	childSetVisible("navbar", gSavedSettings.getBOOL("NavVisible"));

	if (buttons_changed)
	{
		layoutButtons();
	}

		// turn off the whole bar in mouselook
	if (gAgent.cameraMouselook())
	{
		childSetVisible("navbar", FALSE);
		
	}
}