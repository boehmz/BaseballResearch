#include <stdio.h>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include "StatsCollectionFunctions.h"
#include "SharedGlobals.h"
using namespace std;

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	if (userp != NULL)
		((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

string GetEntireFileContents(string fileName)
{
	ifstream readFromFile(fileName);
	if (!readFromFile.good())
		return "";
	stringstream sstr;
	sstr << readFromFile.rdbuf();
	return sstr.str();
}

string ReplaceURLWhiteSpaces(string originalURL)
{
	size_t urlWhiteSpaceIndex = originalURL.find(" ", 0);
	while (urlWhiteSpaceIndex != string::npos)
	{
		originalURL.erase(urlWhiteSpaceIndex, 1);
		originalURL.insert(urlWhiteSpaceIndex, "%20");
		urlWhiteSpaceIndex = originalURL.find(" ", urlWhiteSpaceIndex);
	}
	return originalURL;
}

float CalculateRSquared(vector<float> finalInputs, vector<float> outputValues)
{
	if (finalInputs.size() != outputValues.size())
		return -2;

	float inputAverage = 0;
	for (unsigned int i = 0; i < finalInputs.size(); ++i)
	{
		inputAverage += finalInputs[i];
	}
	inputAverage /= (float)finalInputs.size();

	float outputAverage = 0;
	for (unsigned int i = 0; i < outputValues.size(); ++i)
	{
		outputAverage += outputValues[i];
	}
	outputAverage /= (float)outputValues.size();
	float inputOutputAverage = 0;
	for (unsigned int i = 0; i < finalInputs.size(); ++i)
	{
		inputOutputAverage += finalInputs[i] * outputValues[i];
	}
	inputOutputAverage /= (float)finalInputs.size();
	float inputSquaredAverage = 0;
	for (unsigned int i = 0; i < finalInputs.size(); ++i)
	{
		inputSquaredAverage += finalInputs[i] * finalInputs[i];
	}
	inputSquaredAverage /= (float)finalInputs.size();

	float m = (inputAverage * outputAverage - inputOutputAverage) / (inputAverage * inputAverage - inputSquaredAverage);
	float b = outputAverage - m * inputAverage;
	float ssRes = 0;
	for (unsigned int i = 0; i < finalInputs.size(); ++i)
	{
		float estimatedOutput = m * finalInputs[i] + b;
		ssRes += (outputValues[i] - estimatedOutput) * (outputValues[i] - estimatedOutput);
	}
	float ssTot = 0;
	for (unsigned int i = 0; i < outputValues.size(); ++i)
	{
		ssTot += (outputValues[i] - outputAverage) * (outputValues[i] - outputAverage);
	}
	float rSquared = 1.0f - ssRes / ssTot;
	return rSquared;
}

string GetPlayerStatsRawString(string playerId, string yearString, CURL *curl)
{
	string playerStatsLookupBuffer = "";
	if (yearString == "2016")
		playerStatsLookupBuffer	= GetEntireFileContents("Player2016DataCached\\PlayerId" + playerId + ".txt");
	else 
		playerStatsLookupBuffer = GetEntireFileContents("Player2017DataCached\\PlayerId" + playerId + ".txt");
	
	if (yearString == "2017")
	{
		size_t dateMetaDataIndex = playerStatsLookupBuffer.find("/ZachDateMetaData", 0);
		if (dateMetaDataIndex == string::npos || playerStatsLookupBuffer.substr(0, dateMetaDataIndex) < todaysDate)
			playerStatsLookupBuffer = "";
	}

	if (playerStatsLookupBuffer == "")
	{
		if (curl == NULL)
			curl = curl_easy_init();

		string playerStatsLookupURL = "http://rotoguru1.com/cgi-bin/player.cgi?" + playerId + "x";
		if (yearString == "2016")
			playerStatsLookupURL = "http://rotoguru1.com/cgi-bin/player16.cgi?" + playerId + "x";

		curl_easy_setopt(curl, CURLOPT_URL, playerStatsLookupURL.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &playerStatsLookupBuffer);
		curl_easy_perform(curl);
		curl_easy_reset(curl);

		size_t writeToFileIndex = playerStatsLookupBuffer.find("Daily results", 0);
		if (writeToFileIndex == string::npos)
			writeToFileIndex = playerStatsLookupBuffer.find("No data found", 0);
		ofstream writeToFile;
		if (yearString == "2016")
			writeToFile.open("Player2016DataCached\\PlayerId" + playerId + ".txt");
		else
		{
			writeToFile.open("Player2017DataCached\\PlayerId" + playerId + ".txt");
			writeToFile << todaysDate << "/ZachDateMetaData" << endl;
		}
		writeToFile << playerStatsLookupBuffer.substr(0, writeToFileIndex);
		writeToFile.close();
		
	}
	return playerStatsLookupBuffer;
}

FullSeasonStats GetBatterStats(string playerId, string yearString, CURL *curl)
{
	string playerStatsLookupBuffer = GetPlayerStatsRawString(playerId, yearString, curl);
	FullSeasonStats playerStats;
	playerStats.averagePpg = playerStats.averagePpgVsLefty = playerStats.averagePpgVsRighty = 0;
	playerStats.totalGamesStarted = 0;

	size_t ppgIndex = playerStatsLookupBuffer.find("full season averages", 0);
	if (ppgIndex != string::npos)
	{
		ppgIndex = playerStatsLookupBuffer.find("(R)", ppgIndex);
		size_t ppgStartIndex = playerStatsLookupBuffer.find_last_of(" ", ppgIndex);
		playerStats.averagePpgVsRighty = stof(playerStatsLookupBuffer.substr(ppgStartIndex + 1, ppgIndex - ppgStartIndex - 1).c_str());

		ppgIndex = playerStatsLookupBuffer.find("(L)", ppgIndex);
		ppgStartIndex = playerStatsLookupBuffer.find_last_of(" ", ppgIndex);
		playerStats.averagePpgVsLefty = stof(playerStatsLookupBuffer.substr(ppgStartIndex + 1, ppgIndex - ppgStartIndex - 1).c_str());

		ppgIndex = playerStatsLookupBuffer.find("(starting)", 0);
		ppgIndex = playerStatsLookupBuffer.find_last_of(" ", ppgIndex);
		ppgStartIndex = playerStatsLookupBuffer.find_last_of(" ", ppgIndex - 1);
		playerStats.averagePpg = stof(playerStatsLookupBuffer.substr(ppgStartIndex + 1, ppgIndex - ppgStartIndex - 1).c_str());

		size_t gamesStartedIndex = playerStatsLookupBuffer.find("started:", 0);
		if (gamesStartedIndex != string::npos)
		{
			size_t gamesStartedLeftIndex = playerStatsLookupBuffer.rfind(">", gamesStartedIndex);
			string gamesStartedString = playerStatsLookupBuffer.substr(gamesStartedLeftIndex + 1, gamesStartedIndex - gamesStartedLeftIndex - 2);
			playerStats.totalGamesStarted = atoi(gamesStartedString.c_str());
		}
	}

	return playerStats;
}

std::vector<string> GetFangraphsRowColumns(std::string yearRow, std::string allData, int numColumns, std::string section, std::string nextSection)
{
	vector<string> allColumns;
	size_t fangraphsCurrentIndex = allData.find(section, 0);
	size_t fangraphsNextCategoryIndex = allData.find(nextSection, 0);
	if (fangraphsCurrentIndex != string::npos)
	{
		fangraphsCurrentIndex = allData.find(yearRow, fangraphsCurrentIndex + 1);
		
		if (fangraphsCurrentIndex != string::npos && fangraphsCurrentIndex < fangraphsNextCategoryIndex)
		{
			fangraphsCurrentIndex = allData.find("</td>", fangraphsCurrentIndex + 1);
			while (numColumns > 0)
			{
				fangraphsCurrentIndex = allData.find("</td>", fangraphsCurrentIndex + 1);
				size_t fangraphsPrevIndex = allData.rfind(">", fangraphsCurrentIndex);
				allColumns.push_back(allData.substr(fangraphsPrevIndex + 1, fangraphsCurrentIndex - fangraphsPrevIndex - 1));
				numColumns--;
			}
		}
	}
	return allColumns;
}

FullSeasonPitcherStats GetPitcherStats(string playerId, string yearString, CURL *curl)
{
	FullSeasonPitcherStats pitcherStats;

	string fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != "2017", AdvancedStatsPitchingStarterStatsOnly);
	vector<string> fangraphsStandardRows = GetFangraphsRowColumns(yearString, fangraphsPlayerData, 18, "name=\"standard\"", "name=\"advanced\"");
	if (fangraphsStandardRows.size() == 0)
		return pitcherStats;
	pitcherStats.numInnings = stof(fangraphsStandardRows[1].c_str());
	pitcherStats.numInnings = floor(pitcherStats.numInnings) + ((pitcherStats.numInnings - floor(pitcherStats.numInnings)) * 3.4f);
	pitcherStats.era = stof(fangraphsStandardRows[2].c_str());
	pitcherStats.opsAllowed = stof(fangraphsStandardRows[15].c_str());
	pitcherStats.opsAllowed += stof(fangraphsStandardRows[16].c_str());
	pitcherStats.wobaAllowed = stof(fangraphsStandardRows[17].c_str());

	vector<string> fangraphsAdvancedRows = GetFangraphsRowColumns(yearString, fangraphsPlayerData, 14, "name=\"advanced\"", "name=\"battedball\"");
	pitcherStats.strikeOutsPer9 = stof(fangraphsAdvancedRows[1].c_str());
	pitcherStats.whip = stof(fangraphsAdvancedRows[9].c_str());
	pitcherStats.fip = stof(fangraphsAdvancedRows[12].c_str());
	pitcherStats.xfip = stof(fangraphsAdvancedRows[13].c_str());

	return pitcherStats;
}

FullSeasonStatsAdvanced GetPitcherAdvancedStats(string playerId, string yearString, CURL *curl)
{
	FullSeasonStatsAdvanced pitcherAdvancedStats;

	string fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != "2017", AdvancedStatsPitchingSplitsVersusLeftHand);
	size_t fangraphsCurrentIndex = fangraphsPlayerData.find("name=\"standard\"", 0);
	if (fangraphsCurrentIndex != string::npos)
	{
		fangraphsCurrentIndex = fangraphsPlayerData.find(">" + yearString + "<", fangraphsCurrentIndex + 1);

		if (fangraphsCurrentIndex != string::npos)
		{
			pitcherAdvancedStats.opsVersusLefty = 0;

			for (int i = 0; i < 16; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			size_t fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			pitcherAdvancedStats.averageVersusLefty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());

			for (int i = 0; i < 2; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			pitcherAdvancedStats.opsVersusLefty += stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());

			for (int i = 0; i < 2; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			float slugging = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());
			pitcherAdvancedStats.opsVersusLefty += slugging;
			pitcherAdvancedStats.sluggingVersusLefty = slugging;
			pitcherAdvancedStats.isoVersusLefty = slugging - pitcherAdvancedStats.averageVersusLefty;

			for (int i = 0; i < 2; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			pitcherAdvancedStats.wobaVersusLefty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());
		}
	}

	fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != "2017", AdvancedStatsPitchingSplitsVersusRightHand);
	fangraphsCurrentIndex = fangraphsPlayerData.find("name=\"standard\"", 0);
	if (fangraphsCurrentIndex != string::npos)
	{
		fangraphsCurrentIndex = fangraphsPlayerData.find(">" + yearString + "<", fangraphsCurrentIndex + 1);

		if (fangraphsCurrentIndex != string::npos)
		{
			pitcherAdvancedStats.opsVersusRighty = 0;

			for (int i = 0; i < 16; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			size_t fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			pitcherAdvancedStats.averageVersusRighty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());

			for (int i = 0; i < 2; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			pitcherAdvancedStats.opsVersusRighty += stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());

			for (int i = 0; i < 2; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			float slugging = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());
			pitcherAdvancedStats.opsVersusRighty += slugging;
			pitcherAdvancedStats.sluggingVersusRighty = slugging;
			pitcherAdvancedStats.isoVersusRighty = slugging - pitcherAdvancedStats.averageVersusRighty;

			for (int i = 0; i < 2; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			pitcherAdvancedStats.wobaVersusRighty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());
		}
	}

	return pitcherAdvancedStats;
}

