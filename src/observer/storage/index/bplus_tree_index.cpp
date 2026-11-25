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

#include "storage/index/bplus_tree_index.h"
#include "common/log/log.h"
#include "storage/table/table.h"
#include "storage/db/db.h"

BplusTreeIndex::~BplusTreeIndex() noexcept { close(); }

RC BplusTreeIndex::create(Table *table, const char *file_name, const IndexMeta &index_meta,
    const vector<FieldMeta> &field_metas)
{
  if (inited_) {
    LOG_WARN("Failed to create index due to the index has been created before. file_name:%s, index:%s",
        file_name, index_meta.name());
    return RC::RECORD_OPENNED;
  }

  RC rc = Index::init(index_meta, field_metas);
  if (RC::SUCCESS != rc) {
    LOG_WARN("Failed to init index. file_name:%s, index:%s, rc:%s", file_name, index_meta.name(), strrc(rc));
    return rc;
  }

  // 将各字段的信息转换成B+树需要的键段描述
  vector<IndexKeySegment> key_fields;
  for (const FieldMeta &field_meta : field_metas) {
    IndexKeySegment segment;
    segment.type   = field_meta.type();
    segment.length = field_meta.len();
    key_fields.push_back(segment);
  }

  BufferPoolManager &bpm = table->db()->buffer_pool_manager();
  rc = index_handler_.create(table->db()->log_handler(), bpm, file_name, key_fields);
  if (RC::SUCCESS != rc) {
    LOG_WARN("Failed to create index_handler, file_name:%s, index:%s, rc:%s",
        file_name, index_meta.name(), strrc(rc));
    return rc;
  }

  inited_ = true;
  table_  = table;
  LOG_INFO("Successfully create index, file_name:%s, index:%s, field_num:%d",
    file_name, index_meta.name(), index_meta.field_num());
  return RC::SUCCESS;
}

RC BplusTreeIndex::open(Table *table, const char *file_name, const IndexMeta &index_meta,
    const vector<FieldMeta> &field_metas)
{
  if (inited_) {
    LOG_WARN("Failed to open index due to the index has been initedd before. file_name:%s, index:%s",
        file_name, index_meta.name());
    return RC::RECORD_OPENNED;
  }

  RC rc = Index::init(index_meta, field_metas);
  if (RC::SUCCESS != rc) {
    LOG_WARN("Failed to init index. file_name:%s, index:%s, rc:%s", file_name, index_meta.name(), strrc(rc));
    return rc;
  }

  BufferPoolManager &bpm = table->db()->buffer_pool_manager();
  rc = index_handler_.open(table->db()->log_handler(), bpm, file_name);
  if (RC::SUCCESS != rc) {
    LOG_WARN("Failed to open index_handler, file_name:%s, index:%s, rc:%s",
        file_name, index_meta.name(), strrc(rc));
    return rc;
  }

  inited_ = true;
  table_  = table;
  LOG_INFO("Successfully open index, file_name:%s, index:%s, field_num:%d",
    file_name, index_meta.name(), index_meta.field_num());
  return RC::SUCCESS;
}

RC BplusTreeIndex::create(Table *table, const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta)
{
  vector<FieldMeta> field_metas;
  field_metas.push_back(field_meta);
  return create(table, file_name, index_meta, field_metas);
}

RC BplusTreeIndex::open(Table *table, const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta)
{
  vector<FieldMeta> field_metas;
  field_metas.push_back(field_meta);
  return open(table, file_name, index_meta, field_metas);
}

RC BplusTreeIndex::close()
{
  if (inited_) {
    LOG_INFO("Begin to close index, index:%s", index_meta_.name());
    index_handler_.close();
    inited_ = false;
  }
  LOG_INFO("Successfully close index.");
  return RC::SUCCESS;
}

RC BplusTreeIndex::insert_entry(const char *record, const RID *rid)
{
  // 从记录数据中抽取索引键值。单字段索引时键值就是字段数据，
  // 复合索引时按字段顺序拼接所有字段的数据
  vector<char> key;
  RC rc = make_key(record, key);
  if (rc != RC::SUCCESS) {
    LOG_WARN("Failed to make index key. index:%s, rc:%s", index_meta_.name(), strrc(rc));
    return rc;
  }
  return index_handler_.insert_entry(key.data(), rid);
}

RC BplusTreeIndex::delete_entry(const char *record, const RID *rid)
{
  vector<char> key;
  RC rc = make_key(record, key);
  if (rc != RC::SUCCESS) {
    LOG_WARN("Failed to make index key. index:%s, rc:%s", index_meta_.name(), strrc(rc));
    return rc;
  }
  return index_handler_.delete_entry(key.data(), rid);
}

IndexScanner *BplusTreeIndex::create_scanner(
    const char *left_key, int left_len, bool left_inclusive, const char *right_key, int right_len, bool right_inclusive)
{
  // 复合索引上的扫描未实现：复合键无法用单个字段的值直接构造出完整的键
  if (field_metas_.size() > 1) {
    LOG_WARN("index scanner on composite index is not supported. index:%s", index_meta_.name());
    return nullptr;
  }

  BplusTreeIndexScanner *index_scanner = new BplusTreeIndexScanner(index_handler_);
  RC rc = index_scanner->open(left_key, left_len, left_inclusive, right_key, right_len, right_inclusive);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open index scanner. rc=%d:%s", rc, strrc(rc));
    delete index_scanner;
    return nullptr;
  }
  return index_scanner;
}

RC BplusTreeIndex::sync() { return index_handler_.sync(); }

////////////////////////////////////////////////////////////////////////////////
BplusTreeIndexScanner::BplusTreeIndexScanner(BplusTreeHandler &tree_handler) : tree_scanner_(tree_handler) {}

BplusTreeIndexScanner::~BplusTreeIndexScanner() noexcept { tree_scanner_.close(); }

RC BplusTreeIndexScanner::open(
    const char *left_key, int left_len, bool left_inclusive, const char *right_key, int right_len, bool right_inclusive)
{
  return tree_scanner_.open(left_key, left_len, left_inclusive, right_key, right_len, right_inclusive);
}

RC BplusTreeIndexScanner::next_entry(RID *rid) { return tree_scanner_.next_entry(*rid); }

RC BplusTreeIndexScanner::destroy()
{
  delete this;
  return RC::SUCCESS;
}
