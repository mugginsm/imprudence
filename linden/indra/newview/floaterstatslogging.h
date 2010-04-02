#include "llviewerprecompiledheaders.h"

#include "llfloater.h"
#include "timer.h"

class LLCheckBoxCtrl;
class LLSpinCtrl;
class FloaterStatsLog : public LLFloater
{
public:
    FloaterStatsLog();

    virtual ~FloaterStatsLog();
	virtual BOOL postBuild();
    // by convention, this shows the floater and does instance management
    static void show(void*);
	static FloaterStatsLog* getInstance(){ return sInstance; }
	static BOOL session_complete;
	static S32 sessionDuration;
	static void stopLogging(void *userdata);
	static void generateGraph(void *userdata);
 
private:

    static FloaterStatsLog* sInstance;
	static std::string extraURLData;
	static void startLogging(void *userdata);
};
class LogTimer : public LLEventTimer 
{
public:
	LogTimer();
	virtual ~LogTimer();
	//function to be called at the supplied frequency
	virtual BOOL tick();
private:
	S32 count;
	S32 max;
	S32 maxPerSim;
};