FullSeasonStatsAdvanced GetBatterAdvancedStats(string playerId, string yearString, CURL *curl)
{
	FullSeasonStatsAdvanced batterAdvancedStats;

	string fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != "2017", AdvancedStatsBattingSplitsVersusLeftHand);
	size_t fangraphsCurrentIndex = fangraphsPlayerData.find("name=\"standard\"", 0);
	if (fangraphsCurrentIndex != string::npos)
	{
		fangraphsCurrentIndex = fangraphsPlayerData.find(">" + yearString + "<", fangraphsCurrentIndex + 1);

		if (fangraphsCurrentIndex != string::npos)
		{
			for (int i = 0; i < 22; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			size_t fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			batterAdvancedStats.averageVersusLefty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());
		}
	}

	fangraphsCurrentIndex = fangraphsPlayerData.find("name=\"advanced\"", 0);
	if (fangraphsCurrentIndex != string::npos)
	{
		fangraphsCurrentIndex = fangraphsPlayerData.find(">" + yearString + "<", fangraphsCurrentIndex + 1);

		if (fangraphsCurrentIndex != string::npos)
		{
			for (int i = 0; i < 8; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			size_t fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			batterAdvancedStats.sluggingVersusLefty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());
			batterAdvancedStats.isoVersusLefty = batterAdvancedStats.sluggingVersusLefty - batterAdvancedStats.averageVersusLefty;

			for (int i = 0; i < 2; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			batterAdvancedStats.opsVersusLefty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());

			for (int i = 0; i < 6; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			batterAdvancedStats.wobaVersusLefty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());
		}
	}

	fangraphsPlayerData = GetPlayerFangraphsPageData(playerId, curl, yearString != "2017", AdvancedStatsBattingSplitsVersusRightHand);
	fangraphsCurrentIndex = fangraphsPlayerData.find("name=\"standard\"", 0);
	if (fangraphsCurrentIndex != string::npos)
	{
		fangraphsCurrentIndex = fangraphsPlayerData.find(">" + yearString + "<", fangraphsCurrentIndex + 1);

		if (fangraphsCurrentIndex != string::npos)
		{
			for (int i = 0; i < 22; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			size_t fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			batterAdvancedStats.averageVersusRighty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());
		}
	}
	fangraphsCurrentIndex = fangraphsPlayerData.find("name=\"advanced\"", 0);
	if (fangraphsCurrentIndex != string::npos)
	{
		fangraphsCurrentIndex = fangraphsPlayerData.find(">" + yearString + "<", fangraphsCurrentIndex + 1);

		if (fangraphsCurrentIndex != string::npos)
		{
			for (int i = 0; i < 8; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			size_t fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			batterAdvancedStats.sluggingVersusRighty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());
			batterAdvancedStats.isoVersusRighty = batterAdvancedStats.sluggingVersusRighty - batterAdvancedStats.averageVersusRighty;

			for (int i = 0; i < 2; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			batterAdvancedStats.opsVersusRighty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());

			for (int i = 0; i < 6; ++i)
			{
				fangraphsCurrentIndex = fangraphsPlayerData.find("</td>", fangraphsCurrentIndex + 1);
			}
			fangraphsNextIndex = fangraphsCurrentIndex;
			fangraphsCurrentIndex = fangraphsPlayerData.rfind(">", fangraphsNextIndex);
			batterAdvancedStats.wobaVersusRighty = stof(fangraphsPlayerData.substr(fangraphsCurrentIndex + 1, fangraphsNextIndex - fangraphsCurrentIndex - 1).c_str());
		}
	}

	return batterAdvancedStats;
}

