/** 
* @file llnavigationlist.h
* @author Vectorform
* @brief Complete Teleport History.
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
#include "llscrolllistctrl.h"
#include "llnavbar.h"
#include "locationinfo.h"

class LLNavigationList: public LLFloater
{
public:
	LLNavigationList();
	~LLNavigationList();
	BOOL postBuild();
	static BOOL refreshNavList();
	static void setBackList(LLSD backList);
	static void setRecentPlaces(LLSD recent);
	static void getRecentPlaces(LLSD& recent);
	static void setForwardList(LLSD forwardList);
	static void addToHistory();
	static void clearRecentPlaces();
	static void show();
	static BOOL hasTeleported;
	static LLSD history;
private:
	BOOL buildList(BOOL asc);
	static void onLocationSelect(void *data);
	int getDate(std::string dateString);
	std::vector<LocationInfo> mToday_vector;
	std::vector<LocationInfo> mYesterday_vector;
	std::vector<LocationInfo> mTwoDaysAgo_vector;
	std::vector<LocationInfo> mThreeDaysAgo_vector;
	std::vector<LocationInfo> mFourDaysAgo_vector;
	std::vector<LocationInfo> mFiveDaysAgo_vector;
	std::vector<LocationInfo> mLongDaysAgo_vector;
	static void onSortChanged(void* userdata);
	void fillDayVectors();

	LLScrollListCtrl* list;
	//static LLSD teleportHistory;
	static LLSD recentPlaces;
	static LLNavigationList* sInstance;

};
