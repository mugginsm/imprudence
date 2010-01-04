#ifndef _VNCSCREENUPDATELISTENER_H_
#define _VNCSCREENUPDATELISTENER_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class VNCScreenUpdateListener
{
public:
    virtual void screenUpdated(HDC dc, HBITMAP bitmap) = 0;

}; //   class VNCScreenUpdateListener

#undef WIN32_LEAN_AND_MEAN

#endif  //  _SCREENUPDATELISTENER_H_
