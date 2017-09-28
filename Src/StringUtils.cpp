#include <stdio.h>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <assert.h>
#include "StringUtils.h"
#include "StatsCollectionFunctions.h"

using namespace std;

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

std::string IntToDateYMD(int date, bool roundUp, string yearString)
{
	int monthInteger = (date / 100);
	int isolatedDay = date - (monthInteger * 100);
	if (isolatedDay == 0)
	{
		monthInteger--;
		date -= 100;
		switch (monthInteger)
		{
		case 4:
		case 6:
		case 9:
			isolatedDay = 30;
			break;
		default:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
			isolatedDay = 31;
			break;
		}
		date = monthInteger * 100 + isolatedDay;
	}

	char thisDateCStr[5];
	_itoa_s(date, thisDateCStr, 10);
	string thisDate = thisDateCStr;

	string dateFormatted = yearString;
	if (date < 1000)
		dateFormatted += "0";
	dateFormatted += thisDate;
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