string GetPlayerFangraphsPageData(string playerId, CURL *curl, bool bCachedOk, int advancedStatsFlags)
{
	string fangraphsData = "";
	string cachedFileName = "FangraphsCachedPages\\PlayerId" + playerId + ".txt";
	if ((advancedStatsFlags & AdvancedStatsBattingSplitsVersusLeftHand) ||
		(advancedStatsFlags & AdvancedStatsPitchingSplitsVersusLeftHand))
		cachedFileName = "FangraphsCachedPages\\PlayerId" + playerId + "VsLeft.txt";
	else if ((advancedStatsFlags & AdvancedStatsBattingSplitsVersusRightHand) ||
		(advancedStatsFlags & AdvancedStatsPitchingSplitsVersusRightHand))
		cachedFileName = "FangraphsCachedPages\\PlayerId" + playerId + "VsRight.txt";

	fangraphsData = GetEntireFileContents(cachedFileName);

	if (!bCachedOk)
	{
		size_t dateMetaDataIndex = fangraphsData.find("/ZachDateMetaData", 0);
		if (dateMetaDataIndex == string::npos || fangraphsData.substr(0, dateMetaDataIndex) < todaysDate)
			fangraphsData = "";
	}

	if (fangraphsData == "")
	{
		if (curl == NULL)
			curl = curl_easy_init();

		string playerRotoGuruData = GetPlayerStatsRawString(playerId, "any", curl);

		size_t fangraphsURLIndexStart = playerRotoGuruData.find("www.fangraphs.com", 0);
		fangraphsURLIndexStart = playerRotoGuruData.rfind("\"", fangraphsURLIndexStart);
		size_t fangraphsURLIndexEnd = playerRotoGuruData.find("\" ", fangraphsURLIndexStart + 1);
		string fangraphsURL = playerRotoGuruData.substr(fangraphsURLIndexStart + 1, fangraphsURLIndexEnd - fangraphsURLIndexStart - 1);
		fangraphsURL = ReplaceURLWhiteSpaces(fangraphsURL);

		curl_easy_setopt(curl, CURLOPT_URL, fangraphsURL.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		if (advancedStatsFlags == 0)
		{
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fangraphsData);
		}
		else
		{
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
		}
		curl_easy_perform(curl);
		char* finalFangraphsUrlCStr;
		curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &finalFangraphsUrlCStr);
		string finalFangraphsURL = finalFangraphsUrlCStr;
		curl_easy_reset(curl);

		if (advancedStatsFlags != 0)
		{
			if (advancedStatsFlags & AdvancedStatsPitchingStarterStatsOnly)
			{
				size_t statsIndex = finalFangraphsURL.find("statss.aspx", 0);
				finalFangraphsURL.erase(statsIndex + 5, 1);
				finalFangraphsURL.insert(statsIndex + 5, "plits");
				finalFangraphsURL += "&season=0";
				finalFangraphsURL += "&split=8.1";
				curl_easy_setopt(curl, CURLOPT_URL, finalFangraphsURL.c_str());
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fangraphsData);
				curl_easy_perform(curl);
				curl_easy_reset(curl);
			}
			else if ((advancedStatsFlags & AdvancedStatsPitchingSplitsVersusLeftHand)
				|| (advancedStatsFlags & AdvancedStatsPitchingSplitsVersusRightHand))
			{
				size_t statsIndex = finalFangraphsURL.find("statss.aspx", 0);
				finalFangraphsURL.erase(statsIndex + 5, 1);
				finalFangraphsURL.insert(statsIndex + 5, "plits");
				finalFangraphsURL += "&season=0";
				if (advancedStatsFlags & AdvancedStatsPitchingSplitsVersusLeftHand)
					finalFangraphsURL += "&split=0.1";
				else
					finalFangraphsURL += "&split=0.2";
				curl_easy_setopt(curl, CURLOPT_URL, finalFangraphsURL.c_str());
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fangraphsData);
				curl_easy_perform(curl);
				curl_easy_reset(curl);
			}
			else if ((advancedStatsFlags & AdvancedStatsBattingSplitsVersusLeftHand)
				|| (advancedStatsFlags & AdvancedStatsBattingSplitsVersusRightHand))
			{
				size_t statsIndex = finalFangraphsURL.find("statss.aspx", 0);
				finalFangraphsURL.erase(statsIndex + 5, 1);
				finalFangraphsURL.insert(statsIndex + 5, "plits");
				finalFangraphsURL += "&season=0";
				if (advancedStatsFlags & AdvancedStatsBattingSplitsVersusLeftHand)
					finalFangraphsURL += "&split=0.5";
				else
					finalFangraphsURL += "&split=0.6";
				curl_easy_setopt(curl, CURLOPT_URL, finalFangraphsURL.c_str());
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fangraphsData);
				curl_easy_perform(curl);
				curl_easy_reset(curl);
			}
		}

		size_t writeToFileIndexBegin = fangraphsData.find("text/javascript", 0);
		if (writeToFileIndexBegin == string::npos)
			writeToFileIndexBegin = 0;
		size_t writeToFileLength = fangraphsData.find("BEGIN FIRSTIMPRESSION TAG", writeToFileIndexBegin);
		if (writeToFileLength != string::npos)
			writeToFileLength -= writeToFileIndexBegin;
		ofstream writeToFile;
		writeToFile.open(cachedFileName);
		writeToFile << todaysDate << "/ZachDateMetaData" << endl;
		if ((advancedStatsFlags & AdvancedStatsBattingSplitsVersusLeftHand) ||
			(advancedStatsFlags & AdvancedStatsPitchingSplitsVersusLeftHand))
			writeToFile << fangraphsData;
		else if ((advancedStatsFlags & AdvancedStatsBattingSplitsVersusRightHand) ||
			(advancedStatsFlags & AdvancedStatsPitchingSplitsVersusRightHand))
			writeToFile << fangraphsData;
		else
			writeToFile << fangraphsData.substr(writeToFileIndexBegin, writeToFileLength);
		writeToFile.close();
	}

	return fangraphsData;
}

