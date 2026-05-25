#pragma once

#ifdef _WIN32
#define ANTELOPE_SCRIPT_EXPORT __declspec(dllexport)
#else
#define ANTELOPE_SCRIPT_EXPORT __attribute__((visibility("default")))
#endif

#define ANTELOPE_COMPONENT()
#define ANTELOPE_FIELD()
#define ANTELOPE_SCRIPT()
#define ANTELOPE_SYSTEM()