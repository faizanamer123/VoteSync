#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <thread>

using boost::asio::ip::tcp;
using namespace std;

vector<pair<string, int>> vote_counts; // Vector to store vote and its count
mutex vote_mutex;                      // To ensure thread-safe access

// Function to update vote count
void update_vote(const string& vote) {
    lock_guard<mutex> lock(vote_mutex);

    // Check if the vote already exists
    bool found = false;
    for (int i = 0; i < vote_counts.size(); i++) {
        if (vote_counts[i].first == vote) {
            vote_counts[i].second++;
            found = true;
            break;
        }
    }

    // If vote not found, add new entry
    if (!found) {
        vote_counts.push_back(make_pair(vote, 1));
    }
}

// Function to handle one client connection
void handle_connection(tcp::socket socket) {
    try {
        boost::asio::streambuf buffer;
        boost::asio::read_until(socket, buffer, "\n");

        istream input(&buffer);
        string vote;
        getline(input, vote);

        update_vote(vote);

        // Send acknowledgment to client
        string ack = "Vote received for: " + vote + "\n";
        boost::asio::write(socket, boost::asio::buffer(ack));

        // Print current vote counts
        cout << "Current vote counts:\n";
        lock_guard<mutex> lock(vote_mutex);
        for (int i = 0; i < vote_counts.size(); i++) {
            cout << "  " << vote_counts[i].first << ": " << vote_counts[i].second << "\n";
        }
    }
    catch (...) {
        cerr << "Error handling client.\n";
    }
}

int main() {
    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 54000));

    cout << "Server started on port 54000...\n";

    while (true) {
        tcp::socket socket(io_context);
        acceptor.accept(socket);
        thread(handle_connection, move(socket)).detach();
    }

    return 0;
}