string FullSeasonPitcherStats::ToString()
{
	//era; fip; xfip; strikeOutsPer9; numInnings; whip; wobaAllowed; opsAllowed;
	return to_string(era) + ";" + to_string(fip) + ";" + to_string(xfip) + ";" + to_string(strikeOutsPer9) + ";" + to_string(numInnings) + ";" + to_string(whip) + ";" + to_string(wobaAllowed) + ";" + to_string(opsAllowed) + ";";
}

bool FullSeasonPitcherStats::operator==(const FullSeasonPitcherStats& rhs)
{
	if (abs(rhs.era - era) > 0.0015f)
		return false;
	if (abs(rhs.fip - fip) > 0.0015f)
		return false;
	if (abs(rhs.xfip - xfip) > 0.0015f)
		return false;
	if (abs(rhs.numInnings - numInnings) > 0.015f)
		return false;
	if (abs(rhs.strikeOutsPer9 - strikeOutsPer9) > 0.015f)
		return false;
	if (abs(rhs.whip - whip) > 0.015f)
		return false;
	if (abs(rhs.opsAllowed - opsAllowed) > 0.0015f)
		return false;
	if (abs(rhs.wobaAllowed - wobaAllowed) > 0.0015f)
		return false;
	return true;
}

bool FullSeasonStatsAdvanced::operator==(const FullSeasonStatsAdvanced& rhs)
{
	if (abs(rhs.averageVersusLefty - averageVersusLefty) > 0.0015f)
		return false;
	if (abs(rhs.averageVersusRighty - averageVersusRighty) > 0.0015f)
		return false;
	if (abs(rhs.isoVersusLefty - isoVersusLefty) > 0.0015f)
		return false;
	if (abs(rhs.isoVersusRighty - isoVersusRighty) > 0.0015f)
		return false;
	if (abs(rhs.opsVersusLefty - opsVersusLefty) > 0.0015f)
		return false;
	if (abs(rhs.opsVersusRighty - opsVersusRighty) > 0.0015f)
		return false;
	if (abs(rhs.sluggingVersusLefty - sluggingVersusLefty) > 0.0015f)
		return false;
	if (abs(rhs.sluggingVersusRighty - sluggingVersusRighty) > 0.0015f)
		return false;
	if (abs(rhs.wobaVersusLefty - wobaVersusLefty) > 0.0015f)
		return false;
	if (abs(rhs.wobaVersusRighty - wobaVersusRighty) > 0.0015f)
		return false;
	return true;
}

