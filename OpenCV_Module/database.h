#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include "sqlite3.h"

struct EyeRecord
{
    int id = 0;

    std::string timestamp;
    std::string eyeUsePeriod;
    int continuousUseSeconds = 0;

    std::string eyeSide;
    std::string eyeImagePath;
    std::string faceImagePath;

    double eyePosX = 0.0;
    double eyePosY = 0.0;

    double gazeX = 0.0;
    double gazeY = 0.0;

    double eyeScreenDistanceCm = 0.0;
    double gazeAngleDeg = 0.0;

    double pitchDeg = 0.0;
    double rollDeg = 0.0;
    double yawDeg = 0.0;
};

class Database
{
public:
    Database();
    ~Database();

    bool open(const std::string& dbFile);
    void close();
    bool createTable();

    bool insertRecord(const EyeRecord& record);
    bool deleteRecord(int id);
    bool getRecordById(int id, EyeRecord& record);
    std::vector<EyeRecord> getAllRecords();

private:
    sqlite3* db_;
};

#endif
