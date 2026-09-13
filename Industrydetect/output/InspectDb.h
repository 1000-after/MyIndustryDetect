#pragma once
// #pragma once：本头文件在同一次编译里只会被包含一次，防止重复定义

#include "core/InspectTypes.h"  // InspectResult / InspectParams：检测结果和参数结构体
#include <QString>              // Qt 字符串

// 前置声明：头文件里只写指针/引用时，不必 #include 完整类定义，减少编译依赖
class QSqlDatabase;

// =========================================================
// InspectDb：检测结果入库（第 7 课 / 质量追溯）
//
// 【低耦合高内聚】怎么理解？
//   - 高内聚：这个类只干“连数据库 + 插入检测记录”一件事
//   - 低耦合：窗口 IndustryDetect 不知道 SQL 怎么写；
//             算法 PatchDetector 也不碰数据库。
//             大家只认 InspectResult / InspectParams 这两个结构体。
//
// 【为什么单独一个模块？】
//   以后换 PostgreSQL、或改成写 CSV，主要改这里即可，界面少动。
//
// 【本项目连接方式】
//   Qt 插件：QODBC（你本机有 qsqlodbc.dll）
//   Windows 驱动：MySQL ODBC 26.7 Unicode Driver（你已装好）
//   本机：127.0.0.1:3306 / 库 industry_detect / 用户 root
// =========================================================
class InspectDb
{
public:
    // 构造：只初始化成员，真正连库在 open() 里做
    InspectDb();

    // 析构：自动 close，防止连接一直占着
    ~InspectDb();

    // 禁止拷贝：一个对象对应一条命名连接，拷贝容易乱
    InspectDb(const InspectDb&) = delete;
    InspectDb& operator=(const InspectDb&) = delete;

    // ---------- 连接管理 ----------
    // 打开数据库连接；成功返回 true，失败返回 false
    // 失败原因可用 lastError() 查看
    bool open();

    // 关闭连接（析构也会调用；也可主动调用）
    void close();

    // 当前是否已成功打开
    bool isOpen() const;

    // ---------- 业务：写入一条检测记录 ----------
    // result：一次检测的结果（路径、OK/NG、缺陷数、耗时…）
    // params：当时用的配方参数（模糊、阈值、面积…），方便事后追溯
    // 成功返回 true；失败返回 false，并用 lastError() 说明原因
    bool insertRecord(const InspectResult& result, const InspectParams& params);

    // 最近一次失败的人类可读错误信息（给状态栏/弹窗用）
    QString lastError() const;

private:
    // 把 result 转成表字段 result 要存的字符串：OK / NG / FAIL
    static QString resultText(const InspectResult& result);

    // Qt 里每条连接要有唯一名字；固定名字方便 open/close 找到同一条
    static const char* connectionName();

private:
    bool m_opened = false;   // 是否已 open 成功
    QString m_lastError;     // 最近一次错误说明
};
