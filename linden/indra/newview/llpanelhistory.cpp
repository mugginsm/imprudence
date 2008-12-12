/** 
* @file llpanelhistory.cpp
* @author Vectorform
* @brief panel for setting the preferences of navigationbar
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
#include "llpanelhistory.h"
#include "lluictrlfactory.h"
#include "llfloaterexportlandmark.h"
#include "lllineeditor.h"
#include "llviewercontrol.h"
#include "llcheckboxctrl.h"
#include "llnavbar.h"
#include "llspinctrl.h"
#include "llnavigationlist.h"
#include "llviewerwindow.h"

LLPanelHistory::LLPanelHistory():LLPanel("panel_preferences_history") 
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_history.xml");
	
	childSetCommitCallback("days", onCommitDays, this);
	childSetCommitCallback("locations_name", onCommitRecent, this);
	childSetCommitCallback("address_bar_history_age", onCommitAddressHistoryAge, this);

	childSetAction("Apply", onClickApply, this);
	childSetAction("Clear_Now", onClickClearNowTeleportHistory, this);
	childSetAction("Clear_Now_recent", onClickClearNowrecent, this);

	childSetAction("reset_address_bar_expiry", onClickClearNowReset, this);

	LLSpinCtrl*  spin_ctrl = getChild<LLSpinCtrl>("days");
	LLSpinCtrl*  spin_ctrl2 = getChild<LLSpinCtrl>("locations_name");
	
	std::ostringstream buffer;
	buffer<<gSavedSettings.getU32("HistoryDays");
	spin_ctrl->setValue(buffer.str());

	int history_expiry_age = gSavedSettings.getU32( "AddressBarMaxHistoryAge" );
	getChild<LLSpinCtrl>("address_bar_history_age")->setValue( history_expiry_age );

	llinfos << "history_expiry_age = " << history_expiry_age << llendl;

	std::ostringstream buffer2;
	buffer2<<gSavedSettings.getU32("RecentLocations");
	spin_ctrl2->setValue(buffer2.str());
}

BOOL LLPanelHistory::postBuild()
{
	childSetAction("Export_btn", onClickExport_btn, this);
	
	mpreviousdays = gSavedSettings.getU32("HistoryDays");   
    mpreviouslocation = gSavedSettings.getU32("RecentLocations");   

	return TRUE;

}
LLPanelHistory::~LLPanelHistory()
{
}

void LLPanelHistory::apply()
{
	LLSpinCtrl*  spin_ctrl = getChild<LLSpinCtrl>("days");
	LLSD val = spin_ctrl->getValue();
	U32 days = val.asInteger();
	gSavedSettings.setU32("HistoryDays", days );

	LLSpinCtrl* spin_ctrl3 = getChild<LLSpinCtrl>("address_bar_history_age");
	U32 address_history_age = (U32) spin_ctrl3->getValue().asInteger();
	gSavedSettings.setU32("AddressBarMaxHistoryAge", address_history_age );

	llinfos << "LLPanelHistory::apply(); address_history_age = " << address_history_age << llendl;

	LLSpinCtrl*  spin_ctrl2 = getChild<LLSpinCtrl>("locations_name");
	LLSD value1 = spin_ctrl2->getValue();
	U32 location = value1.asInteger();
	gSavedSettings.setU32("RecentLocations", location );

	gNavBar->cullHistory();
}

void LLPanelHistory::cancel()
{
}

void LLPanelHistory::onClickClearNowReset( void* data )
{
	LLPanelHistory* self = static_cast<LLPanelHistory*>( data );
	gViewerWindow->alertXml( "HistoryPanelResetAddressBar", onClearNowDialogReset, self );
}


void LLPanelHistory::onClearNowDialogReset( S32 option, void* data )
{
	switch( option )
	{
		case 0: // "OK"
		{
			gNavBar->clearAddressBarHistory();
		} break;
		
		case 1://"Cancel"
			break;
		
		default :
			break;

	}
}


void LLPanelHistory::onClickExport_btn(void* data)
{
	LLFloaterExportLandmark::showInstance(1);

}

void LLPanelHistory::onCommitDays(LLUICtrl* ctrl, void* userdata)
{
	LLSD val = ctrl->getValue();
	U32 days = val.asInteger();
	gSavedSettings.setU32("HistoryDays", days );
}

void LLPanelHistory::onCommitRecent(LLUICtrl* ctrl, void* userdata)
{
	LLSD val2 = ctrl->getValue();
	U32 location  = val2.asInteger();
	gSavedSettings.setU32("RecentLocations", location );
}
void LLPanelHistory::onCommitAddressHistoryAge(LLUICtrl* ctrl, void *userdata)
{
	LLSD val2 = ctrl->getValue();
	U32 address_history_age = (U32) val2.asInteger();
	gSavedSettings.setU32("AddressBarMaxHistoryAge", address_history_age );
	llinfos << "onCommitAddressHistoryAge(); address_history_age = " << address_history_age << llendl;
	gNavBar->cullHistory();
}

void LLPanelHistory::onClickApply(void* data)
{
	LLPanelHistory* self = (LLPanelHistory*)data;

	LLSpinCtrl*  spin_ctrl = self->getChild<LLSpinCtrl>("days");
	LLSD val = spin_ctrl->getValue();
	U32 days = val.asInteger();
	gSavedSettings.setU32("HistoryDays", days );

	LLSpinCtrl* spin_ctrl3 = self->getChild<LLSpinCtrl>("address_bar_history_age");
	gSavedSettings.setU32("AddressBarMaxHistoryAge", spin_ctrl3->getValue().asInteger() );

	llinfos << "onCommitAddressHistoryAge(); spin_ctrl3->getValue().asInteger() = " << spin_ctrl3->getValue().asInteger() << llendl;

	LLSpinCtrl*  spin_ctrl2 = self->getChild<LLSpinCtrl>("locations_name");
	LLSD value1 = spin_ctrl2->getValue();
	U32 location = value1.asInteger();
	gSavedSettings.setU32("RecentLocations", location );
}

void LLPanelHistory::onClickClearNowTeleportHistory(void* data)
{
    LLPanelHistory* self = (LLPanelHistory*)data;
	
	gViewerWindow->alertXml("panelhistory",onClearDialog, self);
	
	}
void LLPanelHistory::onClearDialog(S32 option, void* data)
{
 
	switch( option )
	{
		case 0:  // "ok"
		{
			gNavBar->clearHistory();
			
		} break;

		case 1:  // "cancel"
		break;

		default:
		break;
	}	
}

void LLPanelHistory::onClickClearNowrecent(void* data)
{   
	
    LLPanelHistory* self = (LLPanelHistory*)data;
	
	gViewerWindow->alertXml("panelhistoryrecent",onClearNowDialog, self);
	
	}

void LLPanelHistory::onClearNowDialog(S32 option, void* data)
{ 
	switch( option )
	{
		case 0:  // "ok"
		{
			LLNavigationList::clearRecentPlaces();
		} break;

		case 1:  // "cancel"
		break;

		default:
		break;
	}	
}


