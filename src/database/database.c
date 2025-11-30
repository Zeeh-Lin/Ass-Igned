#include <stdio.h>
#include <sqlite3.h>

int main() {
    sqlite3 *db;
    char *errmsg = NULL;

    // 1. 打开数据库（没有就自动创建）
    if (sqlite3_open("test.db", &db) != SQLITE_OK) {
        printf("Failed to open DB: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // 2. 创建表
    const char *sql_create =
        "CREATE TABLE IF NOT EXISTS tasks ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " title TEXT NOT NULL"
        ");";

    if (sqlite3_exec(db, sql_create, NULL, NULL, &errmsg) != SQLITE_OK) {
        printf("SQL error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }

    // 3. 插入数据（直接 exec）
    const char *sql_insert = "INSERT INTO tasks (title) VALUES ('Hello SQLite');";
    if (sqlite3_exec(db, sql_insert, NULL, NULL, &errmsg) != SQLITE_OK) {
        printf("Insert error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }

    // 4. 查询数据（使用 prepare + step）
    const char *sql_select = "SELECT id, title FROM tasks;";
    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(db, sql_select, -1, &stmt, NULL);

    printf("Query result:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *title = sqlite3_column_text(stmt, 1);
        printf("  %d  %s\n", id, title);
    }

    sqlite3_finalize(stmt);  // 释放语句对象
    sqlite3_close(db);       // 关闭数据库

    return 0;
}
