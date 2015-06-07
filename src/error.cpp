//
// Created by Tony Hoyle on 30/05/15.
//

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

#include "error.h"

error::error(const char *text, ...)
{
    va_list arg;

    va_start(arg, text);
    vsnprintf(mString, sizeof(mString), text, arg);
    snprintf(mString+strlen(mString), sizeof(mString)-strlen(mString), ": %s", strerror(errno));
    va_end(arg);
}

const char *error::text()
{
    return mString;
}
