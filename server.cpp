#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/config.hpp>
#include <sqlite3.h>
#include <iostream>
#include <string>
#include <thread>
#include <map>
#include <set>
#include <mutex>
#include <chrono>
#include <sstream>

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;
using namespace std;
using namespace chrono;

map<string, string> votes; // client_id -> candidate
set<string> assigned_ids;
recursive_mutex mtx;           // General mutex (for votes, clients)
recursive_mutex db_mutex;      // NEW separate mutex for database access

bool voting_started = false;
bool voting_ended = false;
steady_clock::time_point voting_end_time;
int voting_duration_minutes = 0;

// SQLite database
sqlite3* db;

// XOR Encryption key
const char XOR_KEY = 'K';

// Simple XOR encryption and decryption
string encrypt(const string& text) {
    string enc = text;
    for (char& c : enc) c ^= XOR_KEY;
    return enc;
}

string decrypt(const string& enc) {
    return encrypt(enc);
}

void initializeDatabase() {
    if (sqlite3_open("votes.db", &db)) {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        exit(1);
    }
    const char* sql = "CREATE TABLE IF NOT EXISTS votes (client_id TEXT PRIMARY KEY, candidate TEXT);";
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
}

void saveVote(const string& client_id, const string& candidate) {
    lock_guard<recursive_mutex> db_lock(db_mutex);  //  DB lock

    string enc_candidate = encrypt(candidate);
    string sql = "INSERT INTO votes (client_id, candidate) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, client_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, enc_candidate.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        cerr << "Error saving vote." << endl;
    }
    sqlite3_finalize(stmt);
}

map<string, int> countVotes() {
    lock_guard<recursive_mutex> db_lock(db_mutex);  // 🔥 DB lock

    map<string, int> results;
    string sql = "SELECT candidate FROM votes;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        string enc_candidate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        string candidate = decrypt(enc_candidate);
        results[candidate]++;
    }
    sqlite3_finalize(stmt);
    return results;
}

string winner() {
    auto results = countVotes();
    string final_winner;
    int max_votes = 0;
    vector<string> tied_candidates;

    for (auto& [cand, c] : results) {
        if (c > max_votes) {
            max_votes = c;
            tied_candidates.clear();
            tied_candidates.push_back(cand);
        }
        else if (c == max_votes) {
            tied_candidates.push_back(cand);
        }
    }

    if (tied_candidates.size() == 1) {
        return tied_candidates[0];
    }
    else if (tied_candidates.size() > 1) {
        stringstream ss;
        ss << "Tie between ";
        for (size_t i = 0; i < tied_candidates.size(); ++i) {
            ss << tied_candidates[i];
            if (i != tied_candidates.size() - 1) {
                ss << " and ";
            }
        }
        return ss.str();
    }

    return "No votes";
}


string generate_client_id() {
    static int counter = 1;
    lock_guard<recursive_mutex> lock(mtx);
    string id = "client_" + to_string(counter++);
    assigned_ids.insert(id);
    return id;
}

void set_cors_headers(http::response<http::string_body>& res) {
    res.set(http::field::access_control_allow_origin, "*");
    res.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
    res.set(http::field::access_control_allow_headers, "Content-Type");
}

