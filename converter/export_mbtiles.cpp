#include "webtiles/reader.h"

#include "absl/cleanup/cleanup.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "sqlite3.h"

#include <filesystem>
#include <string>

namespace wt = webtiles;

ABSL_FLAG(std::string, input_path, "", "Input file path");
ABSL_FLAG(std::string, output_path, "", "Output file path");

int main(int argc, char** argv)
{
    absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();

    std::string inputPath = absl::GetFlag(FLAGS_input_path);
    std::string outputPath = absl::GetFlag(FLAGS_output_path);
    CHECK(!inputPath.empty() && std::filesystem::exists(inputPath));
    CHECK(!outputPath.empty() && !std::filesystem::exists(outputPath));

    LOG(INFO) << "Creating reader...";

    auto reader = wt::createFileReader(inputPath);

    LOG(INFO) << "Creating output file...";

    sqlite3* db = nullptr;
    absl::Cleanup dbClose = [&db]() { sqlite3_close(db); };

    int openFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (sqlite3_open_v2(outputPath.c_str(), &db, openFlags, nullptr) != SQLITE_OK) {
        LOG(ERROR) << sqlite3_errmsg(db);
        return 1;
    }

    char* errMsg = nullptr;
    absl::Cleanup freeErrMsg = [&errMsg]() { sqlite3_free(errMsg); };

    const auto CREATE_QUERY = R"(
CREATE TABLE metadata (name TEXT, value TEXT);
CREATE TABLE tiles (zoom_level INTEGER, tile_column INTEGER, tile_row INTEGER, tile_data BLOB);
CREATE UNIQUE INDEX tile_index ON tiles (zoom_level, tile_column, tile_row);
)";
    if (sqlite3_exec(db, CREATE_QUERY, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        LOG(ERROR) << errMsg;
        return 1;
    }
    if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        LOG(ERROR) << errMsg;
        return 1;
    }

    absl::Cleanup rollback = [&db]() { sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr); };

    sqlite3_stmt* metadataStmt = nullptr;
    absl::Cleanup metadataStmtFinalize = [&metadataStmt]() { sqlite3_finalize(metadataStmt); };

    const auto METADATA_QUERY = "INSERT INTO metadata (name, value) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db, METADATA_QUERY, -1, &metadataStmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << sqlite3_errmsg(db);
        return 1;
    }

    // TODO(eak1mov): customize
    sqlite3_bind_text(metadataStmt, 1, "name", -1, SQLITE_STATIC);
    sqlite3_bind_text(metadataStmt, 2, "dataset_name", -1, SQLITE_STATIC);
    sqlite3_step(metadataStmt);
    sqlite3_reset(metadataStmt);

    // TODO(eak1mov): customize
    sqlite3_bind_text(metadataStmt, 1, "format", -1, SQLITE_STATIC);
    sqlite3_bind_text(metadataStmt, 2, "png", -1, SQLITE_STATIC);
    sqlite3_step(metadataStmt);
    sqlite3_reset(metadataStmt);

    sqlite3_finalize(metadataStmt);
    metadataStmt = nullptr;

    sqlite3_stmt* tileStmt = nullptr;
    absl::Cleanup tileStmtFinalize = [&tileStmt]() { sqlite3_finalize(tileStmt); };

    const auto TILE_QUERY =
        "INSERT INTO tiles (zoom_level, tile_column, tile_row, tile_data) VALUES (?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, TILE_QUERY, -1, &tileStmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << sqlite3_errmsg(db);
        return 1;
    }

    LOG(INFO) << "Writing tiles...";

    for (const auto& [tileId, tileData] : reader->allTiles()) {
        uint32_t newY = (1 << tileId.z) - 1 - tileId.y; // fix mapbox tiling scheme (TMS -> XYZ)

        sqlite3_bind_int(tileStmt, 1, tileId.z);
        sqlite3_bind_int(tileStmt, 2, tileId.x);
        sqlite3_bind_int(tileStmt, 3, newY);
        sqlite3_bind_blob(tileStmt, 4, tileData.data(), tileData.size(), SQLITE_TRANSIENT);

        if (sqlite3_step(tileStmt) != SQLITE_DONE) {
            LOG(ERROR) << sqlite3_errmsg(db);
            return 1;
        }
        sqlite3_reset(tileStmt);

        LOG_EVERY_N_SEC(INFO, 10) << "Processed " << COUNTER << " tiles";
    }

    sqlite3_finalize(tileStmt);
    tileStmt = nullptr;

    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        LOG(ERROR) << errMsg;
        return 1;
    }

    std::move(rollback).Cancel();

    sqlite3_close(db);
    db = nullptr;

    LOG(INFO) << "Done!";

    return 0;
}
