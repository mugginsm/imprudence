/** 
* @file llnavigationlist.cpp
* @author Vectorform
* @brief Implementation of Complete Teleport History.
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

#include "llnavigationlist.h"

#include "llagent.h"
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
#include "llnavbar.h"
#include "llviewerregion.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include <ctime>

const std::string LL_NAVIGATION_LIST_SCROLLLIST_NAME("location_list");

LLNavigationList* LLNavigationList::sInstance = NULL;
BOOL LLNavigationList::hasTeleported = FALSE;

LLSD LLNavigationList::history;

LLSD LLNavigationList::recentPlaces;
LLNavigationList::LLNavigationList(): LLFloater("floater_navigation_list", "FloaterNavListRect","NULL")
,list(NULL)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_navigation_list.xml");	

}
BOOL LLNavigationList::postBuild()
{
	list = getChild<LLScrollListCtrl>(LL_NAVIGATION_LIST_SCROLLLIST_NAME);
	childSetDoubleClickCallback(LL_NAVIGATION_LIST_SCROLLLIST_NAME, onLocationSelect, this);
	list->setSortChangedCallback(onSortChanged);

	buildList(true);
	return TRUE;
}

LLNavigationList::~LLNavigationList()
{
	sInstance = NULL;
}

void LLNavigationList::setBackList(LLSD backList)
{
	history = backList;

}

void LLNavigationList::setRecentPlaces(LLSD recent)
{
	recentPlaces = recent;
}

void LLNavigationList::getRecentPlaces(LLSD& recent)
{
	recent = recentPlaces;
}

void LLNavigationList::clearRecentPlaces()
{
	recentPlaces.clear();
	//init_landmark_menu();
	reload_recent_places_menu();
}
void LLNavigationList::addToHistory()
{
	LLViewerRegion *region = gAgent.getRegion();
	std::string name = region->getName();
	LLVector3d agentPos = gAgent.getPositionGlobal();
	LLSD recent;
	S32 agent_x = llround( (F32)fmod( agentPos.mdV[VX], (F64)REGION_WIDTH_METERS ) );
	S32 agent_y = llround( (F32)fmod( agentPos.mdV[VY], (F64)REGION_WIDTH_METERS ) );
	S32 agent_z = llround( (F32)agentPos.mdV[VZ] );
	LLURI slurl = LLURLDispatcher::buildSLURL(region->getName(), agent_x, agent_y, agent_z);
	LLNavBar::getBackList(history);
	LLNavigationList::getRecentPlaces(recent);	
	time_t date; // Make a time_t object that'll hold the date
	time(&date); //  Set the date variable to the current date
	U32 size = history.size();
	U32 counter = size;
	history[counter]["historyentry"]["name"] = name;
	history[counter]["historyentry"]["slurl"] = slurl;
	history[counter]["historyentry"]["time"] = (int) date;

	LLNavBar::teleportHistory[counter]["historyentry"]["name"] = name;
	LLNavBar::teleportHistory[counter]["historyentry"]["slurl"] = slurl;
	LLNavBar::teleportHistory[counter]["historyentry"]["time"] = (int) date;
	
	size = recent.size();
	counter = size;
	U32 MAX_RECENT_PLACES=gSavedSettings.getU32("RecentLocations");

	if(MAX_RECENT_PLACES>0)
	{
		if(size<MAX_RECENT_PLACES)
		{
			recent[counter]["historyentry"]["name"] = name;
			recent[counter]["historyentry"]["slurl"] = slurl;
			recent[counter]["historyentry"]["time"] = (int) date;
			LLNavigationList::setRecentPlaces(recent);
		}
		else if(size==MAX_RECENT_PLACES)
		{
			LLSD temp;
			for(U32 i=counter-1;i>0;i--)
			{
				temp[i-1]["historyentry"]["name"]=recent[i]["historyentry"]["name"];
				temp[i-1]["historyentry"]["slurl"]=recent[i]["historyentry"]["slurl"];
				temp[i-1]["historyentry"]["time"]=recent[i]["historyentry"]["time"];
			}
			temp[counter-1]["historyentry"]["name"] = name;
			temp[counter-1]["historyentry"]["slurl"] = slurl;
			temp[counter-1]["historyentry"]["time"] = (int) date;

			LLNavigationList::setRecentPlaces(temp);		
		}
		else
		{
			LLSD temp;
			U32 j=MAX_RECENT_PLACES-1;
			for(U32 i=counter-1;j>0;i--)
			{
				temp[j-1]["historyentry"]["name"]=recent[i]["historyentry"]["name"];
				temp[j-1]["historyentry"]["slurl"]=recent[i]["historyentry"]["slurl"];
				temp[j-1]["historyentry"]["time"]=recent[i]["historyentry"]["time"];
				--j;
			}
			temp[MAX_RECENT_PLACES-1]["historyentry"]["name"] = name;
			temp[MAX_RECENT_PLACES-1]["historyentry"]["slurl"] = slurl;
			temp[MAX_RECENT_PLACES-1]["historyentry"]["time"] = (int) date;

			LLNavigationList::setRecentPlaces(temp);
		}

	}
	else 
	{		
		recentPlaces.clear();
	}	
	LLNavBar::setBackList(history);
	hasTeleported = TRUE;
	refreshNavList();

	if( sInstance && sInstance->list)
	{
		LLSD element;
		element["id"] = slurl;
		element["columns"][0]["column"] = "Name";
		element["columns"][0]["value"] =  name;
		element["columns"][0]["font"] = "SANSSERIF";
		element["columns"][1]["column"] = "slurl";
		element["columns"][1]["value"] = slurl;
		element["columns"][1]["font"] = "SANSSERIF";
		element["columns"][2]["column"] = "Day";
		element["columns"][2]["font"] = "SANSSERIF";
		element["columns"][2]["value"] ="Today";
		sInstance->list->addElement(element, ADD_TOP);
	}		
}
void LLNavigationList::onLocationSelect(void* data)
{
	LLNavigationList* self = (LLNavigationList*)data;
	LLCtrlSelectionInterface *list =self->childGetListInterface(LL_NAVIGATION_LIST_SCROLLLIST_NAME);
	LLSD val=list->getSelectedValue();
	std::string slurl = val.asString();
	gAgent.teleportViaSLURL(slurl);
}

int LLNavigationList::getDate(std::string dateString)
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

void LLNavigationList::fillDayVectors()
{
	int size = history.size();
	time_t now;
	time(&now); //  Set the date variable to the current date
	mToday_vector.clear();
	mYesterday_vector.clear();
	mTwoDaysAgo_vector.clear();
	mThreeDaysAgo_vector.clear();
	mFourDaysAgo_vector.clear();
	mFiveDaysAgo_vector.clear();
	mLongDaysAgo_vector.clear();
	for(int i = size ; i >= 0; i--)
	{
		LocationInfo info;
		time_t date = history[i]["historyentry"]["time"].asInteger();

		if(date != -1)
		{
			double diff = difftime(now, date)  ;
			double timeDiff =(diff/(3600*24));
			timeDiff=ceil(timeDiff);
			if(timeDiff == 1)
			{
				if(history[i]["historyentry"]["slurl"].asString() != "")
				{
					info.setSLURL(history[i]["historyentry"]["slurl"].asString());
					info.setRegionName(history[i]["historyentry"]["name"].asString());
					mToday_vector.push_back(info);
				}
			}
			else if(timeDiff == 2)
			{
				if(history[i]["historyentry"]["slurl"].asString() != "")
				{
					info.setSLURL(history[i]["historyentry"]["slurl"].asString());
					info.setRegionName(history[i]["historyentry"]["name"].asString());
					mYesterday_vector.push_back(info);
				}
			}
			else if(timeDiff == 3)
			{
				info.setSLURL(history[i]["historyentry"]["slurl"].asString());
				info.setRegionName(history[i]["historyentry"]["name"].asString());
				mTwoDaysAgo_vector.push_back(info);
			}
			else if(timeDiff == 4)
			{
				info.setSLURL(history[i]["historyentry"]["slurl"].asString());
				info.setRegionName(history[i]["historyentry"]["name"].asString());
				mThreeDaysAgo_vector.push_back(info);
			}
			else if(timeDiff == 5)
			{
				info.setSLURL(history[i]["historyentry"]["slurl"].asString());
				info.setRegionName(history[i]["historyentry"]["name"].asString());
				mFourDaysAgo_vector.push_back(info);
			}
			else if(timeDiff == 6)
			{
				info.setSLURL(history[i]["historyentry"]["slurl"].asString());
				info.setRegionName(history[i]["historyentry"]["name"].asString());
				mFiveDaysAgo_vector.push_back(info);
			}
			else if(timeDiff > 6 && timeDiff < 365)
			{
				info.setSLURL(history[i]["historyentry"]["slurl"].asString());
				info.setRegionName(history[i]["historyentry"]["name"].asString());
				mLongDaysAgo_vector.push_back(info);
			}

		}

	}
}

BOOL LLNavigationList::buildList(BOOL asc)
{
	fillDayVectors();
	std::vector<LLSD> vector;
	
	if(list)
	{
		list->clearRows();
		LLSD element;
		
		for(unsigned int i = 0; i < mToday_vector.size()  ; i++) 
		{
			LocationInfo info;
			std::string name;
			std::string slurl;
			info = mToday_vector[i];
			slurl = info.getSLURL();
			info.getRegionName(name);
			element["id"] =slurl;
			element["columns"][0]["column"] = "Name";
			element["columns"][0]["value"] = name;
			element["columns"][0]["font"] = "SANSSERIF";
			element["columns"][0]["font-style"] = "NORMAL";
			element["columns"][1]["column"] = "slurl";
			element["columns"][1]["value"] = slurl;
			element["columns"][1]["font"] = "SANSSERIF";
			element["columns"][2]["column"] = "Day";
			element["columns"][2]["font"] = "SANSSERIF";
			element["columns"][2]["value"] ="Today";
			//list->addElement(element);
			vector.push_back(element);
		}
		if(mYesterday_vector.size() > 0)
		{

			for(unsigned int i = 0 ; i < mYesterday_vector.size() ; i++) 
			{
				LocationInfo info;
				std::string name;
				std::string slurl;
				info = mYesterday_vector.at(i);

				slurl = info.getSLURL();
				info.getRegionName(name);
				if(name != "")
				{
					element["id"] =slurl;
					element["columns"][0]["column"] = "Name";
					element["columns"][0]["value"] = name;
					element["columns"][0]["font"] = "SANSSERIF";
					element["columns"][0]["font-style"] = "NORMAL";
					element["columns"][1]["column"] = "slurl";
					element["columns"][1]["value"] = slurl;
					element["columns"][1]["font"] = "SANSSERIF";
					element["columns"][2]["column"] = "Day";
					element["columns"][2]["font"] = "SANSSERIF";
					element["columns"][2]["value"] ="Yesterday";

				//	list->addElement(element);
					vector.push_back(element);
				}
			}
		}

		if(mTwoDaysAgo_vector.size() > 0)
		{

			for(unsigned int i = 0 ; i < mTwoDaysAgo_vector.size() ; i++) 
			{
				LocationInfo info;
				std::string name;
				std::string slurl;
				info =  mTwoDaysAgo_vector.at(i);

				slurl = info.getSLURL();
				info.getRegionName(name);
				if(name != "")
				{
					element["id"] =slurl;
					element["columns"][0]["column"] = "Name";
					element["columns"][0]["value"] = name;
					element["columns"][0]["font"] = "SANSSERIF";
					element["columns"][0]["font-style"] = "NORMAL";
					element["columns"][1]["column"] = "slurl";
					element["columns"][1]["value"] = slurl;
					element["columns"][1]["font"] = "SANSSERIF";
					element["columns"][2]["column"] = "Day";
					element["columns"][2]["font"] = "SANSSERIF";
					element["columns"][2]["value"] ="Two Days Ago";

				//	list->addElement(element);
					vector.push_back(element);
				}
			}
		}

		if(mThreeDaysAgo_vector.size() > 0)
		{

			for(unsigned int i = 0 ; i < mThreeDaysAgo_vector.size() ; i++) 
			{
				LocationInfo info;
				std::string name;
				std::string slurl;
				info =  mThreeDaysAgo_vector.at(i);

				slurl = info.getSLURL();
				info.getRegionName(name);
				if(name != "")
				{
					element["id"] =slurl;
					element["columns"][0]["column"] = "Name";
					element["columns"][0]["value"] = name;
					element["columns"][0]["font"] = "SANSSERIF";
					element["columns"][0]["font-style"] = "NORMAL";
					element["columns"][1]["column"] = "slurl";
					element["columns"][1]["value"] = slurl;
					element["columns"][1]["font"] = "SANSSERIF";
					element["columns"][2]["column"] = "Day";
					element["columns"][2]["font"] = "SANSSERIF";
					element["columns"][2]["value"] ="Three Days Ago";
				//	list->addElement(element);
					vector.push_back(element);
				}
			}
		}

		if(mFourDaysAgo_vector.size() > 0)
		{

			for(unsigned int i = 0 ; i < mFourDaysAgo_vector.size() ; i++) 
			{
				LocationInfo info;
				std::string name;
				std::string slurl;
				info =  mFourDaysAgo_vector.at(i);

				slurl = info.getSLURL();
				info.getRegionName(name);
				if(name != "")
				{
					element["id"] =slurl;
					element["columns"][0]["column"] = "Name";
					element["columns"][0]["value"] = name;
					element["columns"][0]["font"] = "SANSSERIF";
					element["columns"][0]["font-style"] = "NORMAL";
					element["columns"][1]["column"] = "slurl";
					element["columns"][1]["value"] = slurl;
					element["columns"][1]["font"] = "SANSSERIF";
					element["columns"][2]["column"] = "Day";
					element["columns"][2]["font"] = "SANSSERIF";
					element["columns"][2]["value"] ="Four Days Ago";

				//	list->addElement(element);
					vector.push_back(element);
				}
			}
		}

		if(mFiveDaysAgo_vector.size() > 0)
		{

			for(unsigned int i = 0 ; i < mFiveDaysAgo_vector.size() ; i++) 
			{
				LocationInfo info;
				std::string name;
				std::string slurl;
				info =  mFiveDaysAgo_vector.at(i);

				slurl = info.getSLURL();
				info.getRegionName(name);
				if(name != "")
				{
					element["id"] =slurl;
					element["columns"][0]["column"] = "Name";
					element["columns"][0]["value"] =name;
					element["columns"][0]["font"] = "SANSSERIF";
					element["columns"][0]["font-style"] = "NORMAL";
					element["columns"][1]["column"] = "slurl";
					element["columns"][1]["value"] = slurl;
					element["columns"][1]["font"] = "SANSSERIF";
					element["columns"][2]["column"] = "Day";
					element["columns"][2]["font"] = "SANSSERIF";
					element["columns"][2]["value"] ="Five Days Ago";

					//list->addElement(element);
					vector.push_back(element);
				}
			}
		}

		if(mLongDaysAgo_vector.size() > 0)
		{

			for(unsigned int i = 0 ; i < mLongDaysAgo_vector.size() ; i++) 
			{
				LocationInfo info;
				std::string name;
				std::string slurl;
				info =  mLongDaysAgo_vector.at(i);

				slurl = info.getSLURL();
				info.getRegionName(name);
				if(name != "")
				{
					element["id"] =slurl;
					element["columns"][0]["column"] = "Name";
					element["columns"][0]["value"] = name;
					element["columns"][0]["font"] = "SANSSERIF";
					element["columns"][0]["font-style"] = "NORMAL";
					element["columns"][1]["column"] = "slurl";
					element["columns"][1]["value"] = slurl;
					element["columns"][1]["font"] = "SANSSERIF";
					element["columns"][2]["column"] = "Day";
					element["columns"][2]["font"] = "SANSSERIF";
					element["columns"][2]["value"] ="More Than Five Days Ago";
					//list->addElement(element);
					vector.push_back(element);
				}
			}
		}
	}

	if(mToday_vector.size()<=0 && mYesterday_vector.size()<=0 && mTwoDaysAgo_vector.size()<=0 && mThreeDaysAgo_vector.size()<=0 && mFourDaysAgo_vector.size()<=0 && mLongDaysAgo_vector.size() <= 0)
	{
		LLSD element;
		element["id"] =0;
		element["columns"][0]["column"] = "Name";
		element["columns"][0]["font"] = "SANSSERIF";
		element["columns"][0]["font-style"] = "NORMAL";
		element["columns"][1]["column"] = "slurl";
		element["columns"][1]["font"] = "SANSSERIF";
		element["columns"][2]["column"] = "Day";
		element["columns"][2]["font"] = "SANSSERIF";
		//list->addElement(element);
		vector.push_back(element);
	}
	if(asc)
	{
		for(unsigned int i=0;i<vector.size();i++)
		{
			list->addElement(vector[i]);
		}
		
	}else
	{
		for(int i=vector.size()-1;i>=0;i--)
		{
			list->addElement(vector[i]);
		}		
	}

	list->calcColumnWidths();

	return TRUE;

}

void LLNavigationList::onSortChanged(void* data)
{
	LLNavigationList* self = (LLNavigationList*) data;
	//self->buildList();
	self->list->setSorted(false);	
	if(self->list->getSortColumnName()=="Day")
	{
		sInstance->buildList(!(self->list->getSortAscending()));
		self->list->setSorted(true);		
	}
	else
	{
		self->list->setSorted(false);	
	}


}
BOOL LLNavigationList::refreshNavList()
{
	if(hasTeleported && sInstance)
	{
		return sInstance->buildList(true);
	}
	hasTeleported = FALSE;
	return TRUE;
}

void LLNavigationList::show()
{
	if (!sInstance)
	{
		sInstance = new LLNavigationList();
	}
	sInstance->open();
}
