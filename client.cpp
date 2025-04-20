#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>

using boost::asio::ip::tcp;
using namespace std;

// Generate and persist a unique client ID
string get_or_create_client_id() {
    ifstream in("client_id.txt");
    string id;
    if (in >> id) return id;

    // Generate new ID
    id = "Client_" + to_string(chrono::system_clock::now().time_since_epoch().count());
    ofstream out("client_id.txt");
    out << id;
    return id;
}

int main() {
    try {
        string client_id = get_or_create_client_id();

        boost::asio::io_context io_context;
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        boost::asio::connect(socket, resolver.resolve("127.0.0.1", "54000"));

        cout << "Connected to server.\n";
        cout << "Your client ID: " << client_id << "\n";

        // Ask user input
        cout << "Candidates:\n  Alice\n  Bob\n  Charlie\n";
        string candidate;
        cout << "Enter candidate to vote for: ";
        getline(cin, candidate);

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
