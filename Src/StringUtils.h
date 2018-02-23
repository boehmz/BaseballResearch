#ifndef STRINGUTILS_H_INCLUDED
#define STRINGUTILS_H_INCLUDED

#include <string>
#include <vector>
#include "SharedGlobals.h"

#if PLATFORM_OSX
#define itoa _itoa_osx
#else
#define itoa _itoa_s
#endif

void _itoa_osx(int value, char* result, int base);

std::string ConvertFLNameToLFName(std::string firstLast);
std::string ConvertLFNameToFLName(std::string lastFirst);
std::string IntToDateYMD(int date, bool roundup = false, std::string yearString = CURRENT_YEAR);

std::vector<std::string> SplitStringIntoMultiple(std::string wholeString, std::string tokens, std::string removeFromIndividual = "");

void CurlGetSiteContents(CURL* curl, std::string readURL, std::string& writeBuffer);
#endif
