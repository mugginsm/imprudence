#include "llviewerprecompiledheaders.h" 

#include "floaterstatslogging.h"

#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llspinctrl.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewerstats.h"
#include "llversionviewer.h"
#include "chatbar_as_cmdline.h"
#include "llweb.h"
#include "llpanel.h"
#include "llagent.h"
#include "llviewerwindow.h"

//using a stringstream instead of itoa to avoid using char buffers
std::string IntToStr(S32 n)
{
  std::ostringstream result;
  result << n;
  return result.str();
}
std::string FloatToStr(F32 n)
{
  std::ostringstream result;
  result << n;
  return result.str();
}

/*To add a new stat to be logged and charted, there are a few simple steps to follow:
First, create a data structure to store the values.
Next, add a line to the timer's tick() function to grab the current value of that stat and store it.
Then, add a line to the generateGraph function to retrieve that data and possibly generate averages if the session was too long.
Optionally, you might want to add a stat min and max value to the URL string (Add these in STAT_VALUES <min>,<max>).

Keep in mind that while I have defined
a maximum session length before things 
start getting averaged to keep the url
length sane, the more stats you add to
this file the more aggressive you will
need to be to keep your url size down.
*/


//If these are to be used, add data set info in the same order as the data structures are declared.
//                               RANGE
std::string STAT_VALUES = "&chds=0,50,0,1000,0,200";
//Labels and colors               LABEL              COLOR                    SPACING                 RANGE
std::string CHART_LABELS = "&chdl=FPS|KBPS|Ping&chco=FF0000,00FF00,0000FF&chg=4.16,10,1,2&chxt=x&chxr=0,0,60";

//Not sure if this will actually be used, if it is it simply needs to be filled out with the relevant data and
//added anywhere in the url in generateGraph()
std::string AXIS_LABELS = "&chxt=axis 1 label,axis 2 label";

std::string CHART_SIZE = "&chs=1000x300";

//data structures to store our stats, everything is done within this class to keep it modular and also because
//viewerstats doesn't have everything we need.
std::vector<F32> viewer_fps;
std::vector<F32> viewer_bandwith;
std::vector<S32> ping; 

FloaterStatsLog* FloaterStatsLog::sInstance = NULL;
BOOL FloaterStatsLog::session_complete = FALSE;
S32 FloaterStatsLog::sessionDuration = 0;
std::string FloaterStatsLog::extraURLData = "";

FloaterStatsLog::FloaterStatsLog()
:LLFloater(std::string("floater_stats_logging"))
{
    LLUICtrlFactory::getInstance()->buildFloater(this, "floater_stats_logging.xml");
}
FloaterStatsLog::~FloaterStatsLog()
{
    sInstance = NULL;
}
// static
void FloaterStatsLog::show(void*)
{
    if (!sInstance)
	sInstance = new FloaterStatsLog();
    sInstance->open();
}

BOOL FloaterStatsLog::postBuild()
{
	childSetAction("start_logging", startLogging,this);
	childSetAction("stop_logging", stopLogging,this);
	childSetColor("status_button",LLColor4::red);
	//childSetText("time_left",std::string("No Session"));
	return true;
}
/*Since we know timers are imprecise when it comes to time we're going to use the same time
to account for both our collection and timekeeping, this way we know our data is accurate*/

LogTimer::LogTimer() : LLEventTimer( 1.0 )
{
	count = 0;
	max = FloaterStatsLog::sessionDuration;
	//maxPerSim = FloaterStatsLog::timePerSim;
};

LogTimer::~LogTimer()
{
}

BOOL LogTimer::tick()
{
	if(count < max && !FloaterStatsLog::session_complete)
	{
		viewer_fps.push_back(LLViewerStats::getInstance()->mFPSStat.getCurrentPerSec());
		viewer_bandwith.push_back(LLViewerStats::getInstance()->mKBitStat.getCurrent());
		ping.push_back(LLViewerStats::getInstance()->mSimPingStat.getCurrent());

		count++;
		FloaterStatsLog::getInstance()->childSetTextArg("samples", "[TOTAL]", IntToStr(count));

		F32 temp = max - count;
		S32 seconds = llround(temp);
		FloaterStatsLog::getInstance()->childSetTextArg("time_left", "[SECS]", IntToStr(seconds));

		return FALSE;
	}
	else
	{
		FloaterStatsLog::stopLogging(FloaterStatsLog::getInstance());
		FloaterStatsLog::generateGraph(FloaterStatsLog::getInstance());
		return TRUE;
	}
};

