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
// Created by Meiyi & Wangyunlai on 2021/5/11.
//

#pragma once

#include <stddef.h>
#include <vector>

#include "common/sys/rc.h"
#include "common/lang/vector.h"
#include "storage/field/field_meta.h"
#include "storage/index/index_meta.h"
#include "storage/record/record_manager.h"

class IndexScanner;

/**
 * @brief 索引
 * @defgroup Index
 * @details 索引可能会有很多种实现，比如B+树、哈希表等，这里定义了一个基类，用于描述索引的基本操作。
 */

/**
 * @brief 索引基类
 * @ingroup Index
 */
class Index
{
public:
  Index()          = default;
  virtual ~Index() = default;

  /// 创建索引（多字段/复合索引）。keys 数组中的字段顺序即复合键中各字段的顺序
  virtual RC create(Table *table, const char *file_name, const IndexMeta &index_meta,
      const vector<FieldMeta> &field_metas)
  {
    return RC::UNSUPPORTED;
  }
  /// 打开索引（多字段/复合索引）
  virtual RC open(Table *table, const char *file_name, const IndexMeta &index_meta,
      const vector<FieldMeta> &field_metas)
  {
    return RC::UNSUPPORTED;
  }

  /// 创建索引（单字段）。保留此接口以兼容向量索引、全文索引等单字段索引
  virtual RC create(Table *table, const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta)
  {
    return RC::UNSUPPORTED;
  }
  /// 打开索引（单字段）
  virtual RC open(Table *table, const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta)
  {
    return RC::UNSUPPORTED;
  }

  virtual bool is_vector_index() { return false; }

  const IndexMeta &index_meta() const { return index_meta_; }

  /**
   * @brief 插入一条数据
   *
   * @param record 插入的记录，当前假设记录是定长的
   * @param[out] rid    插入的记录的位置
   */
  virtual RC insert_entry(const char *record, const RID *rid) = 0;

  /**
   * @brief 删除一条数据
   *
   * @param record 删除的记录，当前假设记录是定长的
   * @param[in] rid   删除的记录的位置
   */
  virtual RC delete_entry(const char *record, const RID *rid) = 0;

  /**
   * @brief 创建一个索引数据的扫描器
   *
   * @param left_key 要扫描的左边界
   * @param left_len 左边界的长度
   * @param left_inclusive 是否包含左边界
   * @param right_key 要扫描的右边界
   * @param right_len 右边界的长度
   * @param right_inclusive 是否包含右边界
   */
  virtual IndexScanner *create_scanner(const char *left_key, int left_len, bool left_inclusive, const char *right_key,
      int right_len, bool right_inclusive) = 0;

  /**
   * @brief 同步索引数据到磁盘
   *
   */
  virtual RC sync() = 0;

public:
  /// 初始化（多字段）
  RC init(const IndexMeta &index_meta, const vector<FieldMeta> &field_metas);
  /// 初始化（单字段）
  RC init(const IndexMeta &index_meta, const FieldMeta &field_meta);

  /// @brief 索引中所有字段的长度之和，也就是复合键的总长度
  int key_length() const;

  /// @brief 索引包含的字段个数
  int field_num() const { return static_cast<int>(field_metas_.size()); }

  /**
   * @brief 从一条记录中抽取索引键值
   * @details 单字段索引时键值就是该字段的数据；复合索引时按字段顺序将各字段数据拼接起来
   * @param record 输入的记录数据
   * @param key    输出的键值，长度为key_length()
   */
  RC make_key(const char *record, vector<char> &key) const;

  /**
   * @brief 比较两条记录在当前索引上的键值是否相同
   */
  bool same_key(const char *record1, const char *record2) const;

protected:
  IndexMeta         index_meta_;   ///< 索引的元数据
  vector<FieldMeta> field_metas_;  ///< 索引包含的字段，支持复合索引；当前至少包含一个字段
};

/**
 * @brief 索引扫描器
 * @ingroup Index
 */
class IndexScanner
{
public:
  IndexScanner()          = default;
  virtual ~IndexScanner() = default;

  /**
   * 遍历元素数据
   * 如果没有更多的元素，返回RECORD_EOF
   */
  virtual RC next_entry(RID *rid) = 0;
  virtual RC destroy()            = 0;
};
