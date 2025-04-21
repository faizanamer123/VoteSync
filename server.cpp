#include <boost/asio.hpp>
#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>
#include <iomanip>

using boost::asio::ip::tcp;
using namespace std;

mutex db_mutex;
sqlite3* db;
bool voting_active = true;

void initialize_database() {
    lock_guard<mutex> lock(db_mutex);
    string sql = R"(
        CREATE TABLE IF NOT EXISTS candidates (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL
        );
        CREATE TABLE IF NOT EXISTS votes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            client_id TEXT UNIQUE NOT NULL,
            candidate TEXT NOT NULL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(candidate) REFERENCES candidates(name)
        );
    )";

    char* errmsg;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        cerr << "Error initializing DB: " << errmsg << endl;
        sqlite3_free(errmsg);
    }

    vector<string> candidate_names = {
        "Alice", "Bob", "Charlie", "Diana", "Ethan",
        "Fiona", "George", "Hannah", "Isaac", "Julia"
    };

    string candidate_insert = "INSERT OR IGNORE INTO candidates (name) VALUES ";
    for (size_t i = 0; i < candidate_names.size(); ++i) {
        candidate_insert += "('" + candidate_names[i] + "')";
        if (i < candidate_names.size() - 1) candidate_insert += ", ";
        else candidate_insert += ";";
    }

    if (sqlite3_exec(db, candidate_insert.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        cerr << "Error inserting candidates: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
}

void reset_votes() {
    lock_guard<mutex> lock(db_mutex);
    char* errmsg;
    if (sqlite3_exec(db, "DELETE FROM votes;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
        cerr << "Error resetting votes: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
}

string get_vote_counts() {
    string result = "\nCurrent Vote Counts:\n";
    sqlite3_stmt* stmt;
    string query = R"(
        SELECT candidate, COUNT(*) as votes 
        FROM votes 
        GROUP BY candidate 
        ORDER BY votes DESC
    )";
    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        string candidate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int count = sqlite3_column_int(stmt, 1);
        result += "  " + candidate + ": " + to_string(count) + " votes\n";
    }
    sqlite3_finalize(stmt);
    return result;
}

string process_vote(const string& client_id, const string& candidate) {
    lock_guard<mutex> lock(db_mutex);

    sqlite3_stmt* stmt;
    string query;

    query = "SELECT COUNT(*) FROM candidates WHERE name = ?";
    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, candidate.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0) {
        sqlite3_finalize(stmt);
        return "Invalid candidate.\n";
    }
    sqlite3_finalize(stmt);

    query = "SELECT COUNT(*) FROM votes WHERE client_id = ?";
    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, client_id.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0) {
        sqlite3_finalize(stmt);
        return "You have already voted.\n";
    }
    sqlite3_finalize(stmt);

    query = "INSERT INTO votes (client_id, candidate) VALUES (?, ?)";
    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, client_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, candidate.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return "Error recording vote.\n";
    }
    sqlite3_finalize(stmt);

    return "Vote recorded for: " + candidate + "\n";
}

void declare_winner() {
    lock_guard<mutex> lock(db_mutex);
    cout << "\nVoting session ended. Final Results:\n";
    cout << get_vote_counts();

    sqlite3_stmt* stmt;
    string query = R"(
        SELECT candidate, COUNT(*) as votes 
        FROM votes 
        GROUP BY candidate 
        ORDER BY votes DESC LIMIT 1
    )";
    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        string winner = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int votes = sqlite3_column_int(stmt, 1);
        cout << "\nWinner: " << winner << " with " << votes << " votes!\n";
    }
    else {
        cout << "\nNo votes were cast.\n";
    }
    sqlite3_finalize(stmt);
}

void handle_connection(tcp::socket socket) {
    try {
        boost::asio::streambuf buffer;
        boost::asio::read_until(socket, buffer, "\n");

        istream input(&buffer);
        string client_id, candidate;
        getline(input, client_id, ',');
        getline(input, candidate);

        if (!voting_active) {
            boost::asio::write(socket, boost::asio::buffer("Voting is currently closed.\n"));
            return;
        }

        string response = process_vote(client_id, candidate);
        boost::asio::write(socket, boost::asio::buffer(response));
    }
    catch (...) {
        cerr << "Client handling error.\n";
    }
}

int main() {
    sqlite3_open("voting.db", &db);
    initialize_database();
    reset_votes();

    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 54000));
    acceptor.non_blocking(true);  // Make acceptor non-blocking

    cout << "Voting Server started on port 54000...\nVoting will run for 1 minute...\n";

    auto start_time = chrono::steady_clock::now();
    const auto voting_duration = chrono::minutes(1);

    int last_printed = -1;

    while (chrono::steady_clock::now() - start_time < voting_duration) {
        tcp::socket socket(io_context);
        boost::system::error_code ec;

        // Calculate time left
        auto now = chrono::steady_clock::now();
        int seconds_left = chrono::duration_cast<chrono::seconds>(voting_duration - (now - start_time)).count();

        if (seconds_left != last_printed) {
            cout << "\rTime left: " << setw(3) << seconds_left << "s " << flush;
            last_printed = seconds_left;
        }

        acceptor.accept(socket, ec);

        if (!ec) {
            thread(handle_connection, move(socket)).detach();
        }
        else if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
        else {
            cerr << "\nAccept failed: " << ec.message() << "\n";
        }
    }

    voting_active = false;
    cout << "\rTime left:   0s       \nVoting period over.\n";
    declare_winner();
    sqlite3_close(db);
    return 0;
}