void cmdline_printchat(std::string message);

void FloaterStatsLog::startLogging(void *userdata)
{
	FloaterStatsLog* self = (FloaterStatsLog*) userdata;
	self->childSetColor("status_button",LLColor4::green);
	self->childSetEnabled("start_logging",FALSE);
	self->childSetEnabled("stop_logging",TRUE);
	session_complete = FALSE; //We're starting a new session so it isn't complete
	
	FloaterStatsLog::sessionDuration = self->getChild<LLSpinCtrl>("session_duration")->getValue().asReal(); 

	viewer_fps.clear(); //Make sure our data structures are empty
	viewer_bandwith.clear();
	ping.clear(); 

	//Tell our LSL test to start
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ChatFromViewer);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_ChatData);
	msg->addStringFast(_PREHASH_Message, "start");
	msg->addU8Fast(_PREHASH_Type, CHAT_TYPE_WHISPER);
	msg->addS32("Channel", 0);

	gAgent.sendReliableMessage();

	cmdline_printchat(FloatToStr(gFrameTimeSeconds));

	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_CHAT_COUNT);

	new LogTimer(); //generate the timer that is doing the logging.
}

void FloaterStatsLog::stopLogging(void *userdata)
{
	FloaterStatsLog* self = (FloaterStatsLog*) userdata;
	self->childSetColor("status_button",LLColor4::red);
	self->childSetEnabled("start_logging",TRUE);
	self->childSetEnabled("stop_logging",FALSE);
	//We need to make sure the session is stopped, to prevent a running session from affecting ongoing stat logging.
	session_complete = TRUE;
}

void FloaterStatsLog::generateGraph(void *userdata)
{
	if(!session_complete) return; //Don't want to send an incomplete session

	cmdline_printchat(FloatToStr(gFrameTimeSeconds));

	FloaterStatsLog* self = (FloaterStatsLog*) userdata;

	std::string viewername = IMP_VIEWER_NAME;
	//base url and we'll make our title the viewername for now, we could maybe have a line editor for this later?
	std::string url = "http://chart.apis.google.com/chart?cht=lc&chtt=" + viewername;
	if(!extraURLData.empty())
	url += extraURLData;
	
	/**********
	If the number of stats is increased, the code below needs to be modified to sample less data, otherwise google will return an error due to url length.
	***********/

	S32 total_count = viewer_fps.size();
	std::string data_string, temp, clipboard_string;

	S32 x;

	//Print FPS stats
	for(x = 0; x < total_count; x++)
	{
		temp += FloatToStr(llround(viewer_fps[x],0.1f)) + ",";
	}
	//it's more efficient to leave the for() loop as is and simply remove the extra , from the last value added
	temp = temp.substr(0,temp.length() - 1); 

	self->childSetText("fps_csv", temp);
	
	clipboard_string += temp + "\n";
	data_string +=  temp + "|";
	temp = "";

	//Print bandwidth stats
	for(x = 0; x < total_count; x++)
	{
		temp += IntToStr(viewer_bandwith[x]) + ",";
	}
	temp = temp.substr(0,temp.length() - 1);
	
	self->childSetText("bw_csv", temp);

	clipboard_string += temp + "\n";
	data_string +=  temp + "|";
	temp = "";

	//Print ping stats
	for(x = 0; x < total_count; x++)
	{
		temp += IntToStr(ping[x]) + ",";
	}

	temp = temp.substr(0,temp.length() - 1);

	self->childSetText("ping_csv", temp);

	clipboard_string += temp + "\n";
	data_string +=  temp;

	url += "&chd=t:" + data_string + STAT_VALUES + CHART_SIZE + CHART_LABELS;
	cmdline_printchat(url);
	gViewerWindow->mWindow->copyTextToClipboard(utf8str_to_wstring(clipboard_string + url));
	LLWeb::loadURL(url);
}
