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
// Created by Wangyunlai.wyl on 2021/5/18.
//

#include "storage/index/index_meta.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "storage/field/field_meta.h"
#include "storage/table/table_meta.h"
#include "json/json.h"

const static Json::StaticString FIELD_NAME("name");
const static Json::StaticString FIELD_FIELD_NAME("field_name");
const static Json::StaticString FIELD_FIELD("field");
const static Json::StaticString FIELD_FIELDS("fields");

RC IndexMeta::init(const char *name, const vector<FieldMeta> &field_metas)
{
  if (common::is_blank(name) || field_metas.empty()) {
    LOG_ERROR("Failed to init index, name is empty or no field.");
    return RC::INVALID_ARGUMENT;
  }

  name_ = name;
  fields_.clear();
  for (const FieldMeta &field_meta : field_metas) {
    fields_.push_back(field_meta.name());
  }
  return RC::SUCCESS;
}

RC IndexMeta::init(const char *name, const FieldMeta &field)
{
  if (common::is_blank(name)) {
    LOG_ERROR("Failed to init index, name is empty.");
    return RC::INVALID_ARGUMENT;
  }

  name_ = name;
  fields_.clear();
  fields_.push_back(field.name());
  return RC::SUCCESS;
}

void IndexMeta::to_json(Json::Value &json_value) const
{
  json_value[FIELD_NAME] = name_;
  // 兼容旧版本的元数据格式：保留单字段 field_name
  json_value[FIELD_FIELD_NAME] = fields_.front();
  // 复合索引将所有字段写入fields数组
  Json::Value fields_value;
  for (const string &field : fields_) {
    fields_value.append(field.c_str());
  }
  json_value[FIELD_FIELDS] = std::move(fields_value);
}

RC IndexMeta::from_json(const TableMeta &table, const Json::Value &json_value, IndexMeta &index)
{
  const Json::Value &name_value  = json_value[FIELD_NAME];
  const Json::Value &field_value = json_value[FIELD_FIELD_NAME];
  if (!name_value.isString()) {
    LOG_ERROR("Index name is not a string. json value=%s", name_value.toStyledString().c_str());
    return RC::INTERNAL;
  }

  if (!field_value.isString()) {
    LOG_ERROR("Field name of index [%s] is not a string. json value=%s",
        name_value.asCString(), field_value.toStyledString().c_str());
    return RC::INTERNAL;
  }

  // 优先读取fields数组（复合索引），如果没有则按单字段索引处理
  vector<string> field_names;
  const Json::Value &fields_value = json_value[FIELD_FIELDS];
  if (fields_value.isArray()) {
    for (const Json::Value &field : fields_value) {
      if (!field.isString()) {
        LOG_ERROR("Field name of index [%s] is not a string. json value=%s",
                  name_value.asCString(), field.toStyledString().c_str());
        return RC::INTERNAL;
      }
      field_names.push_back(field.asString());
    }
  } else {
    field_names.push_back(field_value.asString());
  }

  // 检查所有字段是否都存在于表中
  vector<FieldMeta> field_metas;
  for (const string &field_name : field_names) {
    const FieldMeta *field_meta = table.field(field_name.c_str());
    if (nullptr == field_meta) {
      LOG_ERROR("Deserialize index [%s]: no such field: %s", name_value.asCString(), field_name.c_str());
      return RC::SCHEMA_FIELD_MISSING;
    }
    field_metas.push_back(*field_meta);
  }

  return index.init(name_value.asCString(), field_metas);
}

const char *IndexMeta::name() const { return name_.c_str(); }

const char *IndexMeta::field() const { return fields_.front().c_str(); }

const char *IndexMeta::field(int index) const { return fields_[index].c_str(); }

void IndexMeta::desc(ostream &os) const
{
  os << "index name=" << name_ << ", field=";
  for (size_t i = 0; i < fields_.size(); i++) {
    if (i != 0) {
      os << ",";
    }
    os << fields_[i];
  }
}
