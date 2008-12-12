/**
 * @file llfloaterexportlandmark.h
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008, Linden Research, Inc.
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
#include "lluictrl.h"
#include "lllandmarklist.h"
#include "llinventory.h"
#include "llinventoryview.h"
#include "llinventorymodel.h"

class LLFloaterExportLandmark : public LLFloater, public LLFloaterSingleton<LLFloaterExportLandmark>
{
public:
	LLFloaterExportLandmark(const LLSD& seed );
	~LLFloaterExportLandmark(void);

	static void recurse(LLUUID id, U32 level);
	static void clearVectors();
	
	// async callback; called once the landmarks are loaded and ready to be exported
	static void finishExport( LLFloaterExportLandmark* self, std::string export_type );

	static void exportToXML();
	static void exportToHTML();
	static void exportToFile();

private:

	static std::string escapeHTML(const std::string& html);

	static std::vector<LLUUID> sLandmark_id_vector;
	static std::vector<std::string> sLandmark_name_vector;

	static void onChangeText(LLUICtrl* ctrl, void* data);
	static void onClickExport(void* data);
	static void onClickCancel(void* data);
	
	static void onCommitChoice(LLUICtrl* ctrl, void* data);
	static std::string getNotes(LLUUID landmark_id);

	static void writeToOstream(std::ostream& stream, std::string demark);

	static LLLandmark* sLandmark;
	static LLUUID sLandmarkId;
    
	std::string mChoice;
};

