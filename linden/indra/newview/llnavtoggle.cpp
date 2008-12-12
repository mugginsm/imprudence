/** 
* @file llnavtoggle.cpp
* @author Vectorform
* @brief Toggle Button.
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

#include "llnavtoggle.h"


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
#include "llviewerwindow.h"
#include "lltoolgrab.h"
#include "llcombobox.h"
#include "llfloaterchat.h"
#include "llfloatermute.h"
#include "llimpanel.h"
#include "llscrolllistctrl.h"

//
// Globals
//

LLNavToggle *gNavToggle = NULL;


//
// Functions
//

LLNavToggle::LLNavToggle()
:	LLPanel()
{
	setIsChrome(TRUE);
	setFocusRoot(TRUE);
}


BOOL LLNavToggle::postBuild()
{

	childSetAction("nav_btn", onClickNav, this);
	childSetControlName("nav_btn", "NavVisible");


	for (child_list_const_iter_t child_iter = getChildList()->begin();
		child_iter != getChildList()->end(); ++child_iter)
	{
		LLView *view = *child_iter;
		LLButton* buttonp = dynamic_cast<LLButton*>(view);
		if(buttonp)
		{
			buttonp->setSoundFlags(LLView::SILENT);
		}
	}

	layoutButtons();

	return TRUE;
}

LLNavToggle::~LLNavToggle()
{

}


// static
void LLNavToggle::toggle(void*)
{
	BOOL show = gSavedSettings.getBOOL("ShowNavBar");                      
	gSavedSettings.setBOOL("ShowNavBar", !show);                           
	gNavToggle->setVisible(!show);
}


// static
BOOL LLNavToggle::visible(void*)
{
	return gNavToggle->getVisible();
}

void LLNavToggle::layoutButtons()
{
}


// virtual
void LLNavToggle::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLPanel::reshape(width, height, called_from_parent);

	layoutButtons();
}


// Per-frame updates of visibility
void LLNavToggle::refresh()
{
	BOOL show = gSavedSettings.getBOOL("ShowNavBar");
	BOOL mouselook = gAgent.cameraMouselook();
	setVisible(show && !mouselook);
	
}

// static
void LLNavToggle::onClickNav(void* user_data)
{
	handle_nav(NULL);
}






























