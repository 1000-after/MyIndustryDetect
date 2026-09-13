#include "output/InspectDb.h"

#include <QSqlDatabase>  // 数据库连接对象
#include <QSqlQuery>     // 执行 SQL（INSERT/SELECT…）
#include <QSqlError>     // 错误信息
#include <QFileInfo>     // 从完整路径取出文件名
#include <QVariant>      // bindValue 绑定参数时用

// =========================================================
// 构造 / 析构
// =========================================================
InspectDb::InspectDb() = default;

InspectDb::~InspectDb()
{
    // 窗口关掉时自动断开，避免“连接没关干净”
    close();
}

const char* InspectDb::connectionName()
{
    // 自定义连接名：避免和默认连接冲突；整个程序共用这一条
    return "industry_detect_odbc";
}

// =========================================================
// open：用 QODBC + MySQL ODBC Unicode 驱动连上本机 MySQL
//
// 小白理解：
//   程序 → Qt 的 QODBC 插件 → MySQL ODBC 驱动 → MySQL 服务 → 表
// =========================================================
bool InspectDb::open()
{
    m_lastError.clear();

    // 若以前打开过，先关掉，保证可重复调用 open()
    if (QSqlDatabase::contains(connectionName())) {
        {
            // 先取出再 close，再 removeDatabase
            QSqlDatabase old = QSqlDatabase::database(connectionName());
            if (old.isOpen()) {
                old.close();
            }
        }
        QSqlDatabase::removeDatabase(connectionName());
    }

    // ---------- 1) 创建“ODBC 类型”的数据库连接对象 ----------
    // "QODBC" 必须和 plugins/sqldrivers/qsqlodbc.dll 对应
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QODBC"),
                                                QString::fromLatin1(connectionName()));

    // ---------- 2) 无 DSN 连接串（不用先在 ODBC 里建数据源名）----------
    // DRIVER={...} 必须和你在「ODBC 数据源(64位)→驱动程序」里看到的名字完全一致
    // SERVER/PORT/DATABASE/UID/PWD：就是你说的本机回环 + root/root
    const QString connStr =
        QStringLiteral(
            "DRIVER={MySQL ODBC 26.7 Unicode Driver};"
            "SERVER=127.0.0.1;"
            "PORT=3306;"
            "DATABASE=industry_detect;"
            "UID=root;"
            "PWD=root;"
            "CHARSET=UTF8;");

    // ODBC 方式：整段连接信息塞进 setDatabaseName
    db.setDatabaseName(connStr);

    // ---------- 3) 真正尝试连接 ----------
    if (!db.open()) {
        m_opened = false;
        m_lastError = QStringLiteral("数据库打开失败：%1")
                          .arg(db.lastError().text());
        // 失败也清掉这条连接注册，避免下次 contains 判断混乱
        QSqlDatabase::removeDatabase(QString::fromLatin1(connectionName()));
        return false;
    }

    m_opened = true;
    m_lastError.clear();
    return true;
}

// =========================================================
// close：断开并移除命名连接
// =========================================================
void InspectDb::close()
{
    if (!QSqlDatabase::contains(connectionName())) {
        m_opened = false;
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(connectionName());
        if (db.isOpen()) {
            db.close();
        }
    }
    // removeDatabase 前，上面那个 db 局部变量必须已经析构（所以用了花括号）
    QSqlDatabase::removeDatabase(QString::fromLatin1(connectionName()));
    m_opened = false;
}

bool InspectDb::isOpen() const
{
    if (!m_opened) {
        return false;
    }
    if (!QSqlDatabase::contains(connectionName())) {
        return false;
    }
    return QSqlDatabase::database(connectionName()).isOpen();
}

QString InspectDb::lastError() const
{
    return m_lastError;
}

// =========================================================
// resultText：把检测结果转成表里 result 列要存的文字
//   OK   = 合格
//   NG   = 不合格（有缺陷）
//   FAIL = 读图失败等异常（我们用 defectCount < 0 表示）
// =========================================================
QString InspectDb::resultText(const InspectResult& result)
{
    if (result.defectCount < 0) {
        return QStringLiteral("FAIL");
    }
    return result.ok ? QStringLiteral("OK") : QStringLiteral("NG");
}

// =========================================================
// insertRecord：往 inspect_record 插一行
//
// 使用“预编译 + 绑定参数”：
//   - 更安全（减少 SQL 注入风险）
//   - 路径里有空格/引号也不容易把 SQL 写坏
// =========================================================
bool InspectDb::insertRecord(const InspectResult& result, const InspectParams& params)
{
    m_lastError.clear();

    // 没连上就不要硬插
    if (!isOpen()) {
        m_lastError = QStringLiteral("数据库未打开，无法入库");
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(QString::fromLatin1(connectionName()));
    QSqlQuery query(db);

    // 列名必须和你在 Navicat 里建的表一致：
    // image_path, file_name, result, defect_count, cost_ms,
    // blur_size, thresh, min_area, max_area
    // created_at 用表的 DEFAULT CURRENT_TIMESTAMP，这里不手动写
    const QString sql = QStringLiteral(
        "INSERT INTO inspect_record "
        "(image_path, file_name, result, defect_count, cost_ms, "
        " blur_size, thresh, min_area, max_area) "
        "VALUES "
        "(:image_path, :file_name, :result, :defect_count, :cost_ms, "
        " :blur_size, :thresh, :min_area, :max_area)");

    if (!query.prepare(sql)) {
        m_lastError = QStringLiteral("SQL 准备失败：%1")
                          .arg(query.lastError().text());
        return false;
    }

    // 从完整路径拆出文件名，例如 D:/a/b/patches_1.jpg → patches_1.jpg
    const QString fileName = QFileInfo(result.imagePath).fileName();

    // :名字 和 SQL 里占位符一一对应
    query.bindValue(QStringLiteral(":image_path"), result.imagePath);
    query.bindValue(QStringLiteral(":file_name"), fileName);
    query.bindValue(QStringLiteral(":result"), resultText(result));
    query.bindValue(QStringLiteral(":defect_count"), result.defectCount);
    // cost_ms 表里是 INT；qint64 转 int 对毫秒级耗时足够
    query.bindValue(QStringLiteral(":cost_ms"), static_cast<int>(result.costMs));
    query.bindValue(QStringLiteral(":blur_size"), params.blurSize);
    query.bindValue(QStringLiteral(":thresh"), params.thresh);
    query.bindValue(QStringLiteral(":min_area"), params.minArea);
    query.bindValue(QStringLiteral(":max_area"), params.maxArea);

    if (!query.exec()) {
        m_lastError = QStringLiteral("插入失败：%1")
                          .arg(query.lastError().text());
        return false;
    }

    return true;
}
