/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
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
// drop table 执行器
//

#pragma once

#include "common/log/log.h"
#include "common/sys/rc.h"
#include "event/session_event.h"
#include "event/sql_event.h"
#include "session/session.h"
#include "sql/executor/execute_stage.h"
#include "sql/stmt/drop_table_stmt.h"
#include "storage/db/db.h"

/**
 * @brief drop table 语句的执行器
 * @ingroup Executor
 */
class DropTableExecutor
{
public:
  DropTableExecutor() = default;
  ~DropTableExecutor() = default;

  RC execute(SQLStageEvent *sql_event)
  {
    Stmt    *stmt    = sql_event->stmt();
    Session *session = sql_event->session_event()->session();

    ASSERT(stmt->type() == StmtType::DROP_TABLE,
        "drop table executor can not run this command: %d", static_cast<int>(stmt->type()));

    DropTableStmt *drop_table_stmt = static_cast<DropTableStmt *>(stmt);
    Db            *db              = session->get_current_db();

    RC rc = db->drop_table(drop_table_stmt->table_name().c_str());
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to drop table. db=%s, table=%s, rc=%d:%s",
               db->name(), drop_table_stmt->table_name().c_str(), rc, strrc(rc));
    }
    return rc;
  }
};
