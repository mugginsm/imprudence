/**
 * @file locationinfo.h
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

#ifndef LL_LOCATIONINFO_H
#define LL_LOCATIONINFO_H

class LocationInfo
{
public:
	LocationInfo();
	~LocationInfo();

	void setRegionName(const std::string& name);
	void getRegionName(std::string& name) const;
	const std::string& getRegionName() const;
	void setRegionPosition(const LLVector3d& pos);
	void getRegionPosition(LLVector3d& pos) const;
	void setRegionNotes(const std::string& notes);
	void getRegionNotes(std::string& notes) const;
	void setAssetId(const LLUUID& asset_id);
	void getAssetId(LLUUID& asset_id) const;
	void setSLURL(const std::string& slurl);
	std::string getSLURL() const;

	bool isSameRegionLocation(const LocationInfo& rhs) const;


private:
	std::string mRegionName;
	LLVector3d mRegionPosition;
	std::string mRegionNotes;
	LLUUID mAssetId;
	std::string mSLURL;
};

#endif
