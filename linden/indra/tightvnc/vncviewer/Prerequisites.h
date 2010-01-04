#ifndef _PREREQUISITES_H_
#define _PREREQUISITES_H_

#ifdef WIN32
#ifndef TIGHTVNC_EXPORT
#ifdef TIGHTVNC_EXPORTS
#define TIGHTVNC_EXPORT __declspec(dllexport)
#pragma warning(disable:4251)
#pragma warning(disable:4275)
#else
#define TIGHTVNC_EXPORT __declspec(dllimport)
#endif
#endif
#endif

#endif  //  _PREREQUISITES_H_
