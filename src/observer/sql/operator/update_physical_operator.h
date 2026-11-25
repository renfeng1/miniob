/* Copyright (c) OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by miniob-lab on 2025/11/25.
//

#pragma once

#include "common/value.h"
#include "sql/operator/physical_operator.h"

class Trx;
class Table;

/**
 * @brief 更新物理算子
 * @ingroup PhysicalOperator
 * @details 遍历子算子（表扫描/索引扫描 + 谓词过滤）得到所有需要更新的记录，
 * 先全部收集起来再逐条更新，避免边扫描边修改导致数据错乱。
 */
class UpdatePhysicalOperator : public PhysicalOperator
{
public:
  UpdatePhysicalOperator(Table *table, string attribute_name, Value value)
      : table_(table), attribute_name_(std::move(attribute_name)), value_(value)
  {}

  virtual ~UpdatePhysicalOperator() = default;

  PhysicalOperatorType type() const override { return PhysicalOperatorType::UPDATE; }

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  Tuple *current_tuple() override { return nullptr; }

private:
  /// 根据旧记录构造更新后的新记录。新的记录与旧记录拥有相同的RID
  RC make_new_record(const Record &old_record, Record &new_record);

private:
  Table   *table_ = nullptr;
  string   attribute_name_;
  Value    value_;

  Trx           *trx_     = nullptr;
  vector<Record> records_;
};
