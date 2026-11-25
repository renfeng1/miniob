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
// Created by wangyunlai.wyl on 2021/5/19.
//

#include "storage/index/index.h"
#include "common/lang/string.h"
#include "common/log/log.h"

RC Index::init(const IndexMeta &index_meta, const vector<FieldMeta> &field_metas)
{
  if (field_metas.empty()) {
    return RC::INVALID_ARGUMENT;
  }
  index_meta_  = index_meta;
  field_metas_ = field_metas;
  return RC::SUCCESS;
}

RC Index::init(const IndexMeta &index_meta, const FieldMeta &field_meta)
{
  index_meta_  = index_meta;
  field_metas_.clear();
  field_metas_.push_back(field_meta);
  return RC::SUCCESS;
}

int Index::key_length() const
{
  int length = 0;
  for (const FieldMeta &field_meta : field_metas_) {
    length += field_meta.len();
  }
  return length;
}

RC Index::make_key(const char *record, vector<char> &key) const
{
  if (nullptr == record) {
    LOG_WARN("record is null while making index key");
    return RC::INVALID_ARGUMENT;
  }

  key.resize(key_length());
  int offset = 0;
  for (const FieldMeta &field_meta : field_metas_) {
    memcpy(key.data() + offset, record + field_meta.offset(), field_meta.len());
    offset += field_meta.len();
  }
  return RC::SUCCESS;
}

bool Index::same_key(const char *record1, const char *record2) const
{
  for (const FieldMeta &field_meta : field_metas_) {
    if (memcmp(record1 + field_meta.offset(), record2 + field_meta.offset(), field_meta.len()) != 0) {
      return false;
    }
  }
  return true;
}
