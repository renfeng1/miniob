/* Copyright (c) 2021 Xie Meiyi(xiemeiyi@hust.edu.cn) and OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2021/5/12.
//

#pragma once

#include "common/sys/rc.h"
#include "common/lang/string.h"
#include "common/lang/vector.h"

class TableMeta;
class FieldMeta;

namespace Json {
class Value;
}  // namespace Json

/**
 * @brief 描述一个索引
 * @ingroup Index
 * @details 一个索引包含了表的哪些字段，索引的名称等。
 * 一个索引可以包含多个字段，称为复合索引/多字段索引。
 * 如果以后实现了多种类型的索引，还需要记录索引的类型，对应类型的一些元数据等
 */
class IndexMeta
{
public:
  IndexMeta() = default;

  /// 多字段索引的初始化。field_metas 中字段的顺序就是复合键中字段的顺序
  RC init(const char *name, const vector<FieldMeta> &field_metas);

  /// 单字段索引的初始化
  RC init(const char *name, const FieldMeta &field);

public:
  const char *name() const;
  /// 返回索引中的第一个字段名。为了兼容老版本接口保留
  const char *field() const;
  /// 返回索引中指定位置的字段名
  const char *field(int index) const;
  /// 索引包含的字段个数
  int field_num() const { return static_cast<int>(fields_.size()); }
  /// 索引包含的所有字段名
  const vector<string> &fields() const { return fields_; }

  void desc(ostream &os) const;

public:
  void      to_json(Json::Value &json_value) const;
  static RC from_json(const TableMeta &table, const Json::Value &json_value, IndexMeta &index);

protected:
  string         name_;    // index's name
  vector<string> fields_;  // index's fields（复合索引包含多个字段）
};
