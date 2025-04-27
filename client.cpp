#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>

using boost::asio::ip::tcp;
using namespace std;
namespace fs = std::filesystem;

string get_or_create_client_id() {
    string filename = "client_id.txt";
    ifstream in(filename);
    string id;
    if (in >> id) return id;

    id = "Client_" + to_string(rand() % 10000);
    ofstream out(filename);
    out << id;
    return id;
}

string wait_for_vote() {
    string vote_file = "vote.txt";
    while (!fs::exists(vote_file)) {
        cout << "Waiting for your vote...\n";
        this_thread::sleep_for(chrono::seconds(1));
    }

    ifstream in(vote_file);
    string candidate;
    getline(in, candidate);
    return candidate;
}

void open_browser() {
    system("start index.html"); // Windows browser launch
}

int main() {
    try {
        string client_id = get_or_create_client_id();

        open_browser();

        string candidate = wait_for_vote();
        cout << "You selected: " << candidate << endl;

        boost::asio::io_context io_context;
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        boost::asio::connect(socket, resolver.resolve("127.0.0.1", "54000"));

        string message = client_id + "," + candidate + "\n";
        boost::asio::write(socket, boost::asio::buffer(message));

        boost::asio::streambuf response;
        boost::asio::read_until(socket, response, "\n");

        istream input(&response);
        string line;
        while (getline(input, line)) {
            cout << line << endl;
        }
    }
    catch (exception& e) {
        cerr << "Client error: " << e.what() << "\n";
    }

    return 0;
}
