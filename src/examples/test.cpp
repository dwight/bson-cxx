
#pragma once

#include "status.h"
#include "string_data.h"

namespace _bson {

  /**
   * Parses a number out of a StringData.
   *
   * Parses "stringValue", interpreting it as a number of the given "base".  On success, stores
   * the parsed value into "*result" and returns Status::OK().
   *
   * Valid values for "base" are 2-36, with 0 meaning "choose the base by inspecting the prefix
   * on the number", as in strtol.  Returns Status::BadValue if an illegal value is supplied for
   * "base".
   *
   * The entirety of the string must consist of digits in the given base, except optionally the
   * first character may be "+" or "-", and hexadecimal numbers may begin "0x".  Same as strtol,
   * without the property of stripping whitespace at the beginning, and fails to parse if there
   * are non-digit characters at the end of the string.
   *
   * See parse_number.cpp for the available instantiations, and add any new instantiations there.
   */
  template <typename NumberType>
  Status  parseNumberFromStringWithBase(const StringData& stringValue, int base, NumberType* result);

  template <typename NumberType>
  static Status parseNumberFromString(const StringData& stringValue, NumberType* result) {
    return parseNumberFromStringWithBase(stringValue, 0, result);
  }

}
