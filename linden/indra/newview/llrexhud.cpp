/** 
 * @file llrexhud.cpp
 * @brief llrexhud class implementation
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#include "llviewerprecompiledheaders.h"
#include "llrexhud.h"
#include "llviewercontrol.h"
#include "llgl.h"
#include "llviewerimage.h"
#include "llviewerimagelist.h"
#include "llviewerwindow.h"
#include "llviewergenericmessage.h"
#include "llagent.h"
#include "llappviewer.h"
#include "llresmgr.h"
#include "llvieweraudio.h"

#include <exception>
#include <boost/python.hpp>


using namespace boost::python;

// Global pointer to hud object. 
extern LLRexHud* gRexHud;
RexScriptCommandDispatchHandler LLRexHud::smRexScriptCommandHandler;
const LLString RexScriptCommandDispatchHandler::key = "RexScr";

bool LLRexHud::mSendMouseButtons = false;
bool LLRexHud::mSendMouseWheel = false;
bool LLRexHud::mScriptDeaf = false;
bool LLRexHud::mScriptMute = false;


extern LLAgent gAgent;



// Call exposed to python named with rxXXX
void rxSetDrawColor(float vR, float vG, float vB, float vA) 
{
	if(gRexHud)
	{
		gRexHud->DrawColor = LLColor4(vR,vG,vB,vA);
	}
}

void rxDrawImage(std::string vTexName, float vX, float vY) 
{
	if(gRexHud)
	{
        LLUIImage* imagep = LLUI::getUIImage(vTexName);
        if (imagep)
        {
            LLImageGL* imagegl = imagep->getImage();
            if (imagegl)
                gl_draw_image(S32(vX),S32(vY),imagegl,gRexHud->DrawColor);
        }
	}
}

void rxDrawImageScaled(std::string vTexName, float vX, float vY, float vWitdh, float vHeight) 
{
	if(gRexHud)
	{
        LLUIImage* imagep = LLUI::getUIImage(vTexName);
        if (imagep)
        {
            LLImageGL* imagegl = imagep->getImage();
            if (imagegl)
		        gl_draw_scaled_image(S32(vX),S32(vY),S32(vWitdh),S32(vHeight),imagegl,gRexHud->DrawColor);
        }
	}
}

void rxDrawText(std::string vText, float vX, float vY, int vFontType, int vHAlign, int vVAlign) 
{
	if(gRexHud)
	{
		switch(vFontType)
		{
			case 1:  LLResMgr::getInstance()->getRes(LLFONT_SMALL)->renderUTF8(vText, 0,S32(vX),S32(vY), gRexHud->DrawColor,LLFontGL::HAlign(vHAlign),LLFontGL::VAlign(vVAlign)); break;
			case 2:  LLResMgr::getInstance()->getRes(LLFONT_OCRA)->renderUTF8(vText, 0,S32(vX),S32(vY), gRexHud->DrawColor,LLFontGL::HAlign(vHAlign), LLFontGL::VAlign(vVAlign)); break;
			case 3:  LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF)->renderUTF8(vText, 0,S32(vX),S32(vY), gRexHud->DrawColor,LLFontGL::HAlign(vHAlign), LLFontGL::VAlign(vVAlign)); break;
			case 4:  LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF_SMALL)->renderUTF8(vText, 0,S32(vX),S32(vY), gRexHud->DrawColor,LLFontGL::HAlign(vHAlign), LLFontGL::VAlign(vVAlign)); break;
			case 5:  LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF_BIG)->renderUTF8(vText, 0,S32(vX),S32(vY), gRexHud->DrawColor,LLFontGL::HAlign(vHAlign), LLFontGL::VAlign(vVAlign)); break;
			case 6:  LLFontGL::sSansSerifHuge->renderUTF8(vText, 0,S32(vX),S32(vY), gRexHud->DrawColor,LLFontGL::HAlign(vHAlign), LLFontGL::VAlign(vVAlign)); break;
			case 7:  LLFontGL::sSansSerif42->renderUTF8(vText, 0,S32(vX),S32(vY), gRexHud->DrawColor,LLFontGL::HAlign(vHAlign), LLFontGL::VAlign(vVAlign)); break;
			default: LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF)->renderUTF8(vText, 0,S32(vX),S32(vY), gRexHud->DrawColor,LLFontGL::HAlign(vHAlign), LLFontGL::VAlign(vVAlign)); break;
		}
	}
}

S32 rxGetTextWidth(std::string vText,int vFontType) 
{
	if(gRexHud)
	{
		switch(vFontType)
		{
			case 1: return LLResMgr::getInstance()->getRes(LLFONT_SMALL)->getWidth(vText); break;
			case 2: return LLResMgr::getInstance()->getRes(LLFONT_OCRA)->getWidth(vText); break;
			case 3: return LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF)->getWidth(vText); break;
			case 4: return LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF_SMALL)->getWidth(vText); break;
			case 5: return LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF_BIG)->getWidth(vText); break;
			case 6: return LLFontGL::sSansSerifHuge->getWidth(vText); break;
			case 7: return LLFontGL::sSansSerif42->getWidth(vText); break;
			default: return LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF)->getWidth(vText); break;
		}
	}
	return 0;
}

S32 rxGetTextHeight(int vFontType) 
{
	if(gRexHud)
	{
		switch(vFontType)
		{
			case 1: return S32(LLResMgr::getInstance()->getRes(LLFONT_SMALL)->getLineHeight()); break;
			case 2: return S32(LLResMgr::getInstance()->getRes(LLFONT_OCRA)->getLineHeight()); break;
			case 3: return S32(LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF)->getLineHeight()); break;
			case 4: return S32(LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF_SMALL)->getLineHeight()); break;
			case 5: return S32(LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF_BIG)->getLineHeight()); break;
			case 6: return S32(LLFontGL::sSansSerifHuge->getLineHeight()); break;
			case 7: return S32(LLFontGL::sSansSerif42->getLineHeight()); break;
			default: return S32(LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF)->getLineHeight()); break;
		}
	}
	return 0;
}

BOOST_PYTHON_MODULE(rxCanvas)
{
    def("rxDrawImage", rxDrawImage);
    def("rxDrawImageScaled", rxDrawImageScaled);
    def("rxDrawText", rxDrawText);
    def("rxSetDrawColor", rxSetDrawColor);
    def("rxGetTextWidth", rxGetTextWidth);
    def("rxGetTextHeight", rxGetTextHeight);
}

// Python log message redirection to viewer log file.
PyObject* log_CaptureStdout(PyObject* self, PyObject* pArgs)
{
	char* LogStr = NULL;
	if (!PyArg_ParseTuple(pArgs, "s", &LogStr)) 
		return NULL;

	llwarns << LogStr << llendl;
	Py_INCREF(Py_None);
	return Py_None;
}

PyObject* log_CaptureStderr(PyObject* self, PyObject* pArgs)
{
	char* LogStr = NULL;
	if (!PyArg_ParseTuple(pArgs, "s", &LogStr)) 
		return NULL;

	llwarns << LogStr << llendl;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef logMethods[] = {
 {"CaptureStdout", log_CaptureStdout, METH_VARARGS, "Logs stdout"},
 {"CaptureStderr", log_CaptureStderr, METH_VARARGS, "Logs stderr"},
 {NULL, NULL, 0, NULL}
};









LLRexHud::LLRexHud(const LLString& name, const LLRect& rect, BOOL mouse_opaque, U32 follows) : LLView(name,rect,mouse_opaque,follows)
{
	try
	{
		gGenericDispatcher.addHandler(smRexScriptCommandHandler.getKey(), &smRexScriptCommandHandler);
		LLRexHud::mSendMouseButtons = false;
		LLRexHud::mSendMouseWheel = false;

		DrawColor = LLColor4(1,1,1,1);

		Py_Initialize();
		Py_InitModule("log", logMethods);
		PyRun_SimpleString(
			"import log\n"
			"import sys\n"
			"class StdoutCatcher:\n"
			"\tdef write(self, str):\n"
			"\t\tlog.CaptureStdout(str)\n"
			"class StderrCatcher:\n"
			"\tdef write(self, str):\n"
			"\t\tlog.CaptureStderr(str)\n"
			"sys.stdout = StdoutCatcher()\n"
			"sys.stderr = StderrCatcher()\n"
		);
	
		initrxCanvas();
		PyRun_SimpleString("import rxCanvas");

		object main_module((handle<>(borrowed(PyImport_AddModule("__main__")))));
		object main_namespace = main_module.attr("__dict__");
//		std::string mainfile = gDirUtilp->getExecutableDir() + "\\lib\\rxclientcore\\rxclientmain.py";
		std::string mainfile = gDirUtilp->getWorkingDir() + "/lib/rxclientcore/rxclientmain.py";

		exec_file(mainfile.c_str(), main_namespace, main_namespace);
		PyRun_SimpleString("DoInit()");
	}
	catch(int e)
	{
		llwarns << "LLRexHud::LLRexHud exception:" << e << llendl;
	}
	catch (error_already_set &)
	{
		llwarns << "Python exception." << llendl;
	}
    catch (std::exception& e)
    {
        llwarns << "General exception " << llendl;
    }
}

LLRexHud::~LLRexHud()
{
	Py_Finalize();	
}

void LLRexHud::draw()
{
	std::stringstream ss;
	std::string strw,strh;

	ss << gViewerWindow->getWindowDisplayWidth();
	ss >> strw;
	
	ss.clear();
	ss << gViewerWindow->getWindowDisplayHeight();
	ss >> strh;

	std::string fullcommand = std::string("DrawHudTick(") + strw.c_str() + std::string(",") + strh.c_str() + std::string(")");
	PyRun_SimpleString(fullcommand.c_str());
	ss.clear();
}

void LLRexHud::toggleHudTestMode()
{
	PyRun_SimpleString("ToggleHudTestMode()");
}

void LLRexHud::showInvMessage(std::string vMessage)
{
	std::string TempLine;

	TempLine = "ShowInventoryMessage(\"" + vMessage + "\")";
	PyRun_SimpleString(TempLine.c_str());
}

void LLRexHud::ProcessServerCommand(std::string vComponent,std::string vCommand,std::string vCommandParams)
{
	std::string TempLine;

	// Process commands with component as client
	if(vComponent == "client")
	{
		if (vCommand == "mousebtns")
		{
			if(vCommandParams == "1")
				LLRexHud::mSendMouseButtons = true;
			else
				LLRexHud::mSendMouseButtons = false;
		}	
		else if (vCommand == "mousewheel")
		{
			if(vCommandParams == "1")
				LLRexHud::mSendMouseWheel = true;
			else
				LLRexHud::mSendMouseWheel = false;
		}
		else if (vCommand == "setrot")
			ProcessSetRotCommand(vCommandParams,false);
		else if (vCommand == "setrelrot")
			ProcessSetRotCommand(vCommandParams,true);
        else if (vCommand == "deaf")
        {
            if(vCommandParams == "1")
            {
                LLRexHud::mScriptDeaf = true;
                audio_update_volume(true);		
            }
            else
            {
                LLRexHud::mScriptDeaf = false;
                audio_update_volume(true);
            }
        }         
        else if (vCommand == "mute")
        {
            if(vCommandParams == "1")
            {
                LLRexHud::mScriptMute = true;
            }
            else
            {
                LLRexHud::mScriptMute = false;
            }
        }
		return;
	}

	// Other components are assumed as python components
	try
	{
		TempLine = "ProcessServerCommand('" + vComponent + "','" + vCommand + "')";
		PyRun_SimpleString(TempLine.c_str());
	}
	catch(int e)
	{
		llwarns << "LLRexHud::RunScriptCommand exception:" << e << llendl;
	}
}





void LLRexHud::ProcessSetRotCommand(std::string vParams, bool vbRelative)
{
	LLQuaternion newrot = LLQuaternion::DEFAULT;
	if(LLQuaternion::parseQuat(vParams.c_str(), &newrot))
	{
		if(vbRelative)
			gAgent.rotate(newrot);
		else
			gAgent.setLookDir(newrot);
	}
}

bool RexScriptCommandDispatchHandler::operator ()(const LLDispatcher *dispatcher, const std::string &key, const LLUUID &invoice, const LLDispatchHandler::sparam_t &string)
{
	if (string.size() >= 2)
	{
		LLString StrComponentName(string[0]);
		LLString StrCommand(string[1]);

		LLString StrCommandParams = "";
		if(string.size() > 2)
			StrCommandParams = string[2];

		gRexHud->ProcessServerCommand(StrComponentName,StrCommand,StrCommandParams);
		return true;
	}
	else
		return false;
}
