/**
 * @file llgenericmultiplexer.h
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
#ifndef LL_GENERICMULTIPLEXER_H
#define LL_GENERICMULTIPLEXER_H

#include <vector>

template< class T >
class LLVFGenericMultiplexer;

// implementation is in llfloatermanagelandmark.cpp
template< class T >
class LLVFGenericObserver
{
public:

	LLVFGenericObserver() {}
	virtual ~LLVFGenericObserver() {}

	typedef T TObserverUpdateType;

	// observers can detach from the subject by returning TRUE
	virtual BOOL onObserverUpdate( T& Data ) = 0;// { llinfos << "LLVFGenericObserver::onObserverUpdate(); non-overriden callback" << llendl; return TRUE; }
};

// TODO: add an LLSingletonManager system for easy cleanup of singleton objects
// otherwise we cant make use of the apr based mutex, since a static singleton might get cleaned up after apr during shutdown
template< class T >
class LLVFGenericMultiplexer
{
public:

	typedef T ClassType;

	LLVFGenericMultiplexer()// : mObserverListMutex(NULL)
	{
		
	}

	static LLVFGenericMultiplexer<T>* getInstance()
	{
		static LLVFGenericMultiplexer<T> instance;
		return &instance;
	}

	void addObserver(LLVFGenericObserver<T>* observer) {
		llinfos << "LLVFGenericMultiplexer::addObserver(); adding observer: " << observer << llendl;
		//LLMutexLock lock( &mObserverListMutex );
		mObservers.push_back(observer);
	}

	void removeObserver(LLVFGenericObserver<T>* observer)
	{
		llinfos << "LLVFGenericMultiplexer::removeObserver(); removing observer: " << observer << llendl;
		//LLMutexLock lock( &mObserverListMutex );

		typename observer_list_t::iterator tmp = mObservers.end();

		for( typename observer_list_t::iterator iter = mObservers.begin(); iter != mObservers.end(); iter ++ )
		{
			if( (*iter) == observer )
				tmp = iter;
		}

		if( tmp != mObservers.end() )
			mObservers.erase( tmp );
	}

	BOOL containsObserver(LLVFGenericObserver<T>* observer) {
		//LLMutexLock lock( &mObserverListMutex );
		return mObservers.find(observer) != mObservers.end();
	}

	void notifyObservers( T& newData )
	{
		//LLMutexLock lock( &mObserverListMutex );

		for ( typename observer_list_t::iterator iter = mObservers.begin(); iter != mObservers.end(); )
		{
			LLVFGenericObserver<T>* observer = *iter;

			llinfos << "LLVFGenericMultiplexer::notifyObservers(); notifying observer: " << observer << llendl;

			// if observer returned TRUE, remove it from the list
			if( observer->onObserverUpdate( newData ) )
				iter = mObservers.erase( iter );
			else
				++ iter;
		}
	}

	//LLMutex mObserverListMutex;
	typedef std::vector< LLVFGenericObserver<T>* > observer_list_t;
	observer_list_t mObservers;
};

#endif
