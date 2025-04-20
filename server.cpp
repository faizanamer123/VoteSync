#include <boost/asio.hpp>
#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>

using boost::asio::ip::tcp;
using namespace std;

mutex db_mutex;
sqlite3* db;
bool voting_active = true;

// Initialize the database
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

    sql = R"(
        INSERT OR IGNORE INTO candidates (name) VALUES 
        ('Alice'), 
        ('Bob'), 
        ('Charlie');
    )";

    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        cerr << "Error inserting candidates: " << errmsg << endl;
        sqlite3_free(errmsg);
    }
}

// Return current vote counts as a string
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

// Process a vote
string process_vote(const string& client_id, const string& candidate) {
    lock_guard<mutex> lock(db_mutex);

    sqlite3_stmt* stmt;
    string query;

    // Check valid candidate
    query = "SELECT COUNT(*) FROM candidates WHERE name = ?";
    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, candidate.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0) {
        sqlite3_finalize(stmt);
        return "Invalid candidate.\n";
    }
    sqlite3_finalize(stmt);

    // Check if already voted
    query = "SELECT COUNT(*) FROM votes WHERE client_id = ?";
    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, client_id.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0) {
        sqlite3_finalize(stmt);
        return "You have already voted.\n";
    }
    sqlite3_finalize(stmt);

    // Record vote
    query = "INSERT INTO votes (client_id, candidate) VALUES (?, ?)";
    sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, client_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, candidate.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return "Error recording vote.\n";
    }
    sqlite3_finalize(stmt);

    return "Vote recorded successfully for: " + candidate + "\n" + get_vote_counts();
}

// Declare the winner
void declare_winner() {
    lock_guard<mutex> lock(db_mutex);
    cout << "\nVoting has ended. Final Results:\n";
    string results = get_vote_counts();
    cout << results;

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
    voting_active = false;
}

// Handle client connection
void handle_connection(tcp::socket socket) {
    try {
        boost::asio::streambuf buffer;
        boost::asio::read_until(socket, buffer, "\n");

        istream input(&buffer);
        string client_id, candidate;
        getline(input, client_id, ',');
        getline(input, candidate);

        if (!voting_active) {
            boost::asio::write(socket, boost::asio::buffer("Voting has ended.\n"));
            return;
        }

        string response = process_vote(client_id, candidate);
        boost::asio::write(socket, boost::asio::buffer(response));
    }
    catch (...) {
        cerr << "Error handling client connection.\n";
    }
}

int main() {
    sqlite3_open("voting.db", &db);
    initialize_database();

    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 54000));

    cout << "Server started on port 54000...\n";

    // End voting in 2 minutes
    thread([] {
        this_thread::sleep_for(chrono::minutes(2));
        declare_winner();
        }).detach();

        while (voting_active) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            thread(handle_connection, move(socket)).detach();
        }

        sqlite3_close(db);
        return 0;
}


