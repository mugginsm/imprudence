/** 
 * @file llrexhud.h
 * @llrexhud class header file
 *
 * Copyright (c) 2007-2008 LudoCraft
 */

#ifndef LL_LLREXHUD_H
#define LL_LLREXHUD_H

#include "llview.h"


class RexScriptCommandDispatchHandler : public LLDispatchHandler
{
public:
   //! Handle dispatch
	virtual bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& string);

   virtual std::string getKey() const { return key; }

   const static std::string key;
};





class LLRexHud : public LLView
{
protected:

public:
	LLRexHud(const std::string& name, const LLRect& rect, BOOL mouse_opaque, U32 follows=FOLLOWS_NONE);
	virtual ~LLRexHud( void );

	void draw();
	void toggleHudTestMode();
	void showInvMessage(std::string vMessage);
	void ProcessServerCommand(std::string vUnit,std::string vCommand,std::string vCommandParams);

	LLColor4 DrawColor;

	static bool mSendMouseButtons,mSendMouseWheel,mScriptDeaf,mScriptMute;


private:
	void ProcessSetRotCommand(std::string vCommand,bool vbRelative);

	static RexScriptCommandDispatchHandler smRexScriptCommandHandler;
};






#endif //LL_LLREXHUD_H