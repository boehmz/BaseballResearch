#include <android/log.h>
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
#include <regex>
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
    if (!FileExists(srcFileName)) {
        cout << "Could not cut file" << srcFileName << " because it does not exist." << endl;
        return;
    }
    if (FileExists(destFileName)) {
        cout << "WARNING: copying to existing file: " << destFileName << ".  Aborting." << endl;
        return;
    }
	if (strlen(srcFileName) > 0 && strlen(destFileName) > 0) {
		std::ifstream src(srcFileName, std::ios::binary);
		std::ofstream dest(destFileName, std::ios::binary);
		dest << src.rdbuf();
		src.close();
		dest.close();
		if (remove(srcFileName) != 0) {
			perror("Error deleting file that was being cut");
		}
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

std::string ConvertSpecialCharactersToEnglish26(std::string specialCharactersString) {
    vector< vector<string>> specialCharacterMappings(27);
    specialCharacterMappings[0].push_back("\xc3\xa0");
    specialCharacterMappings[0].push_back("a");
    specialCharacterMappings[1].push_back("\xc3\xa1");
    specialCharacterMappings[1].push_back("a");
    specialCharacterMappings[2].push_back("\xc3\xa2");
    specialCharacterMappings[2].push_back("a");
    specialCharacterMappings[3].push_back("\xc3\xa3");
    specialCharacterMappings[3].push_back("a");
    specialCharacterMappings[4].push_back("\xc3\xa4");
    specialCharacterMappings[4].push_back("a");
    specialCharacterMappings[5].push_back("\xc3\xa5");
    specialCharacterMappings[5].push_back("a");
    specialCharacterMappings[6].push_back("\xc3\xa7");
    specialCharacterMappings[6].push_back("c");
    specialCharacterMappings[7].push_back("\xc3\xa8");
    specialCharacterMappings[7].push_back("e");
    specialCharacterMappings[8].push_back("\xc3\xa9");
    specialCharacterMappings[8].push_back("e");
    specialCharacterMappings[9].push_back("\xc3\xaa");
    specialCharacterMappings[9].push_back("e");
    specialCharacterMappings[10].push_back("\xc3\xab");
    specialCharacterMappings[10].push_back("e");
    specialCharacterMappings[11].push_back("\xc3\xac");
    specialCharacterMappings[11].push_back("i");
    specialCharacterMappings[12].push_back("\xc3\xad");
    specialCharacterMappings[12].push_back("i");
    specialCharacterMappings[13].push_back("\xc3\xae");
    specialCharacterMappings[13].push_back("i");
    specialCharacterMappings[14].push_back("\xc3\xaf");
    specialCharacterMappings[14].push_back("i");
    specialCharacterMappings[15].push_back("\xc3\xb1");
    specialCharacterMappings[15].push_back("n");
    specialCharacterMappings[16].push_back("\xc3\xb2");
    specialCharacterMappings[16].push_back("o");
    specialCharacterMappings[17].push_back("\xc3\xb3");
    specialCharacterMappings[17].push_back("o");
    specialCharacterMappings[18].push_back("\xc3\xb4");
    specialCharacterMappings[18].push_back("o");
    specialCharacterMappings[19].push_back("\xc3\xb5");
    specialCharacterMappings[19].push_back("o");
    specialCharacterMappings[20].push_back("\xc3\xb6");
    specialCharacterMappings[20].push_back("o");
    specialCharacterMappings[21].push_back("\xc3\xb9");
    specialCharacterMappings[21].push_back("u");
    specialCharacterMappings[22].push_back("\xc3\xba");
    specialCharacterMappings[22].push_back("u");
    specialCharacterMappings[23].push_back("\xc3\xbb");
    specialCharacterMappings[23].push_back("u");
    specialCharacterMappings[24].push_back("\xc3\xbc");
    specialCharacterMappings[24].push_back("u");
    specialCharacterMappings[25].push_back("\xc3\xbd");
    specialCharacterMappings[25].push_back("y");
    specialCharacterMappings[26].push_back("\xc3\xbf");
    specialCharacterMappings[26].push_back("y");
    
    for (unsigned int i = 0; i < specialCharacterMappings.size(); ++i) {
        size_t specialCharacterIndex = specialCharactersString.find(specialCharacterMappings[i][0]);
        while (specialCharacterIndex != string::npos) {
            specialCharactersString.replace(specialCharacterIndex, 2, specialCharacterMappings[i][1]);
            specialCharacterIndex = specialCharactersString.find(specialCharacterMappings[i][0]);
        }
    }
    
    return specialCharactersString;
}

std::string ConvertNameToFirstInitialLastName(std::string name) {
    size_t commaIndex = name.find(",");
    if (commaIndex != string::npos) {
        name = ConvertLFNameToFLName(name);
    }
    size_t spaceIndex = name.find(" ");
    if (spaceIndex != string::npos) {
        name = name.substr(0, 1) + "." + name.substr(spaceIndex);
    }
    return name;
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

void ReplaceInStringIfExists(std::string& originalString, const std::string& removeString, const std::string& replaceString) {
	size_t removalIndex = originalString.find(removeString);
	if (removalIndex != string::npos) {
		originalString.replace(removalIndex, removeString.length(), replaceString);
	}
}

void EraseInString(std::string& originalString, const std::string& eraseString) {
	size_t removalIndex = originalString.find(eraseString);
	if (removalIndex != string::npos) {
		originalString.erase(removalIndex, eraseString.size());
	}
}

size_t FindPlayerNameIndexInList(const std::string& playerName, const std::string& searchText) {
	if (playerName.length() == 0 || searchText.length() == 0)
		return string::npos;
	size_t index = searchText.find(playerName);
	if (index == string::npos) {
		string tempFinder = playerName;
		if (playerName.find(",") != string::npos) {
			tempFinder = ConvertLFNameToFLName(playerName);
		} else {
			tempFinder = ConvertFLNameToLFName(playerName);
		}
		index = searchText.find(tempFinder);
		if (index == string::npos) {
			size_t dotIndex = tempFinder.find('.');
			while (dotIndex != string::npos) {
				tempFinder.erase(dotIndex, 1);
				dotIndex = tempFinder.find('.');
			}
			index = searchText.find(tempFinder);
			if (index == string::npos) {
				ReplaceInStringIfExists(tempFinder, "Kike", "Enrique");
				index = searchText.find(tempFinder);
				if (index == string::npos) {
					EraseInString(tempFinder, " Jr");
					index = searchText.find(tempFinder);
					if (index == string::npos) {
						ReplaceInStringIfExists(tempFinder, "Nick", "Nicholas");
						ReplaceInStringIfExists(tempFinder, "Mike", "Michael");
                        ReplaceInStringIfExists(tempFinder, "Steven", "Steve");
                        ReplaceInStringIfExists(tempFinder, "Norichika", "Nori");
                        ReplaceInStringIfExists(tempFinder, "Reymond", "Rey");
                        ReplaceInStringIfExists(tempFinder, "Hyun-Soo", "Hyun Soo");
                        ReplaceInStringIfExists(tempFinder, "Daniel", "Dan");
                        ReplaceInStringIfExists(tempFinder, "Javy", "Javier");
                        ReplaceInStringIfExists(tempFinder, "Chris", "Christopher");
                        ReplaceInStringIfExists(tempFinder, "Kris", "Kristopher");
                        ReplaceInStringIfExists(tempFinder, "Raul", "Adalberto");
                        ReplaceInStringIfExists(tempFinder, "Phillip", "Phil");

						EraseInString(tempFinder, "-hitter");
						index = searchText.find(tempFinder);
						if (index == string::npos) {
                            ReplaceInStringIfExists(tempFinder, "Nicholas", "Nick");
                            ReplaceInStringIfExists(tempFinder, "Michael", "Mike");
							ReplaceInStringIfExists(tempFinder, "Rafael", "Raffy");
							ReplaceInStringIfExists(tempFinder, "JR ", "John Ryan ");
                            ReplaceInStringIfExists(tempFinder, "Steve", "Steven");
                            ReplaceInStringIfExists(tempFinder, "Nori", "Norichika");
                            ReplaceInStringIfExists(tempFinder, "Rey", "Reymond");
                            ReplaceInStringIfExists(tempFinder, "Hyun Soo", "Hyun-Soo");
                            ReplaceInStringIfExists(tempFinder, "Dan", "Daniel");
                            ReplaceInStringIfExists(tempFinder, "Javier", "Javy");
                            ReplaceInStringIfExists(tempFinder, "Christopher", "Chris");
                            ReplaceInStringIfExists(tempFinder, "Kristopher", "Kris");
                            ReplaceInStringIfExists(tempFinder, "Adalberto", "Raul");
                            ReplaceInStringIfExists(tempFinder, "Phil", "Phillip");

							index = searchText.find(tempFinder);
                            if (index == string::npos) {
                                ReplaceInStringIfExists(tempFinder, "Jake", "Jakob");
                                ReplaceInStringIfExists(tempFinder, "Michael A.", "Michael");
                                ReplaceInStringIfExists(tempFinder, "Nicky", "Nick");

                                index = searchText.find(tempFinder);
                                 if (index == string::npos) {
                                     ReplaceInStringIfExists(tempFinder, "Michael", "Michael A.");
                                     ReplaceInStringIfExists(tempFinder, "Nick", "Nicky");
                                     ReplaceInStringIfExists(tempFinder, "Jakob", "Jacob");
                                     index = searchText.find(tempFinder);
                                     if (index == string::npos) {
                                         ReplaceInStringIfExists(tempFinder, "Jacob", "Jake");
                                         index = searchText.find(tempFinder);
                                         if (index == string::npos) {
                                             ReplaceInStringIfExists(tempFinder, "James", "J.A.");
                                             index = searchText.find(tempFinder);
                                             if (index == string::npos) {
                                                 ReplaceInStringIfExists(tempFinder, "J.A.", "James");
                                             }
                                         }
                                     }
                                 }
                            }
						}
					}
				}
			}
		}
	}
	return index;
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

std::string ExtractStringToBeOnlySectionBetweenKeywords(const std::string& wholeString, const std::string& sectionBegin, const std::string& sectionEnd) {
	string extracted = wholeString;
	CutStringToOnlySectionBetweenKeywords(extracted, sectionBegin, sectionEnd);
	return extracted;
}

void CutStringToOnlySectionBetweenKeywords(std::string& wholeString, const std::string& sectionBegin, const std::string& sectionEnd) {
	size_t beginIndex = wholeString.find(sectionBegin);
	if (beginIndex == string::npos)
		beginIndex = 0;
	size_t endIndex = wholeString.find(sectionEnd, beginIndex);
	if (endIndex != string::npos)
		endIndex += sectionEnd.length();
	wholeString = wholeString.substr(beginIndex, endIndex - beginIndex);
}

void RemoveAllSectionsWithKeyword(std::string& wholeString, const std::string& keyword, const std::string& sectionBegin, const std::string& sectionEnd) {
	size_t keywordIndex = wholeString.find(keyword);
	while (keywordIndex != string::npos) {
		size_t sectionStartIndex = keywordIndex;
		size_t sectionEndIndex = keywordIndex + keyword.length();
		if (sectionBegin.length() > 0)
			sectionStartIndex = wholeString.rfind(sectionBegin, keywordIndex);
		if (sectionEnd.length() > 0)
			sectionEndIndex = wholeString.find(sectionEnd, keywordIndex);
		if (sectionStartIndex == string::npos || sectionEndIndex == string::npos) {
			keywordIndex = wholeString.find(keyword, keywordIndex + 1);
		}
		else {
			wholeString.erase(sectionStartIndex, sectionEndIndex + sectionEnd.length() - sectionStartIndex);
			keywordIndex = wholeString.find(keyword);
		}
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

std::string GetDateBeforeOrAfterNumDays(std::string date, int daysBeforeOrAfter) {
    int dateInt = atoi(date.c_str());
    return IntToDateYMD(dateInt, daysBeforeOrAfter);
}

std::string IntToDateYMD(int date, int daysBeforeOrAfter)
{
	int yearInteger = date / 10000;
	int monthInteger = (date - yearInteger * 10000) / 100;
	int dayInteger = date - (yearInteger * 10000) - (monthInteger * 100);
    dayInteger += daysBeforeOrAfter;
    
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

    return thisDateCStr;
}

void CurlGetSiteContents(CURL* curl, std::string readURL, std::string& writeBuffer, bool allowRedirects)
{
	if (curl == NULL)
		curl = curl_easy_init();

	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, readURL.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        if (allowRedirects) {
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
        }
		curl_easy_perform(curl);
		curl_easy_reset(curl);
	}
}

std::string GetSubStringBetweenStrings(const std::string& wholeString, const std::string& leftString, const std::string& rightString) {
	string betweenString = "";
	if (wholeString != "") {
		size_t leftIndex = wholeString.find(leftString);
		leftIndex += leftString.length();
		size_t rightIndex = wholeString.find(rightString, leftIndex);
		if (leftIndex != string::npos && rightIndex != string::npos) {
			betweenString = wholeString.substr(leftIndex, rightIndex - leftIndex);
		} else {
			cout << "Right or left string not found, returning blank between string\n";
		}
	}
	return betweenString;
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

bool StringStartsWith(std::string mainString, std::string beginning) {
    size_t beginningLength = beginning.length();
    if (mainString.length() >= beginningLength) {
        for (int i = 0; i < beginningLength; ++i) {
            if (mainString[i] != beginning[i])
                return false;
        }
        return true;
    }
    return false;
}

std::vector<std::string> MultineRegex(std::string multiLineString, std::string regexExpression) {
    vector<string> captures;
    size_t lineIndexBegin = 0;
    std::regex regexRows(regexExpression);
    while (lineIndexBegin != string::npos) {
        size_t lineIndexEnd = multiLineString.find("\n", lineIndexBegin + 1);
        string singleLine = multiLineString.substr(lineIndexBegin +1, lineIndexEnd != string::npos ? lineIndexEnd - lineIndexBegin - 1 : string::npos);
        std::smatch matches;
        std::regex_match(singleLine, matches, regexRows);
        int i = 0;
        string::const_iterator searchStart( singleLine.cbegin() );
        while (std::regex_search(searchStart, singleLine.cend(), matches, regexRows)) {
            if (matches.size() > 1 && matches[matches.size()-1].length() > 0) {
                captures.push_back(matches[matches.size()-1]);
            }
            searchStart += matches.position() + matches.length();
            i++;
        }
        lineIndexBegin = lineIndexEnd;
    }
    return captures;
}

string teamCodesDataShared = "\nTampa Bay Rays;Tropicana Field;Tampa Bay;tam;Rays;\nBoston Red Sox;Fenway Park;Boston;bos;Red Sox;\nChicago White Sox;U.S. Cellular Field;Chi Sox;chw;White Sox;\nNew York Yankees;Yankee Stadium;NY Yankees;nyy;Yankees;\nPittsburgh Pirates;PNC Park;Pittsburgh;pit;Pirates;\nSt. Louis Cardinals;Busch Stadium;St. Louis;stl;Cardinals;\nSan Diego Padres;Petco Park;San Diego;sdg;Padres;\nAtlanta Braves;SunTrust Park;Atlanta;atl;Braves;\nMilwaukee Brewers;Miller Park;Milwaukee;mil;Brewers;\nChicago Cubs;Wrigley Field;Chi Cubs;chc;Cubs;\nCleveland Indians;Progressive Field;Cleveland;cle;Indians;\nMinnesota Twins;Target Field;Minnesota;min;Twins;\nLos Angeles Angels;Angel Stadium;LA Angels;laa;Angels;\nHouston Astros;Minute Maid Park;Houston;hou;Astros;\nTexas Rangers;Rangers Ballpark;Texas;tex;Rangers;\nOakland Athletics;Oakland Coliseum;Oakland;oak;Athletics;\nMiami Marlins;Marlins Park;Miami;mia;Marlins;\nSeattle Mariners;Safeco Field;Seattle;sea;Mariners;\nArizona Diamondbacks;Chase Field;Arizona;ari;Diamondbacks;\nLos Angeles Dodgers;Dodger Stadium;LA Dodgers;lad;Dodgers;\nToronto Blue Jays;Rogers Centre;Toronto;tor;Blue Jays;\nBaltimore Orioles;Camden Yards;Baltimore;bal;Orioles;\nCincinnati Reds;Great American Ball Park;Cincinnati;cin;Reds;\nSan Francisco Giants;AT&T Park;SF Giants;sfo;Giants;\nKansas City Royals;Kauffman Stadium;Kansas City;kan;Royals;\nPhiladelphia Phillies;Citizens Bank Park;Philadelphia;phi;Phillies;\nNew York Mets;Citi Field;NY Mets;nym;Mets;\nDetroit Tigers;Comerica Park;Detroit;det;Tigers;\nColorado Rockies;Coors Field;Colorado;col;Rockies;\nWashington Nationals;Nationals Park;Washington;was;Nationals;\n";

std::string ConvertStandardTeamCodeToRotoGuruTeamCode(std::string standardCode) {
	// rotogur1.com uses different team codes than standard...
	if (standardCode == "laa")
		standardCode = "ana";
	if (standardCode == "lad")
		standardCode = "los";
	if (standardCode == "mia")
		standardCode = "fla";
	return standardCode;
}
std::string ConvertRotoGuruTeamCodeToStandardTeamCode(std::string rotoGuruCode) {
	// rotogur1.com uses different team codes than standard...
	if (rotoGuruCode == "ana")
		rotoGuruCode = "laa";
	if (rotoGuruCode == "los")
		rotoGuruCode = "lad";
	if (rotoGuruCode == "fla")
		rotoGuruCode = "mia";
	return rotoGuruCode;
}
string convertTeamCodeToSynonym(string teamCode, int codeIndex) {
    size_t teamNameBeginIndex = teamCodesDataShared.find(";" + teamCode + ";", 0);
    if (teamNameBeginIndex == string::npos) {
        teamNameBeginIndex = teamCodesDataShared.find(teamCode + ";", 0);
    }
    if (teamNameBeginIndex != string::npos) {
        teamNameBeginIndex = teamCodesDataShared.rfind("\n", teamNameBeginIndex);
        teamNameBeginIndex++;
        size_t teamCodeLineEndIndex = teamCodesDataShared.find("\n", teamNameBeginIndex);
        string teamCodesLine = teamCodesDataShared.substr(teamNameBeginIndex, teamCodeLineEndIndex - teamNameBeginIndex);
        vector<string> teamCodesColumns = SplitStringIntoMultiple(teamCodesLine, ";");
        return teamCodesColumns[codeIndex];
    }
    LOGI("team %s wasn't found in synonyms list", teamCode.c_str());
    return "";
}