#include "database.h"
#include <iostream>

Database::Database() : db_(nullptr) {}

Database::~Database()
{
    close();
}

bool Database::open(const std::string& dbFile)
{
    if (sqlite3_open(dbFile.c_str(), &db_) != SQLITE_OK)
    {
        std::cerr << "Open database failed: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    return true;
}

void Database::close()
{
    if (db_)
    {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::createTable()
{
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS eye_monitor_records
        (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            eye_use_period TEXT,
            continuous_use_seconds INTEGER DEFAULT 0,

            eye_side TEXT,
            eye_image_path TEXT,
            face_image_path TEXT,

            eye_pos_x REAL,
            eye_pos_y REAL,

            gaze_x REAL,
            gaze_y REAL,

            eye_screen_distance_cm REAL,
            gaze_angle_deg REAL,

            pitch_deg REAL,
            roll_deg REAL,
            yaw_deg REAL
        );
    )";

    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);

    if (rc != SQLITE_OK)
    {
        std::cerr << "Create table failed: "
                  << (err ? err : "unknown") << '\n';
        sqlite3_free(err);
        return false;
    }

    return true;
}

bool Database::insertRecord(const EyeRecord& r)
{
    const char* sql = R"(
        INSERT INTO eye_monitor_records
        (
            timestamp,
            eye_use_period,
            continuous_use_seconds,
            eye_side,
            eye_image_path,
            face_image_path,
            eye_pos_x,
            eye_pos_y,
            gaze_x,
            gaze_y,
            eye_screen_distance_cm,
            gaze_angle_deg,
            pitch_deg,
            roll_deg,
            yaw_deg
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, r.timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, r.eyeUsePeriod.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, r.continuousUseSeconds);

    sqlite3_bind_text(stmt, 4, r.eyeSide.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, r.eyeImagePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, r.faceImagePath.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_double(stmt, 7, r.eyePosX);
    sqlite3_bind_double(stmt, 8, r.eyePosY);

    sqlite3_bind_double(stmt, 9, r.gazeX);
    sqlite3_bind_double(stmt, 10, r.gazeY);

    sqlite3_bind_double(stmt, 11, r.eyeScreenDistanceCm);
    sqlite3_bind_double(stmt, 12, r.gazeAngleDeg);

    sqlite3_bind_double(stmt, 13, r.pitchDeg);
    sqlite3_bind_double(stmt, 14, r.rollDeg);
    sqlite3_bind_double(stmt, 15, r.yawDeg);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    return ok;
}

bool Database::deleteRecord(int id)
{
    const char* sql =
        "DELETE FROM eye_monitor_records WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int(stmt, 1, id);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    return ok;
}

bool Database::getRecordById(int id, EyeRecord& r)
{
    const char* sql = R"(
        SELECT
            id,
            timestamp,
            eye_use_period,
            continuous_use_seconds,
            eye_side,
            eye_image_path,
            face_image_path,
            eye_pos_x,
            eye_pos_y,
            gaze_x,
            gaze_y,
            eye_screen_distance_cm,
            gaze_angle_deg,
            pitch_deg,
            roll_deg,
            yaw_deg
        FROM eye_monitor_records
        WHERE id = ?;
    )";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return false;
    }

    r.id = sqlite3_column_int(stmt, 0);
    r.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    r.eyeUsePeriod = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    r.continuousUseSeconds = sqlite3_column_int(stmt, 3);

    r.eyeSide = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    r.eyeImagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    r.faceImagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));

    r.eyePosX = sqlite3_column_double(stmt, 7);
    r.eyePosY = sqlite3_column_double(stmt, 8);

    r.gazeX = sqlite3_column_double(stmt, 9);
    r.gazeY = sqlite3_column_double(stmt, 10);

    r.eyeScreenDistanceCm = sqlite3_column_double(stmt, 11);
    r.gazeAngleDeg = sqlite3_column_double(stmt, 12);

    r.pitchDeg = sqlite3_column_double(stmt, 13);
    r.rollDeg = sqlite3_column_double(stmt, 14);
    r.yawDeg = sqlite3_column_double(stmt, 15);

    sqlite3_finalize(stmt);
    return true;
}

std::vector<EyeRecord> Database::getAllRecords()
{
    std::vector<EyeRecord> records;

    const char* sql = R"(
        SELECT
            id,
            timestamp,
            eye_use_period,
            continuous_use_seconds,
            eye_side,
            eye_image_path,
            face_image_path,
            eye_pos_x,
            eye_pos_y,
            gaze_x,
            gaze_y,
            eye_screen_distance_cm,
            gaze_angle_deg,
            pitch_deg,
            roll_deg,
            yaw_deg
        FROM eye_monitor_records
        ORDER BY id DESC;
    )";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return records;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        EyeRecord r;

        r.id = sqlite3_column_int(stmt, 0);
        r.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.eyeUsePeriod = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.continuousUseSeconds = sqlite3_column_int(stmt, 3);

        r.eyeSide = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        r.eyeImagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        r.faceImagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));

        r.eyePosX = sqlite3_column_double(stmt, 7);
        r.eyePosY = sqlite3_column_double(stmt, 8);

        r.gazeX = sqlite3_column_double(stmt, 9);
        r.gazeY = sqlite3_column_double(stmt, 10);

        r.eyeScreenDistanceCm = sqlite3_column_double(stmt, 11);
        r.gazeAngleDeg = sqlite3_column_double(stmt, 12);

        r.pitchDeg = sqlite3_column_double(stmt, 13);
        r.rollDeg = sqlite3_column_double(stmt, 14);
        r.yawDeg = sqlite3_column_double(stmt, 15);

        records.push_back(r);
    }

    sqlite3_finalize(stmt);
    return records;
}
