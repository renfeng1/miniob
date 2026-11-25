/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "common/type/data_type.h"

/**
 * @brief 日期类型
 * @ingroup DataType
 * @details 内部使用 4 字节整数按 YYYYMMDD 格式存储，例如 2021-01-02 存储为 20210102。
 * 这样日期的比较可以直接用整数比较，天然符合日历顺序。
 */
class DateType : public DataType
{
public:
  DateType() : DataType(AttrType::DATES) {}
  virtual ~DateType() {}

  int compare(const Value &left, const Value &right) const override;
  int compare(const Column &left, const Column &right, int left_idx, int right_idx) const override;

  RC cast_to(const Value &val, AttrType type, Value &result) const override;

  int cast_cost(const AttrType type) override
  {
    if (type == AttrType::DATES) {
      return 0;
    } else if (type == AttrType::CHARS) {
      return 1;
    }
    return INT32_MAX;
  }

  RC set_value_from_str(Value &val, const string &data) const override;

  RC to_string(const Value &val, string &result) const override;

  /**
   * @brief 将 YYYY-MM-DD 或 YYYYMMDD 格式的字符串解析成 YYYYMMDD 整数
   * @param str 输入的日期字符串
   * @param date_value 输出的 YYYYMMDD 整数
   */
  static RC parse_date(const char *str, int &date_value);

  /**
   * @brief 将 YYYYMMDD 整数格式化为 YYYY-MM-DD 字符串
   */
  static string format_date(int date_value);
};
