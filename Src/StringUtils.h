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

std::string GetPlatformCompatibleFileNameFromRelativePath(std::string relativeFileName);
void _itoa_osx(int value, char* result, int base);

bool FileExists(const char* fileName);
void CutAndPasteFile(const char* src, const char* dest);
int CurrentYearAsInt();
std::string ConvertFLNameToLFName(std::string firstLast);
std::string ConvertLFNameToFLName(std::string lastFirst);
std::string ConvertNameToFirstInitialLastName(std::string name);
std::string IntToDateYMD(int date, int daysBeforeOrAfter);
std::string GetDateBeforeOrAfterNumDays(std::string date, int daysBeforeOrAfter);
std::string DateToDateWithDashes(std::string date);
std::string ConvertSpecialCharactersToEnglish26(std::string specialCharactersString);
void RemoveJavaScriptBlocksFromFileText(std::string& fileText);
void RemoveAllSectionsWithKeyword(std::string& wholeString, const std::string& keyword, const std::string& sectionBegin, const std::string& sectionEnd);
std::string GetSubStringBetweenStrings(const std::string& wholeString, const std::string& leftString, const std::string& rightString);
void CutStringToOnlySectionBetweenKeywords(std::string& wholeString, const std::string& sectionBegin, const std::string& sectionEnd);
std::string ExtractStringToBeOnlySectionBetweenKeywords(const std::string& wholeString, const std::string& sectionBegin, const std::string& sectionEnd);

std::vector<std::string> SplitStringIntoMultiple(std::string wholeString, std::string tokens, std::string removeFromIndividual = "");

int GetNumDaysInMonth(int monthInteger);

void CurlGetSiteContents(CURL* curl, std::string readURL, std::string& writeBuffer, bool allowRedirects = false);
#endif
