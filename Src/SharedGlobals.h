#ifndef GLOBALS_H_INCLUDED
#define GLOBALS_H_INCLUDED

#include <string>

enum GameType { Fanduel, DraftKings };

extern GameType gameType;
extern int maxTotalBudget;
// game times in Eastern and 24 hour format
extern int latestGameTime;
extern int earliestGameTime;
extern std::string todaysDate;
extern int reviewDateStart;
extern int reviewDateEnd;
extern float percentOf2017SeasonPassed;

#endif