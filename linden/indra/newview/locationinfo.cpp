/**
 * @file locationinfo.cpp
   @author Vectorform
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2008, Linden Research, Inc.
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
#include "locationinfo.h"

LocationInfo::LocationInfo()
{
}

LocationInfo::~LocationInfo()
{
}

void LocationInfo::setRegionName(const std::string& name)
{
	mRegionName = name;
}

void LocationInfo::getRegionName(std::string& name) const
{
	name = mRegionName;
}

const std::string& LocationInfo::getRegionName() const
{
	return mRegionName;
}


void LocationInfo::setRegionPosition(const LLVector3d& pos)
{
	mRegionPosition = pos;
}

void LocationInfo::getRegionPosition(LLVector3d& pos) const
{
	pos = mRegionPosition;
}

void LocationInfo::setRegionNotes(const std::string& notes)
{
	mRegionNotes = notes;
}

void LocationInfo::getRegionNotes(std::string& notes) const
{
	notes = mRegionNotes;
}

void LocationInfo::setAssetId(const LLUUID& asset_id)
{
	mAssetId = asset_id;
}

void LocationInfo::getAssetId(LLUUID& asset_id) const
{
	asset_id=mAssetId;
}

void LocationInfo::setSLURL(const std::string& slurl)
{
	mSLURL = slurl;
}

std::string LocationInfo::getSLURL() const
{
	return mSLURL;
}

bool LocationInfo::isSameRegionLocation(const LocationInfo& rhs) const
{
	return mRegionName == rhs.mRegionName
		&& mRegionPosition == rhs.mRegionPosition;
}

