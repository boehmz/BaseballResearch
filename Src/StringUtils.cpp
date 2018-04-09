#include <stdio.h>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <assert.h>
#include <sys/stat.h>
//#include <unistd.h>
#include "StringUtils.h"
#include "StatsCollectionFunctions.h"

using namespace std;

bool FileExists(const char* fileName) {
    if (strlen(fileName) == 0)
        return false;
    struct stat buffer;
    return (stat (fileName, &buffer) == 0);
}

void CutAndPasteFile(const char* srcFileName, const char* destFileName) {
    if (FileExists(destFileName)) {
        cout << "WARNING: copying to existing file: " << destFileName << ".  Aborting." << endl;
        return;
    }
    if (strlen(srcFileName) > 0 && strlen(destFileName) > 0) {
        std::ifstream src(srcFileName, std::ios::binary);
        std::ofstream dest(destFileName, std::ios::binary);
        dest << src.rdbuf();
        remove(srcFileName);
    }
}

std::string GetPlatformCompatibleFileNameFromRelativePath(std::string relativeFileName) {
#if PLATFORM_OSX
    if (relativeFileName.find("/Users/boehmz/") != 0) {
        relativeFileName = "/Users/boehmz/zb/BaseballResearch/BaseballStatsBuilder/" + relativeFileName;
    }
    size_t folderPathIndex = relativeFileName.find("\\");
    while (folderPathIndex != string::npos) {
        relativeFileName = relativeFileName.replace(folderPathIndex, 1, "/");
        folderPathIndex = relativeFileName.find("\\");
    }
#endif
    return relativeFileName;
}
int CurrentYearAsInt() {
    return atoi(CURRENT_YEAR);
}
std::string ConvertFLNameToLFName(std::string firstLast)
{
	string convertedName = firstLast;
	size_t spaceIndex = firstLast.find(" ", 0);
	if (spaceIndex != string::npos)
	{
		convertedName = firstLast.substr(spaceIndex + 1);
		convertedName += ", ";
		convertedName += firstLast.substr(0, spaceIndex);
	}
	return convertedName;
}
std::string ConvertLFNameToFLName(std::string lastFirst)
{
	string convertedName = lastFirst;
	size_t commaIndex = lastFirst.find(", ", 0);
	if (commaIndex != string::npos)
	{
		convertedName = lastFirst.substr(commaIndex + 2);
		convertedName += " ";
		convertedName += lastFirst.substr(0, commaIndex);
	}
	return convertedName;
}

void _itoa_osx(int value, char* result, int base) {
    // check that the base if valid
    if (base < 2 || base > 36) { *result = '\0'; return; }
    
    char* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;
    
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );
    
    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
}

void RemoveJavaScriptBlocksFromFileText(std::string& fileText) {
	size_t javaScriptBlockIndexStart = fileText.find("script type=\"text/javascript\"");
	while (javaScriptBlockIndexStart != string::npos) {
		size_t javaScriptBlockIndexEnd = fileText.find("</script>", javaScriptBlockIndexStart);
		if (javaScriptBlockIndexEnd == string::npos)
			break;
		fileText.erase(javaScriptBlockIndexStart, javaScriptBlockIndexEnd - javaScriptBlockIndexStart + 9);
		javaScriptBlockIndexStart = fileText.find("script type=\"text/javascript\"");
	}
}

// date = "0808" or "20170808"
// output = "2017-08-08"
std::string DateToDateWithDashes(std::string date) {
	if (date.length() == 4) {
		date = "-" + date.substr(0, 2) + "-" + date.substr(2);
		date = CURRENT_YEAR + date;
	}
	else if (date.length() == 8) {
		date = date.substr(0, 4) + "-" + date.substr(4, 2) + "-" + date.substr(6, 2);
	}
	else if (date.length() != 10) {
		cout << "ERROR: Trying to format date with dashes, invalid input " << date << endl;
	}
	return date;
}

int GetNumDaysInMonth(int monthInteger) {
	switch (monthInteger)
	{
	case 2:
		cout << "Asking how many days in February" << endl;
		return 28;
	case 11:
		cout << "Asking how many days in November" << endl;
	case 4:
	case 6:
	case 9:
		return 30;
	case 12:
	case 1:
		cout << "Asking how many days in Jan or Dec" << endl;
	case 3:
	case 5:
	case 7:
	case 8:
	case 10:
		return 31;
	}
	cout << "ERROR: Invalid month" << monthInteger << "asked how many days are in" << endl;
	return -1;
}

std::string IntToDateYMD(int date, string yearString, bool roundUp)
{
	int yearInteger = date / 10000;
	int monthInteger = (date - yearInteger * 10000) / 100;
	int dayInteger = date - (yearInteger * 10000) - (monthInteger * 100);
	if (!roundUp && dayInteger > GetNumDaysInMonth(monthInteger)) {
		monthInteger++;
		dayInteger = -100 + dayInteger;
	}

	while (dayInteger <= 0)
	{
		monthInteger--;
		dayInteger = GetNumDaysInMonth(monthInteger) + dayInteger;
	}
	while (dayInteger > GetNumDaysInMonth(monthInteger)) {
		dayInteger -= GetNumDaysInMonth(monthInteger);
		monthInteger++;
		
	}

	date = yearInteger * 10000 + monthInteger * 100 + dayInteger;

	char thisDateCStr[9];
	itoa(date, thisDateCStr, 10);

	if (yearInteger > 1900)
		return thisDateCStr;

	string dateFormatted = yearString;
	if (date < 1000)
		dateFormatted += "0";
	dateFormatted += thisDateCStr;
	return dateFormatted;
}

void CurlGetSiteContents(CURL* curl, std::string readURL, std::string& writeBuffer)
{
	if (curl == NULL)
		curl = curl_easy_init();

	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, readURL.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeBuffer);
		curl_easy_perform(curl);
		curl_easy_reset(curl);
	}
}

std::vector<string> SplitStringIntoMultiple(std::string wholeString, std::string tokens, std::string removeFromIndividual)
{
	vector<string> stringArray;
	string singleString;
	size_t cur_token = 0, next_token;
	do
	{
		next_token = wholeString.find_first_of(tokens, cur_token);
		string individualString = wholeString.substr(cur_token, next_token - cur_token);
		if (removeFromIndividual != "") {
			size_t individualIndex = individualString.find(removeFromIndividual);
			while (individualIndex != string::npos) {
				individualString.erase(individualIndex, removeFromIndividual.length());
				individualIndex = individualString.find(removeFromIndividual);
			}
		}
		stringArray.push_back(individualString);
		if (next_token != string::npos)
			cur_token = next_token + 1;
	} while (next_token != string::npos);

	return stringArray;
}
