#include "llviewerprecompiledheaders.h"

#include "hippoupdate.h"

#include <cstdio>
#include <list>
#include <vector>

#include <stdtypes.h>
#include <llhttpclient.h>
#include <llmemory.h>
//#include <llversionviewer.h>
#include "llappviewer.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "llsys.h"
#include "llweb.h"
#include "viewerversion.h"
#include <llwindow.h>


std::string gHippoChannel;


// static
bool HippoUpdate::checkUpdate()
{
	llinfos << "Hippo Update Check..." << llendl;

	// get channel name
	gHippoChannel = gSavedSettings.getString("VersionChannelName");
	
	//get OS
	std::string osString;
#if LL_WINDOWS  
	osString = "win32";
#elif LL_DARWIN
	osString = "mac";
#elif LL_LINUX
	osString = "linux";
#endif

	// get mac address
	char macAddress[18];
	sprintf(macAddress, "%02x:%02x:%02x:%02x:%02x:%02x",
		gMACAddress[0], gMACAddress[1], gMACAddress[2], gMACAddress[3], gMACAddress[4], gMACAddress[5]);

	// build URL for update check
	std::stringstream sstr;	
	sstr << "http://astraviewer.com/app/viewerupdate.php?";
	sstr << "channel=" + gHippoChannel;
	sstr << "&version_major=" << llformat("%u", ViewerVersion::getImpMajorVersion());
	sstr << "&version_minor=" << llformat("%u", ViewerVersion::getImpMinorVersion());
	sstr << "&version_patch=" << llformat("%u", ViewerVersion::getImpPatchVersion());
	sstr << "&platform=" << osString;
	sstr << "&mac=" << macAddress;

	// query update server
	std::string escaped_url = LLWeb::escapeURL(sstr.str());
	LLSD response = LLHTTPClient::blockingGet(escaped_url.c_str());

	// check response, return on error
	S32 status = response["status"].asInteger();
	if ((status != 200) || !response["body"].isMap()) {
		llinfos << "Hippo Update failed (" << status << "): "
			<< (response["body"].isString()? response["body"].asString(): "<unknown error>")
			<< llendl;
		return true;
	}

	// get data from response
	LLSD data = response["body"];
	std::string webpage = (data.has("webpage") && data["webpage"].isString())? data["webpage"].asString(): "";
	std::string message = (data.has("message") && data["message"].isString())? data["message"].asString(): "";
	std::string yourVersion = (data.has("yourVersion") && data["yourVersion"].isString())? data["yourVersion"].asString(): "";
	std::string curVersion = (data.has("curVersion") && data["curVersion"].isString())? data["curVersion"].asString(): "";
	bool update = (data.has("update") && data["update"].isBoolean())? data["update"].asBoolean(): false;
	bool mandatory = (data.has("mandatory") && data["mandatory"].isBoolean())? data["mandatory"].asBoolean(): false;

	// log and return, if no update available
	llinfos << "Your version is " << yourVersion << ", current version is " << curVersion << '.' << llendl;
	if (!update) return true;
	llinfos << "Update is " << (mandatory? "mandatory.": "optional.") << llendl;

	// show update dialog
	char msg[1000];
	snprintf(msg, 1000,
		"There is a new viewer version available.\n"
		"\n"
		"Your version: %s\n"
		"Current version: %s\n"
		"%s\n"
		"Do you want to visit the web site?",
		yourVersion.c_str(), curVersion.c_str(),
		mandatory? "\nThis is a mandatory update.\n": "");
	S32 button = OSMessageBox(msg, "Astra Viewer Update", OSMB_YESNO);
	if (button == OSBTN_YES) {
		llinfos << "Taking user to " << webpage << llendl;
		LLWeb::loadURLExternal(webpage);
		// exit the viewer
		return false;
	}

	return !mandatory;
}
