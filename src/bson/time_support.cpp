/*    Copyright 2010 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <string>
#include "base.h"
#include "time_support.h"

#include <cstdio>
#include <iostream>
#include <ctime>
#include "errorcodes.h"
#include "parse_number.h"
#include "builder.h"
#include "cstdint.h"
#include "status_with.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include "windows.h"
#endif

#if 0
// Not all systems have timegm defined (it isn't part of POSIX), so fall back to our vendored
// implementation if our configure checks did not detect it as available on the current
// system. See SERVER-13446, SERVER-14019, and CXX-204.
extern "C" time_t timegm(struct tm *const tmp);
#endif

using namespace std;

namespace _bson {

    StringData getNextToken(const StringData& currentString,
                            const StringData& terminalChars,
                            size_t startIndex,
                            size_t* endIndex) {
        size_t index = startIndex;

        if (index == std::string::npos) {
            *endIndex = std::string::npos;
            return StringData();
        }

        for (; index < currentString.size(); index++) {
            if (terminalChars.find(currentString[index]) != std::string::npos) {
                break;
            }
        }

        // substr just returns the rest of the string if the length passed in is greater than the
        // number of characters remaining, and since std::string::npos is the length of the largest
        // possible string we know (std::string::npos - startIndex) is at least as long as the rest
        // of the string.  That means this handles both the case where we hit a terminating
        // character and we want a substring, and the case where didn't and just want the rest of
        // the string.
        *endIndex = (index < currentString.size() ? index : std::string::npos);
        return currentString.substr(startIndex, index - startIndex);
    }

    // Check to make sure that the string only consists of digits
    bool isOnlyDigits(const StringData& toCheck) {
        StringData digits("0123456789");
        for (StringData::const_iterator iterator = toCheck.begin();
            iterator != toCheck.end(); iterator++) {
            if (digits.find(*iterator) == std::string::npos) {
                return false;
            }
        }
        return true;
    }

    Status parseTimeZoneFromToken(const StringData& tzStr, int* tzAdjSecs) {

        *tzAdjSecs = 0;

        if (!tzStr.empty()) {
            if (tzStr[0] == 'Z') {
                if (tzStr.size() != 1) {
                    StringBuilder sb;
                    sb << "Found trailing characters in time zone specifier:  " << tzStr;
                    return Status(BadValue, sb.str());
                }
            }
            else if (tzStr[0] == '+' || tzStr[0] == '-') {
                if (tzStr.size() != 5 || !isOnlyDigits(tzStr.substr(1, 4))) {
                    StringBuilder sb;
                    sb << "Time zone adjustment string should be four digits:  " << tzStr;
                    return Status(BadValue, sb.str());
                }

                // Parse the hours component of the time zone offset.  Note that
                // parseNumberFromStringWithBase correctly handles the sign bit, so leave that in.
                StringData tzHoursStr = tzStr.substr(0, 3);
                int tzAdjHours = 0;
                Status status = parseNumberFromStringWithBase(tzHoursStr, 10, &tzAdjHours);
                if (!status.isOK()) {
                    return status;
                }

                if (tzAdjHours < -23 || tzAdjHours > 23) {
                    StringBuilder sb;
                    sb << "Time zone hours adjustment out of range:  " << tzAdjHours;
                    return Status(BadValue, sb.str());
                }

                StringData tzMinutesStr = tzStr.substr(3, 2);
                int tzAdjMinutes = 0;
                status = parseNumberFromStringWithBase(tzMinutesStr, 10, &tzAdjMinutes);
                if (!status.isOK()) {
                    return status;
                }

                if (tzAdjMinutes < 0 || tzAdjMinutes > 59) {
                    StringBuilder sb;
                    sb << "Time zone minutes adjustment out of range:  " << tzAdjMinutes;
                    return Status(BadValue, sb.str());
                }

                // Use the sign that parseNumberFromStringWithBase found to determine if we need to
                // flip the sign of our minutes component.  Also, we need to flip the sign of our
                // final result, because the offset passed in by the user represents how far off the
                // time they are giving us is from UTC, which means that we have to go the opposite
                // way to compensate and get the UTC time
                *tzAdjSecs = (-1) * ((tzAdjHours < 0 ? -1 : 1) * (tzAdjMinutes * 60) +
                                     (tzAdjHours * 60 * 60));

                // Disallow adjustiment of 24 hours or more in either direction (should be checked
                // above as the separate components of minutes and hours)
                assert(*tzAdjSecs > -86400 && *tzAdjSecs < 86400);
            }
            else {
                StringBuilder sb;
                sb << "Invalid time zone string:  \"" << tzStr
                   << "\".  Found invalid character at the beginning of time "
                   << "zone specifier: " << tzStr[0];
                return Status(BadValue, sb.str());
            }
        }
        else {
            return Status(BadValue, "Missing required time zone specifier for date");
        }

        return Status::OK();
    }

    Status parseMillisFromToken(
            const StringData& millisStr,
            int* resultMillis) {

        *resultMillis = 0;

        if (!millisStr.empty()) {
            if (millisStr.size() > 3 || !isOnlyDigits(millisStr)) {
                StringBuilder sb;
                sb << "Millisecond string should be at most three digits:  " << millisStr;
                return Status(BadValue, sb.str());
            }

            Status status = parseNumberFromStringWithBase(millisStr, 10, resultMillis);
            if (!status.isOK()) {
                return status;
            }

            // Treat the digits differently depending on how many there are.  1 digit = hundreds of
            // milliseconds, 2 digits = tens of milliseconds, 3 digits = milliseconds.
            int millisMagnitude = 1;
            if (millisStr.size() == 2) {
                millisMagnitude = 10;
            }
            else if (millisStr.size() == 1) {
                millisMagnitude = 100;
            }

            *resultMillis = *resultMillis * millisMagnitude;

            if (*resultMillis < 0 || *resultMillis > 1000) {
                StringBuilder sb;
                sb << "Millisecond out of range:  " << *resultMillis;
                return Status(BadValue, sb.str());
            }
        }

        return Status::OK();
    }

    Status parseTmFromTokens(
            const StringData& yearStr,
            const StringData& monthStr,
            const StringData& dayStr,
            const StringData& hourStr,
            const StringData& minStr,
            const StringData& secStr,
            std::tm* resultTm) {

        memset(resultTm, 0, sizeof(*resultTm));

        // Parse year
        if (yearStr.size() != 4 || !isOnlyDigits(yearStr)) {
            StringBuilder sb;
            sb << "Year string should be four digits:  " << yearStr;
            return Status(BadValue, sb.str());
        }

        Status status = parseNumberFromStringWithBase(yearStr, 10, &resultTm->tm_year);
        if (!status.isOK()) {
            return status;
        }

        if (resultTm->tm_year < 1970 || resultTm->tm_year > 9999) {
            StringBuilder sb;
            sb << "Year out of range:  " << resultTm->tm_year;
            return Status(BadValue, sb.str());
        }

        resultTm->tm_year -= 1900;

        // Parse month
        if (monthStr.size() != 2 || !isOnlyDigits(monthStr)) {
            StringBuilder sb;
            sb << "Month string should be two digits:  " << monthStr;
            return Status(BadValue, sb.str());
        }

        status = parseNumberFromStringWithBase(monthStr, 10, &resultTm->tm_mon);
        if (!status.isOK()) {
            return status;
        }

        if (resultTm->tm_mon < 1 || resultTm->tm_mon > 12) {
            StringBuilder sb;
            sb << "Month out of range:  " << resultTm->tm_mon;
            return Status(BadValue, sb.str());
        }

        resultTm->tm_mon -= 1;

        // Parse day
        if (dayStr.size() != 2 || !isOnlyDigits(dayStr)) {
            StringBuilder sb;
            sb << "Day string should be two digits:  " << dayStr;
            return Status(BadValue, sb.str());
        }

        status = parseNumberFromStringWithBase(dayStr, 10, &resultTm->tm_mday);
        if (!status.isOK()) {
            return status;
        }

        if (resultTm->tm_mday < 1 || resultTm->tm_mday > 31) {
            StringBuilder sb;
            sb << "Day out of range:  " << resultTm->tm_mday;
            return Status(BadValue, sb.str());
        }

        // Parse hour
        if (hourStr.size() != 2 || !isOnlyDigits(hourStr)) {
            StringBuilder sb;
            sb << "Hour string should be two digits:  " << hourStr;
            return Status(BadValue, sb.str());
        }

        status = parseNumberFromStringWithBase(hourStr, 10, &resultTm->tm_hour);
        if (!status.isOK()) {
            return status;
        }

        if (resultTm->tm_hour < 0 || resultTm->tm_hour > 23) {
            StringBuilder sb;
            sb << "Hour out of range:  " << resultTm->tm_hour;
            return Status(BadValue, sb.str());
        }

        // Parse minute
        if (minStr.size() != 2 || !isOnlyDigits(minStr)) {
            StringBuilder sb;
            sb << "Minute string should be two digits:  " << minStr;
            return Status(BadValue, sb.str());
        }

        status = parseNumberFromStringWithBase(minStr, 10, &resultTm->tm_min);
        if (!status.isOK()) {
            return status;
        }

        if (resultTm->tm_min < 0 || resultTm->tm_min > 59) {
            StringBuilder sb;
            sb << "Minute out of range:  " << resultTm->tm_min;
            return Status(BadValue, sb.str());
        }

        // Parse second if it exists
        if (secStr.empty()) {
            return Status::OK();
        }

        if (secStr.size() != 2 || !isOnlyDigits(secStr)) {
            StringBuilder sb;
            sb << "Second string should be two digits:  " << secStr;
            return Status(BadValue, sb.str());
        }

        status = parseNumberFromStringWithBase(secStr, 10, &resultTm->tm_sec);
        if (!status.isOK()) {
            return status;
        }

        if (resultTm->tm_sec < 0 || resultTm->tm_sec > 59) {
            StringBuilder sb;
            sb << "Second out of range:  " << resultTm->tm_sec;
            return Status(BadValue, sb.str());
        }

        return Status::OK();
    }

    Status parseTm(const StringData& dateString,
                   std::tm* resultTm,
                   int* resultMillis,
                   int* tzAdjSecs) {
        size_t yearEnd = std::string::npos;
        size_t monthEnd = std::string::npos;
        size_t dayEnd = std::string::npos;
        size_t hourEnd = std::string::npos;
        size_t minEnd = std::string::npos;
        size_t secEnd = std::string::npos;
        size_t millisEnd = std::string::npos;
        size_t tzEnd = std::string::npos;
        StringData yearStr, monthStr, dayStr, hourStr, minStr, secStr, millisStr, tzStr;

        yearStr = getNextToken(dateString, "-", 0, &yearEnd);
        monthStr = getNextToken(dateString, "-", yearEnd + 1, &monthEnd);
        dayStr = getNextToken(dateString, "T", monthEnd + 1, &dayEnd);
        hourStr = getNextToken(dateString, ":", dayEnd + 1, &hourEnd);
        minStr = getNextToken(dateString, ":+-Z", hourEnd + 1, &minEnd);

        // Only look for seconds if the character we matched for the end of the minutes token is a
        // colon
        if (minEnd != std::string::npos && dateString[minEnd] == ':') {
            // Make sure the string doesn't end with ":"
            if (minEnd == dateString.size() - 1) {
                StringBuilder sb;
                sb << "Invalid date:  " << dateString << ".  Ends with \"" << dateString[minEnd]
                   << "\" character";
                return Status(BadValue, sb.str());
            }

            secStr = getNextToken(dateString, ".+-Z", minEnd + 1, &secEnd);

            // Make sure we actually got something for seconds, since here we know they are expected
            if (secStr.empty()) {
                StringBuilder sb;
                sb << "Missing seconds in date: " << dateString;
                return Status(BadValue, sb.str());
            }
        }

        // Only look for milliseconds if the character we matched for the end of the seconds token
        // is a period
        if (secEnd != std::string::npos && dateString[secEnd] == '.') {
            // Make sure the string doesn't end with "."
            if (secEnd == dateString.size() - 1) {
                StringBuilder sb;
                sb << "Invalid date:  " << dateString << ".  Ends with \"" << dateString[secEnd]
                   << "\" character";
                return Status(BadValue, sb.str());
            }

            millisStr = getNextToken(dateString, "+-Z", secEnd + 1, &millisEnd);

            // Make sure we actually got something for millis, since here we know they are expected
            if (millisStr.empty()) {
                StringBuilder sb;
                sb << "Missing seconds in date: " << dateString;
                return Status(BadValue, sb.str());
            }
        }

        // Now look for the time zone specifier depending on which prefix of the time we provided
        if (millisEnd != std::string::npos) {
            tzStr = getNextToken(dateString, "", millisEnd, &tzEnd);
        }
        else if (secEnd != std::string::npos && dateString[secEnd] != '.') {
            tzStr = getNextToken(dateString, "", secEnd, &tzEnd);
        }
        else if (minEnd != std::string::npos && dateString[minEnd] != ':') {
            tzStr = getNextToken(dateString, "", minEnd, &tzEnd);
        }

        Status status = parseTmFromTokens(yearStr, monthStr, dayStr, hourStr, minStr, secStr,
                                          resultTm);
        if (!status.isOK()) {
            return status;
        }

        status = parseTimeZoneFromToken(tzStr, tzAdjSecs);
        if (!status.isOK()) {
            return status;
        }

        status = parseMillisFromToken(millisStr, resultMillis);
        if (!status.isOK()) {
            return status;
        }

        return Status::OK();
    }

    StatusWith<Date_t> dateFromISOString(const StringData& dateString) {
        std::tm theTime;
        int millis = 0;
        int tzAdjSecs = 0;
        Status status = parseTm(dateString, &theTime, &millis, &tzAdjSecs);
        if (!status.isOK()) {
            return StatusWith<Date_t>(BadValue, "", status.reason());
        }

        unsigned long long resultMillis = 0;

#if defined(_WIN32)
        SYSTEMTIME dateStruct;
        dateStruct.wMilliseconds = millis;
        dateStruct.wSecond = theTime.tm_sec;
        dateStruct.wMinute = theTime.tm_min;
        dateStruct.wHour = theTime.tm_hour;
        dateStruct.wDay = theTime.tm_mday;
        dateStruct.wDayOfWeek = -1; /* ignored */
        dateStruct.wMonth = theTime.tm_mon + 1;
        dateStruct.wYear = theTime.tm_year + 1900;

        // Output parameter for SystemTimeToFileTime
        FILETIME fileTime;

        // the wDayOfWeek member of SYSTEMTIME is ignored by this function
        if (SystemTimeToFileTime(&dateStruct, &fileTime) == 0) {
            StringBuilder sb;
            sb << "Error converting Windows system time to file time for date:  " << dateString
               << ".  Error code:  " << GetLastError();
            return StatusWith<Date_t>(ErrorCodes::BadValue, sb.str());
        }

        // The Windows FILETIME structure contains two parts of a 64-bit value representing the
        // number of 100-nanosecond intervals since January 1, 1601
        unsigned long long windowsTimeOffset =
            (static_cast<unsigned long long>(fileTime.dwHighDateTime) << 32) |
             fileTime.dwLowDateTime;

        // There are 11644473600 seconds between the unix epoch and the windows epoch
        // 100-nanoseconds = milliseconds * 10000
        unsigned long long epochDifference = 11644473600000 * 10000;

        // removes the diff between 1970 and 1601
        windowsTimeOffset -= epochDifference;

        // 1 milliseconds = 1000000 nanoseconds = 10000 100-nanosecond intervals
        resultMillis = windowsTimeOffset / 10000;
#else
        struct tm dateStruct = { 0 };
        dateStruct.tm_sec = theTime.tm_sec;
        dateStruct.tm_min = theTime.tm_min;
        dateStruct.tm_hour = theTime.tm_hour;
        dateStruct.tm_mday = theTime.tm_mday;
        dateStruct.tm_mon = theTime.tm_mon;
        dateStruct.tm_year = theTime.tm_year;
        dateStruct.tm_wday = 0;
        dateStruct.tm_yday = 0;

        resultMillis = (1000 * static_cast<unsigned long long>(timegm(&dateStruct))) + millis;
#endif

        resultMillis += (tzAdjSecs * 1000);

        return StatusWith<Date_t>(resultMillis);
    }

}
