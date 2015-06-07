//
// Created by Tony Hoyle on 30/05/15.
//

#ifndef SDR_ERROR_H
#define SDR_ERROR_H

#include <string>

class error
{
private:
    char mString[1024];

public:
    error(const char *text, ...);

    const char *text();
};

#endif //SDR_ERROR_H