bool FullSeasonStats::operator==(const FullSeasonStats& rhs)
{
	if (abs(rhs.averagePpg - averagePpg) > 0.15f)
		return false;
	if (abs(rhs.averagePpgVsLefty - averagePpgVsLefty) > 0.15f)
		return false;
	if (abs(rhs.averagePpgVsRighty - averagePpgVsRighty) > 0.0015f)
		return false;
	if (abs(rhs.totalGamesStarted - totalGamesStarted) > 0.5f)
		return false;
	return true;
}

/*
http://rotoguru1.com/cgi-bin/stats.cgi?pos=6&sort=6&game=d&colA=0&daypt=0&denom=3&xavg=3&inact=0&maxprc=99999&sched=1&starters=1&hithand=1&numlist=c
0    1    2               3     4         5             6      7    8     9        10             11       12           13      14      15      16        17     18    19       20    21     22          23
GID; Pos; Name;           Team; Salary; Salary Change; Points; GS;  GP; Active; Pts / Game; Pts / G / $; Pts / G(alt); Last; Days ago; MLBID;  ESPNID; YahooID; Bats; Throws; H / A; Oppt; Oppt hand; Game title
5125; 3; Cabrera, Miguel; det;  4000;      0;             0;     0; 0;    1;     0;           0;             0;         0;      0;     408234; 5544;   7163;     R;      R;     A;    chw;    L;       Jose Quintana(L) chw vs.det - 4:10 PM EDT - U.S.Cellular Field



http://dailybaseballdata.com/cgi-bin/dailyhit.pl?date=&xyear=0&pa=1&showdfs=&sort=woba&r40=0&scsv=0
http://dailybaseballdata.com/cgi-bin/dailyhit.pl?date=&xyear=0&pa=1&showdfs=&sort=woba&r40=0&scsv=1&nohead=1
pitcher vs batter matchups
MLB_ID;  ESPN_ID;  Name(LF);          Name(FL);         Team;  H/A;  Bats;  Active;  FD_pos;  DK_pos;  DD_pos;  YH_pos;  FD_sal;  DK_sal;  DD_sal;  YH_sal;  NP;  PA;  AB;  Hits;  2B;  3B;  HR;  Runs;  RBI;  BB;  IBB;  SO;  HBP;  SB;  CS;  AVG;   OBP;   SLG;   OPS;     wOBA;  MLB_ID(p);  ESPN_ID(p);  Pitcher_name(LF);  Pitcher_name(FL);  P_Team;  Throws;  game_time;    Stadium;        FD_sal;  DK_sal;  DD_sal;  YH_sal
453056;  28637;    Ellsbury, Jacoby;  Jacoby Ellsbury;  nyy;   H;    L;     1;       7;       7;       7;       7;        ;       ;        ;        ;        16;  4;   4;   2;     0;   0;   0;   0;     1;    0;   0;    2;   0;    0;   1;   .500;  .500;  .500;  1.000;  .450;   502009;     30196;       Latos, Mat;        Mat Latos;         tor;     R;       7:05 PM EDT;  Yankee Stadium; ;        ;        ;


http://rotoguru1.com/cgi-bin/byday.pl?date=404&game=fd&scsv=1
http://rotoguru1.com/cgi-bin/byday.pl?date=404&game=fd&scsv=1&nowrap=1
recap of stats earned
0           1         2       3                  4           5         6          7      8       9      10     11       12         13
Date;       GID;   MLB_ID;  Name;              Starter;  Bat order;  FD posn;  FD pts;  FD sal;  Team;  Oppt;  dblhdr;  Tm Runs;  Opp Runs
20170404;  1524;  434378;  Verlander, Justin;  1;        0;          ;         53.0;    ;         DET;  @ chw; ;        6;        3


http://rotoguru1.com/cgi-bin/player16.cgi?3124x
change 3124 with 4 digit player id
x (starting) is the 2016 ppg


http://dailybaseballdata.com/cgi-bin/weather.pl?scsv=1
weather


http://rotoguru1.com/cgi-bin/mlb-dbd-2016.pl?user=GoldenExcalibur&key=G5970032941
0     1        2                 3                 4     5      6         7      8      9      10                              11                 12            13           14           15            16     17          18        19          20      21          22   23   24    25      26                27              28                  29                30   31         32    33      34   35          36          37          38          39          40          41          42          43       44       45       46
GID:  MLB_ID:  Name_Last_First:  Name_First_Last:  P/H:  Hand:  Date:     Team:  Oppt:  H/A:  Game#(1 unless double header):  Game_ID:            Gametime_ET:  Team_score:  Oppt_score:  Home_Ump:     Temp:  Condition:  W_speed:  W_dir:      ADI:    prior_ADI:  GS:  GP:  Pos:  Order:  Oppt_pitch_hand:  Oppt_pich_GID:  Oppt_pitch_MLB_ID:  Oppt_pitch_Name:  PA:  wOBA_num:  IP:   W/L/S:  QS:  FD_points:  DK_points:  DD_points:  YH_points:  FD_salary:  DK_salary:  DD_salary:  YH_salary:  FD_pos:  DK_pos:  DD_pos:  YH_pos
2407: 547989:  Abreu, Jose:      Jose Abreu:       H:    R:     20161002: chw:   min:   h:    1:                              20161002-min-chw-1: 15.10:        3:           6:           Nic Lentz:    65:    cloudy:     6:        Out to LF:  65.90:  65.56:      1:   1:   1B:   4:      R:                136p:           621244:             Berrios, Jose:    4:   2.3:       :     :       :    12.5:       9.00:       19:         8:          3200:       3900:       8350:       19:         3:       3:       3:       3:
136r: 534947:  Adleman, Timothy: Tim Adleman:      P:    R:     20161001: cin:   chc:   h:    1:                              20161001-chc-cin-1: 16.10:        7:           4:           Tom Hallion:  65:    overcast:   8:        R to L:     65.05:  66.30:      1:   1:   P:    9:      L:                1506:           452657:             Lester, Jon:      2:   0.9:       5.0:  W:      1:   30:         12.45:      20:         13:         6100:       5500:       9000:       28:         1:       1:       1:       1:

*/