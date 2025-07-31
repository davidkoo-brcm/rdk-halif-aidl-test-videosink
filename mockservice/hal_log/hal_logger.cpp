#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>

static bool s_VerboseLog = false;

typedef enum eMessageLevel_
{
    eTrace      = 0,
    eWarning    = 1,
    eError      = 2
} eMessageLevel;

extern "C" void HALUTIL_Logger_Message(eMessageLevel level, const char* file, const char* function, int line, const char * format, ...)
{
}