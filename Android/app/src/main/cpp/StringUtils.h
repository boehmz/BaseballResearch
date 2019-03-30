#ifndef STRINGUTILS_H_INCLUDED
#define STRINGUTILS_H_INCLUDED

#include <string>
#include <vector>
#include "SharedGlobals.h"

#define itoa _itoa_osx

#ifdef ANDROID
	#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "MLBPredictor", __VA_ARGS__))
#else
	#define LOGI(...) printf(__VA_ARGS__)
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
void ReplaceInStringIfExists(std::string& originalString, const std::string& removeString, const std::string& replaceString);
std::vector<std::string> MultineRegex(std::string multiLineString, std::string regexExpression);
void EraseInString(std::string& originalString, const std::string& eraseString);
std::vector<std::string> SplitStringIntoMultiple(std::string wholeString, std::string tokens, std::string removeFromIndividual = "");
size_t FindPlayerNameIndexInList(const std::string& playerName, const std::string& searchText);
int GetNumDaysInMonth(int monthInteger);
std::string getJsonValueFromKey(std::string wholeJson, std::string key, size_t startingSearchIndex = 0);
std::string ConvertStandardTeamCodeToRotoGuruTeamCode(std::string standardCode);
std::string ConvertRotoGuruTeamCodeToStandardTeamCode(std::string rotoGuruCode);
std::string convertTeamCodeToSynonym(std::string teamCode, int codeIndex);

bool StringStartsWith(std::string mainString, std::string beginning);

void CurlGetSiteContents(CURL* curl, std::string readURL, std::string& writeBuffer, bool allowRedirects = false);
#endif
