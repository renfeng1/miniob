/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "common/lang/comparator.h"
#include "common/lang/sstream.h"
#include "common/log/log.h"
#include "common/type/date_type.h"
#include "common/value.h"
#include "storage/common/column.h"

/**
 * @brief 判断某年是否为闰年
 */
static bool is_leap_year(int year)
{
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * @brief 校验日期是否合法（月份 1-12，日不超过该月天数）
 */
static bool validate_date(int year, int month, int day)
{
  if (year < 1 || year > 9999) {
    return false;
  }
  if (month < 1 || month > 12) {
    return false;
  }
  static const int days_of_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int              max_day         = days_of_month[month];
  if (month == 2 && is_leap_year(year)) {
    max_day = 29;
  }
  return day >= 1 && day <= max_day;
}

int DateType::compare(const Value &left, const Value &right) const
{
  ASSERT(left.attr_type() == AttrType::DATES, "left type is not date");
  ASSERT(right.attr_type() == AttrType::DATES || right.attr_type() == AttrType::CHARS, "right type is not date or chars");

  if (right.attr_type() == AttrType::DATES) {
    int left_value  = left.get_int();
    int right_value = right.get_int();
    return (left_value < right_value) ? -1 : (left_value > right_value ? 1 : 0);
  }

  // 与字符串比较时，先把字符串解析成日期
  int    right_value = 0;
  string right_str   = right.get_string();
  if (RC::SUCCESS != parse_date(right_str.c_str(), right_value)) {
    return INT32_MAX;
  }
  int left_value = left.get_int();
  return (left_value < right_value) ? -1 : (left_value > right_value ? 1 : 0);
}

int DateType::compare(const Column &left, const Column &right, int left_idx, int right_idx) const
{
  ASSERT(left.attr_type() == AttrType::DATES, "left type is not date");
  ASSERT(right.attr_type() == AttrType::DATES, "right type is not date");
  return common::compare_int(
      (void *)&((int *)left.data())[left_idx], (void *)&((int *)right.data())[right_idx]);
}

RC DateType::cast_to(const Value &val, AttrType type, Value &result) const
{
  switch (type) {
    case AttrType::CHARS: {
      string str = format_date(val.get_int());
      result.set_string(str.c_str(), static_cast<int>(str.size()));
      return RC::SUCCESS;
    }
    default: {
      LOG_WARN("unsupported cast date to type %d", static_cast<int>(type));
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
  }
}

RC DateType::parse_date(const char *str, int &date_value)
{
  if (str == nullptr) {
    return RC::INVALID_ARGUMENT;
  }

  // 跳过前导空格
  while (*str != '\0' && isspace(static_cast<unsigned char>(*str))) {
    str++;
  }
  if (*str == '\0') {
    return RC::INVALID_ARGUMENT;
  }

  int  parts[3] = {0, 0, 0};
  int  part_idx = 0;
  bool has_digit = false;

  for (const char *p = str; *p != '\0'; p++) {
    if (isdigit(static_cast<unsigned char>(*p))) {
      if (part_idx > 2) {
        return RC::INVALID_ARGUMENT;
      }
      parts[part_idx] = parts[part_idx] * 10 + (*p - '0');
      if (parts[part_idx] > 9999) {
        return RC::INVALID_ARGUMENT;
      }
      has_digit = true;
    } else if (*p == '-' || *p == '/' || *p == '.') {
      if (!has_digit) {
        return RC::INVALID_ARGUMENT;
      }
      part_idx++;
      has_digit = false;
      if (part_idx > 2) {
        return RC::INVALID_ARGUMENT;
      }
    } else if (isspace(static_cast<unsigned char>(*p))) {
      continue;
    } else {
      return RC::INVALID_ARGUMENT;
    }
  }

  if (!has_digit || part_idx != 2) {
    // 也支持紧凑格式 YYYYMMDD
    if (strlen(str) == 8) {
      int compact = 0;
      for (const char *p = str; *p != '\0'; p++) {
        if (!isdigit(static_cast<unsigned char>(*p))) {
          return RC::INVALID_ARGUMENT;
        }
        compact = compact * 10 + (*p - '0');
      }
      parts[0] = compact / 10000;
      parts[1] = (compact / 100) % 100;
      parts[2] = compact % 100;
    } else {
      return RC::INVALID_ARGUMENT;
    }
  }

  if (!validate_date(parts[0], parts[1], parts[2])) {
    return RC::INVALID_ARGUMENT;
  }

  date_value = parts[0] * 10000 + parts[1] * 100 + parts[2];
  return RC::SUCCESS;
}

string DateType::format_date(int date_value)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", date_value / 10000, (date_value / 100) % 100, date_value % 100);
  return string(buf);
}

RC DateType::set_value_from_str(Value &val, const string &data) const
{
  int date_value = 0;
  RC  rc         = parse_date(data.c_str(), date_value);
  if (RC::SUCCESS != rc) {
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }
  val.set_int(date_value);
  // set_int 会把类型设置为 INTS，这里修正为 DATES，
  // 否则后续按类型分派时会走到整型的比较与格式化逻辑
  val.set_type(AttrType::DATES);
  return RC::SUCCESS;
}

RC DateType::to_string(const Value &val, string &result) const
{
  result = format_date(val.get_int());
  return RC::SUCCESS;
}
