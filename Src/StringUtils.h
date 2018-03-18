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

int CurrentYearAsInt();
std::string ConvertFLNameToLFName(std::string firstLast);
std::string ConvertLFNameToFLName(std::string lastFirst);
std::string IntToDateYMD(int date, std::string yearString = CURRENT_YEAR, bool roundUp = false);
std::string DateToDateWithDashes(std::string date);
void RemoveJavaScriptBlocksFromFileText(std::string& fileText);

std::vector<std::string> SplitStringIntoMultiple(std::string wholeString, std::string tokens, std::string removeFromIndividual = "");

int GetNumDaysInMonth(int monthInteger);

void CurlGetSiteContents(CURL* curl, std::string readURL, std::string& writeBuffer);
#endif
