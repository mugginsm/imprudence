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
std::string STAT_VALUES = "&chds=0,50,0,2000,0,1.0,0,45,0,10000";
//Labels and colors
std::string CHART_LABELS = "&chdl=FPS|KBPS|Dilation|Sim FPS|Scripts&chco=FF0000,00FF00,0000FF,FFFF00,00FFFF";

//Not sure if this will actually be used, if it is it simply needs to be filled out with the relevant data and added anywhere in the url in generateGraph()
std::string AXIS_LABELS = "&chxt=axis 1 label,axis 2 label";

std::string CHART_SIZE = "&chs=1000x300";

//data structures to store our stats, everything is done within this class to keep it modular and also because viewerstats doesn't have everything we need.
std::vector<F32> viewer_fps;
std::vector<F32> viewer_bandwith;
std::vector<F32> time_dilation;
std::vector<F32> sim_fps;
std::vector<S32> sim_scripts;
//std::vector<S32> ping; 
//std::vector<S32> sim_objects;

FloaterStatsLog* FloaterStatsLog::sInstance = NULL;
BOOL FloaterStatsLog::session_complete = FALSE;
S32 FloaterStatsLog::sessionDuration = 0;
std::string FloaterStatsLog::extraURLData = "";
BOOL FloaterStatsLog::logPing = FALSE;
BOOL FloaterStatsLog::logObjectCount = FALSE;

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
	childSetAction("generate_graph", generateGraph, this);
	childSetCommitCallback("session_duration",onCommitDuration,this);
	childSetText("time_left",std::string("No Session"));
	return true;
}
/*Since we know timers are imprecise when
it comes to time we're going to use the 
same time to account for both our collection
and timekeeping, this way we know our data 
is accurate*/

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
		time_dilation.push_back(LLViewerStats::getInstance()->mSimTimeDilation.getCurrent());
		sim_fps.push_back(LLViewerStats::getInstance()->mSimFPS.getCurrent());
		sim_scripts.push_back(LLViewerStats::getInstance()->mSimActiveScripts.getCurrent());

		//if(FloaterStatsLog::logPing)
		//ping.push_back(LLViewerStats::getInstance()->mSimPingStat.getCurrent());
		//if(FloaterStatsLog::logObjectCount)
		//sim_objects.push_back(LLViewerStats::getInstance()->mSimObjects.getCurrent());

		count++;

		F32 temp = (max - count) / 60;
		S32 minutes = llround(temp);
		S32 seconds = llround((temp - minutes) * 60);
		FloaterStatsLog::getInstance()->childSetTextArg("time_left", "[MINS]", IntToStr(minutes));
		FloaterStatsLog::getInstance()->childSetTextArg("time_left", "[SECS]", IntToStr(seconds));

		return FALSE;
	}
	else
	{
		FloaterStatsLog::stopLogging(FloaterStatsLog::getInstance());
		FloaterStatsLog::session_complete = TRUE;
		return TRUE;
	}
};

void FloaterStatsLog::onCommitDuration(LLUICtrl* ctrl,void *userdata)
{
	sessionDuration = ((LLSpinCtrl*)ctrl)->getValue().asReal(); 
}

void FloaterStatsLog::startLogging(void *userdata)
{
	FloaterStatsLog* self = (FloaterStatsLog*) userdata;
	self->childSetColor("status_button",LLColor4::green);
	self->childSetEnabled("start_logging",FALSE);
	self->childSetEnabled("stop_logging",TRUE);
	session_complete = FALSE; //We're starting a new session so it isn't complete
	
	//we'll just grab the checkbox values on start that way we don't need to add callbacks for each new 
	FloaterStatsLog::logPing = self->getChild<LLCheckBoxCtrl>("FSLLogObjectCount")->get();
	FloaterStatsLog::logObjectCount = self->getChild<LLCheckBoxCtrl>("FSLLogObjectCount")->get();

	viewer_fps.clear(); //Make sure our data structures are empty
	viewer_bandwith.clear();
	time_dilation.clear();
	sim_fps.clear();
	//ping.clear(); 
	//sim_objects.clear();
	sim_scripts.clear();
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
void cmdline_printchat(std::string message);
void FloaterStatsLog::generateGraph(void *userdata)
{
	if(!session_complete) return; //Don't want to send an incomplete session
	
	std::string viewername = IMP_VIEWER_NAME;
	//base url and we'll make our title the viewername for now, we could maybe have a line editor for this later?
	std::string url = "http://chart.apis.google.com/chart?cht=lc&chtt=" + viewername;
	if(!extraURLData.empty())
	url += extraURLData;
	
	
	/**********
	If the number of stats is increased, the code below needs to be modified to sample less data, otherwise google will return an error due to url length.
	***********/
	//Lets graph per 5 seconds if we have more than 2 minutes and per minute if we have more than 5, because google fails if the provided url is too long
	
	S32 total_count = viewer_fps.size();
	F32 tempX = total_count / 60;
	std::string data_string;
	if(tempX >= 2.f && tempX < 5.f)
	{
		S32 x;
		for(x = 0; x < total_count; x += 5)
		{
			data_string += FloatToStr(viewer_fps[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|"; //it's more efficient to leave the for() loop as is and simply remove the extra , from the last value added
		for(x = 0; x < total_count; x += 5)
		{
			data_string += IntToStr(viewer_bandwith[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x += 5)
		{
			data_string += FloatToStr(time_dilation[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x += 5)
		{
			data_string += FloatToStr(sim_fps[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x += 5)
		{
			data_string += IntToStr(sim_scripts[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1);
		/*
		for(x = 0; x < total_count; x += 5)
		{
			data_string += IntToStr(ping[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x += 5)
		{
			data_string += IntToStr(sim_objects[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		*/
	}
	else if(tempX > 5.f)
	{
		S32 x;
		for(x = 0; x < total_count; x += 60)
		{
			data_string += FloatToStr(viewer_fps[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|"; //it's more efficient to leave the for() loop as is and simply remove the extra , from the last value added
		for(x = 0; x < total_count; x += 60)
		{
			data_string += IntToStr(viewer_bandwith[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x += 60)
		{
			data_string += FloatToStr(time_dilation[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x += 60)
		{
			data_string += FloatToStr(sim_fps[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x += 60)
		{
			data_string += IntToStr(sim_scripts[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1);
		/*
		for(x = 0; x < total_count; x += 60)
		{
			data_string += IntToStr(ping[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x += 60)
		{
			data_string += IntToStr(sim_objects[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		*/
	}
	else
	{
		S32 x;
		for(x = 0; x < total_count; x++)
		{
			data_string += FloatToStr(viewer_fps[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|"; //it's more efficient to leave the for() loop as is and simply remove the extra , from the last value added
		for(x = 0; x < total_count; x++)
		{
			data_string += IntToStr(viewer_bandwith[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x++)
		{
			data_string += FloatToStr(time_dilation[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x++)
		{
			data_string += FloatToStr(sim_fps[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x++)
		{
			data_string += IntToStr(sim_scripts[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1);
		/*
		for(x = 0; x < total_count; x++)
		{
			data_string += IntToStr(ping[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		for(x = 0; x < total_count; x++)
		{
			data_string += IntToStr(sim_objects[x]) + ",";
		}
		data_string = data_string.substr(0,data_string.length() - 1) + "|";
		*/
	}

	url += "&chd=t:" + data_string + STAT_VALUES + CHART_SIZE + CHART_LABELS;
	cmdline_printchat(url);
	LLWeb::loadURL(url);
}