void handle_request(http::request<http::string_body> req, http::response<http::string_body>& res) {
    if (req.method() == http::verb::options) {
        res.version(req.version());
        res.result(http::status::ok);
        set_cors_headers(res);
        res.prepare_payload();
        return;
    }

    if (req.method() == http::verb::get && req.target() == "/connect") {
        string client_id = generate_client_id();

        res.version(req.version());
        res.result(http::status::ok);
        set_cors_headers(res);
        res.set(http::field::content_type, "text/plain");
        res.body() = client_id;
        res.prepare_payload();
        return;
    }
    else if (req.method() == http::verb::get && req.target() == "/status") {
        stringstream status;
        lock_guard<recursive_mutex> lock(mtx);
        if (!voting_started) {
            status << "waiting";
        }
        else if (voting_started && !voting_ended) {
            status << "voting";
            auto now = steady_clock::now();
            auto seconds_left = duration_cast<seconds>(voting_end_time - now).count();
            if (seconds_left < 0) seconds_left = 0;
            status << "|" << seconds_left;
        }
        else if (voting_ended) {
            status << "ended";
        }

        res.version(req.version());
        res.result(http::status::ok);
        set_cors_headers(res);
        res.set(http::field::content_type, "text/plain");
        res.body() = status.str();
        res.prepare_payload();
        return;
    }
    else if (req.method() == http::verb::post && req.target() == "/vote") {
        auto body = req.body();
        size_t pos = body.find('&');
        if (pos != string::npos) {
            string client_id = body.substr(0, pos);
            string candidate = body.substr(pos + 1);

            if (candidate.find('=') != string::npos)
                candidate = candidate.substr(candidate.find('=') + 1);

            {
                lock_guard<recursive_mutex> lock(mtx);
                if (votes.count(client_id)) {
                    res.version(req.version());
                    res.result(http::status::forbidden);
                    set_cors_headers(res);
                    res.set(http::field::content_type, "text/plain");
                    res.body() = "Already voted!";
                    res.prepare_payload();
                    return;
                }
                else {
                    votes[client_id] = candidate;
                    saveVote(client_id, candidate);
                }
            }

            res.version(req.version());
            res.result(http::status::ok);
            set_cors_headers(res);
            res.set(http::field::content_type, "text/plain");
            res.body() = "Vote recorded!";
            res.prepare_payload();
            return;
        }
    }
    else if (req.method() == http::verb::get && req.target() == "/results") {
        auto results = countVotes();

        stringstream body;
        body << "{ \"winner\": \"" << winner() << "\", \"votes\": {";

        bool first = true;
        for (auto& [cand, c] : results) {
            if (!first) body << ",";
            body << "\"" << cand << "\":" << c;
            first = false;
        }
        body << "} }";

        res.version(req.version());
        res.result(http::status::ok);
        set_cors_headers(res);
        res.set(http::field::content_type, "application/json");
        res.body() = body.str();
        res.prepare_payload();
        return;
    }
    else if (req.method() == http::verb::get && req.target() == "/stats") {
        auto results = countVotes();

        stringstream body;
        body << "{ \"votes\": {";

        bool first = true;
        for (auto& [cand, c] : results) {
            if (!first) body << ",";
            body << "\"" << cand << "\":" << c;
            first = false;
        }
        body << "} }";

        res.version(req.version());
        res.result(http::status::ok);
        set_cors_headers(res);
        res.set(http::field::content_type, "application/json");
        res.body() = body.str();
        res.prepare_payload();
        return;
    }

    res = http::response<http::string_body>(http::status::not_found, req.version());
    res.set(http::field::server, "VoteSync-HTTP-Server");
    set_cors_headers(res);
    res.set(http::field::content_type, "text/plain");
    res.body() = "Not Found";
    res.prepare_payload();
}

void do_session(tcp::socket socket) {
    try {
        boost::beast::flat_buffer buffer;

        while (true) {
            http::request<http::string_body> req;
            http::read(socket, buffer, req);

            http::response<http::string_body> res;
            handle_request(req, res);

            http::write(socket, res);
        }
    }
    catch (exception& e) {
        cerr << "Session error: " << e.what() << endl;
    }
}

void voting_timer() {
    while (!voting_started) this_thread::sleep_for(chrono::seconds(1));

    voting_end_time = steady_clock::now() + minutes(voting_duration_minutes);

    while (steady_clock::now() < voting_end_time) {
        this_thread::sleep_for(chrono::seconds(1));
    }

    voting_ended = true;

    cout << "\nVoting has ended!\n";
    cout << "Final Votes:\n";
    auto results = countVotes();
    for (auto& [cand, c] : results) {
        cout << cand << " - " << c << " votes\n";
    }
    cout << "Winner: " << winner() << endl;
}

void clearVotes() {
    lock_guard<recursive_mutex> db_lock(db_mutex);

    const char* sql = "DELETE FROM votes;";
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        cerr << "Error clearing old votes: " << errMsg << endl;
        sqlite3_free(errMsg);
    }

    lock_guard<recursive_mutex> lock(mtx);
    votes.clear(); // also clear in-memory votes map
}


int main() {
    try {
        initializeDatabase();

        boost::asio::io_context io_context;

        cout << "Start voting session? (y/n): ";
        string start;
        cin >> start;

        if (start == "y" || start == "Y") {
            clearVotes(); //  Clear database and in-memory votes
            cout << "Enter voting duration (minutes): ";
            cin >> voting_duration_minutes;
            voting_started = true;
            thread(voting_timer).detach();
        }

        tcp::acceptor acceptor(io_context, { tcp::v4(), 8080 });

        cout << "HTTP Voting Server started at http://127.0.0.1:8080\n";

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            thread(do_session, move(socket)).detach();
        }
    }
    catch (exception& e) {
        cerr << "Server error: " << e.what() << endl;
    }

    sqlite3_close(db);

    return 0;
}
