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

#include "sql/operator/update_physical_operator.h"

#include "common/log/log.h"
#include "sql/expr/tuple.h"
#include "storage/db/db.h"
#include "storage/field/field_meta.h"
#include "storage/record/record_manager.h"
#include "storage/table/table.h"
#include "storage/table/table_meta.h"
#include "storage/trx/trx.h"

RC UpdatePhysicalOperator::open(Trx *trx)
{
  if (children_.empty()) {
    return RC::SUCCESS;
  }

  unique_ptr<PhysicalOperator> &child = children_[0];

  RC rc = child->open(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open child operator: %s", strrc(rc));
    return rc;
  }

  trx_ = trx;

  // 先收集所有需要更新的记录再统一更新，避免边扫描边修改造成的数据错乱
  while (OB_SUCC(rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("failed to get current record: %s", strrc(rc));
      return rc;
    }

    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
    Record   &record    = row_tuple->record();
    records_.emplace_back(std::move(record));
  }
  child->close();

  for (Record &old_record_ref : records_) {
    // old_record_ref 中的数据指向buffer pool的页内存。
    // 更新操作会原地修改页内的数据，所以先把旧记录的数据复制一份出来，
    // 后面维护索引时使用的旧键值必须来自这份副本，否则拿到的是更新后的数据
    Record old_record;
    old_record.copy_data(old_record_ref.data(), old_record_ref.len());
    old_record.set_rid(old_record_ref.rid());

    Record new_record;
    rc = make_new_record(old_record, new_record);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to make new record for update. table=%s, field=%s, rc=%s",
               table_->name(), attribute_name_.c_str(), strrc(rc));
      return rc;
    }

    rc = trx_->update_record(table_, old_record, new_record);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to update record. table=%s, rid=%s, rc=%s",
               table_->name(), old_record.rid().to_string().c_str(), strrc(rc));
      return rc;
    }
  }

  return RC::SUCCESS;
}

RC UpdatePhysicalOperator::make_new_record(const Record &old_record, Record &new_record)
{
  const TableMeta &table_meta = table_->table_meta();
  const FieldMeta *field      = table_meta.field(attribute_name_.c_str());
  if (nullptr == field) {
    LOG_WARN("no such field. table=%s, field=%s", table_->name(), attribute_name_.c_str());
    return RC::SCHEMA_FIELD_MISSING;
  }

  // 更新值的类型检查，若与字段类型不一致则尝试转换
  Value real_value;
  RC    rc = RC::SUCCESS;
  if (value_.attr_type() != field->type()) {
    rc = Value::cast_to(value_, field->type(), real_value);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to cast update value. field=%s, field_type=%s, value_type=%s, rc=%s",
               field->name(), attr_type_to_string(field->type()), attr_type_to_string(value_.attr_type()), strrc(rc));
      return rc;
    }
  } else {
    real_value = value_;
  }

  const int record_size = table_meta.record_size();
  // 注意：Record的析构函数使用free释放data owner，这里必须使用malloc分配
  char     *new_data    = (char *)malloc(record_size);
  memcpy(new_data, old_record.data(), record_size);

  // 先清空字段区域再写入新值，防止旧数据残留（比如旧字符串比新字符串长的情况）
  memset(new_data + field->offset(), 0, field->len());

  size_t copy_len = field->len();
  if (field->type() == AttrType::CHARS) {
    const size_t data_len = real_value.length();
    if (copy_len > data_len) {
      copy_len = data_len + 1;  // 字符串需要多复制一个结束符
    }
  }
  memcpy(new_data + field->offset(), real_value.data(), copy_len);

  // 新记录与旧记录拥有相同的RID
  new_record.set_data_owner(new_data, record_size);
  new_record.set_rid(old_record.rid());
  return RC::SUCCESS;
}

RC UpdatePhysicalOperator::next()
{
  return RC::RECORD_EOF;
}

RC UpdatePhysicalOperator::close()
{
  return RC::SUCCESS;
}
