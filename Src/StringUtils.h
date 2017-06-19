#ifndef STRINGUTILS_H_INCLUDED
#define STRINGUTILS_H_INCLUDED

#include <string>
#include <vector>

std::string ConvertFLNameToLFName(std::string firstLast);
std::string ConvertLFNameToFLName(std::string lastFirst);
std::string IntToDateYMD(int date, bool roundup = false, std::string yearString = "2017");

std::vector<std::string> SplitStringIntoMultiple(std::string wholeString, std::string tokens);

void CurlGetSiteContents(CURL* curl, std::string readURL, std::string& writeBuffer);
#endif