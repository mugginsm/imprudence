/** 
* @file llpanelhistory.h
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
#ifndef LL_LLPANELHISTORY_H
#define LL_LLPANELHISTORY_H
#include "lluictrl.h"
#include "llpanel.h"
class LLSpinCtrl;

class LLPanelHistory : public LLPanel
{
public:
	LLPanelHistory();
	virtual BOOL postBuild();
	~LLPanelHistory(void);

	void apply();
	void cancel();

private:

	static void onClickClearNowReset( void* data );
	static void onClearNowDialogReset( S32 option, void* data );
	static void onClearDialog(S32 option, void* data);
    static void onClearNowDialog(S32 option, void* data);

	static void onClickExport_btn(void* data);
	static void onCommitDays(LLUICtrl* ctrl, void *userdata);
	static void onCommitRecent(LLUICtrl* ctrl, void *userdata);
	static void onCommitAddressHistoryAge(LLUICtrl* ctrl, void *userdata);
	static void onClickApply(void* data);
	static void onClickClearNowTeleportHistory(void* data);
	static void onClickClearNowrecent(void* data);

protected:
	U32 mpreviousdays;
	U32 mpreviouslocation;
	U32 mdays;
	U32 mlocation;
};


#endif