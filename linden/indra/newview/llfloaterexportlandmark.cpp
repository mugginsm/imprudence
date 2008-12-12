/**
 * @file llfloaterexportlandmark.cpp
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

#include "llviewerprecompiledheaders.h"
#include "llfloaterexportlandmark.h"
#include "lluictrlfactory.h"
#include "llsdserialize.h"
#include "llradiogroup.h"

std::vector<LLUUID> LLFloaterExportLandmark::sLandmark_id_vector;
std::vector<LLString> LLFloaterExportLandmark::sLandmark_name_vector;

const char *EXPORT_FILENAME = "exported_landmarks";

const LLString FILE_EXTENSION_HTML = ".html";
const LLString FILE_EXTENSION_XML = ".xml";
const LLString FILE_EXTENSION_TXT = ".txt";

const LLString LL_HISTORY_XML_THUMBNAIL("Thumbnail");

LLFloaterExportLandmark::LLFloaterExportLandmark(const LLSD& seed):LLFloater("floater_export_landmarks")
{
	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_export_landmarks.xml" );
	childSetAction("OK_btn", onClickExport, this);
	childSetCommitCallback("export", onCommitChoice, this);
	childSetAction("cancel_btn", onClickCancel, this);

}

LLFloaterExportLandmark::~LLFloaterExportLandmark(void)
{
}

// since the library/inventory is loaded asynchronously
// safest way to export is to wait for the category we want (landmarks)
// to fully load, and then export, rather than say spinning on it or just blindly exporting whats there
class LLVFAsyncExportLandmarkObserver : public LLInventoryFetchDescendentsObserver
{
public:

	LLVFAsyncExportLandmarkObserver( LLFloaterExportLandmark* Exporter, LLString ExportType ) {
		m_Exporter = Exporter;
		m_ExportType = ExportType;
	}
	~LLVFAsyncExportLandmarkObserver() {}

	// called once inventory finishes fetching everything in the requested category/folder
	virtual void done()
	{
		// landmarks done loading
		LLFloaterExportLandmark::finishExport( m_Exporter, m_ExportType );
		gInventory.removeObserver(this);
		delete this;
	}
protected:
	LLString m_ExportType;
	LLFloaterExportLandmark* m_Exporter;
};

void LLFloaterExportLandmark::onClickExport(void* data)
{
	LLFloaterExportLandmark* self = (LLFloaterExportLandmark*)data;
	LLRadioGroup* choice = self->getChild<LLRadioGroup>("export");
	LLSD value = choice->getValue();

	LLString export_type = value.asString();

	// user wants to export data.. make sure the inventory\landmarks path is ready
	LLUUID landmarksUUID = gInventory.findCategoryUUIDForType( LLAssetType::AT_LANDMARK );		

	LLVFAsyncExportLandmarkObserver* asyncLandmarksLoader = new LLVFAsyncExportLandmarkObserver( self, export_type );

	LLInventoryFetchDescendentsObserver::folder_ref_t foldersToFetch;
	foldersToFetch.push_back( landmarksUUID );

	asyncLandmarksLoader->fetchDescendents(foldersToFetch);

	if(asyncLandmarksLoader->isEverythingComplete())
		asyncLandmarksLoader->done();
	else
		gInventory.addObserver( asyncLandmarksLoader );

	// close the export window
	self->close();
}

// called once the landmarks folders are finished loaded and ready to be written out to disk
void LLFloaterExportLandmark::finishExport( LLFloaterExportLandmark* self, LLString export_type )
{
	if( export_type == "HTML" )
		exportToHTML();
	else
	if( export_type == "XML" )
		exportToXML();
	else
		exportToFile();
}

void LLFloaterExportLandmark::onClickCancel(void* data)
{
	LLFloaterExportLandmark* self = (LLFloaterExportLandmark*)data;
	self->close();
}
void LLFloaterExportLandmark::onCommitChoice(LLUICtrl* ctrl, void* data)
{
	LLFloaterExportLandmark* self = (LLFloaterExportLandmark*)data;
	LLSD value = ctrl->getValue();
	self->mChoice = value.asString();
}

void LLFloaterExportLandmark::recurse(LLUUID id, U32 level)
{

	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;

	gInventory.getDirectDescendentsOf(id, cats, items);

	S32 count = cats->count();
	if(count>0)
	{
		S32 count = cats->count();

		for(S32 i = 0; i < count; ++i)
		{
			LLInventoryCategory* cat = cats->get(i);
			LLUUID folder_id = cat->getUUID() ;

			recurse(folder_id, ++level);

		}

	}
	S32 item_count = items->count();

	if(item_count>0)
	{			
		for(S32 j = 0; j < item_count; j++)
		{
			LLInventoryItem* item = items->get(j);
			if(item->getType() == 3)
			{
			LLString landmark_name = item->getName();
			LLUUID landmark_id = item->getAssetUUID() ;
			sLandmark_id_vector.push_back(landmark_id);
			sLandmark_name_vector.push_back(landmark_name);			
		
			}
			}

	}
}

LLString LLFloaterExportLandmark::escapeHTML(const LLString& xml)
{
	LLString out;
	for (LLString::size_type i = 0; i < xml.size(); ++i)
	{
		char c = xml[i];
		switch(c)
		{
			case '"':	out.append("&quot;");	break;
			case '&':	out.append("&amp;");	break;
			case '<':	out.append("&lt;");		break;
			case '>':	out.append("&gt;");		break;
			default:	out.push_back(c);		break;
		}
	}
	return out;
}

void LLFloaterExportLandmark::exportToHTML()
{
	LLFloaterExportLandmark::sLandmark_id_vector.clear();
	LLFloaterExportLandmark::sLandmark_name_vector.clear();
	
	LLUUID landmark_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);
	LLFloaterExportLandmark::recurse(landmark_id, 1);
	
	LLString export_file_name = gDirUtilp->getLindenUserDir() + gDirUtilp->getDirDelimiter() + EXPORT_FILENAME + FILE_EXTENSION_HTML;

	FILE *file_out = LLFile::fopen(export_file_name.c_str(), "w");

	LLString HTMLOut;

	HTMLOut += "<html>\n";
	HTMLOut += "<head><title>Your Landmarks</title></head>\n";

	HTMLOut += "<table border=\"1\">\n";
	HTMLOut += "  <tr><th>Name</th><th>Asset UUID</th></tr>\n";

	U32 count = sLandmark_id_vector.size();
	for(U32 i = 0; i < count; ++i)
	{
		LLString landmark_name = sLandmark_name_vector[i];
		LLUUID landmark_id = sLandmark_id_vector[i];

		HTMLOut += "  <tr><td>" + escapeHTML( landmark_name ) + "</td><td>" + landmark_id.asString() + "</td></tr>\n";
	}

	HTMLOut += "</table>\n";
	HTMLOut += "</html>\n";

	if (fwrite(HTMLOut.c_str(), 1, HTMLOut.length(), file_out) != HTMLOut.length())
	{
		llwarns << "Error while writing out Landmarks to HTML file" << llendl;
	} else
	{

		llinfos << "Exported landmarks to " + export_file_name << llendl;
	}

	fclose(file_out);
}

void LLFloaterExportLandmark::exportToXML()
{
	LLFloaterExportLandmark::sLandmark_id_vector.clear();
	LLFloaterExportLandmark::sLandmark_name_vector.clear();

	LLUUID landmark_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);
	LLFloaterExportLandmark::recurse(landmark_id, 1);

	LLString export_file_name = gDirUtilp->getLindenUserDir() + gDirUtilp->getDirDelimiter() + EXPORT_FILENAME + FILE_EXTENSION_XML;

	FILE *file_out = LLFile::fopen(export_file_name.c_str(), "w");

	LLString XMLOut;

	XMLOut += "<Landmarks>\n";

	U32 count = sLandmark_id_vector.size();
	for(U32 i = 0; i < count; ++i)
	{
		LLString landmark_name = sLandmark_name_vector[i];
		LLUUID landmark_id = sLandmark_id_vector[i];

		XMLOut += "  <Landmark AssetUUID=\"" + landmark_id.asString() + "\">" + escapeHTML( landmark_name )  + "</Landmark>\n";
	}

	XMLOut += "</Landmarks>\n";

	if (fwrite(XMLOut.c_str(), 1, XMLOut.length(), file_out) != XMLOut.length())
	{
		llwarns << "Error while writing out Landmarks to XML file" << llendl;
	} else
	{
		llinfos << "Exported landmarks to " + export_file_name << llendl;
	}

	fclose(file_out);
}

void LLFloaterExportLandmark::exportToFile()
{
	LLFloaterExportLandmark::sLandmark_id_vector.clear();
	LLFloaterExportLandmark::sLandmark_name_vector.clear();
	LLUUID landmark_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);				
	LLFloaterExportLandmark::recurse(landmark_id, 1);
	LLString export_file_name = gDirUtilp->getLindenUserDir();

	export_file_name += gDirUtilp->getDirDelimiter();
	export_file_name += EXPORT_FILENAME + FILE_EXTENSION_TXT;

	LLFILE *fOut = LLFile::fopen(export_file_name.c_str(), "w+");

	std::ostringstream ostream;
	LLString demark= "*";
	writeToOstream(ostream, demark);

	LLString outstring = ostream.str();
	if (fwrite(outstring.c_str(), 1, outstring.length(), fOut) != outstring.length())
	{
		llwarns << "Short write" << llendl;
	}
	fclose(fOut);
}

void LLFloaterExportLandmark::writeToOstream(std::ostream& stream, LLString demark)
{	
	if(sLandmark_id_vector.size()>0)
	{
		U32 count = sLandmark_id_vector.size();

		for(U32 i = 0; i < count; ++i)
		{
			LLString landmark_name = sLandmark_name_vector[i];
			LLUUID landmark_id = sLandmark_id_vector[i];
			stream<<"Asset UUID"<<"\t"<<landmark_id.asString().c_str()<<"\n";
			stream<<"Landmark Name"<<"\t"<<landmark_name.c_str()<<"\n";
			stream<<demark.c_str()<<"\n";
		}
	}
}
