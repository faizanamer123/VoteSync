#include <boost/asio.hpp>
#include <iostream>
using namespace std;

using boost::asio::ip::tcp;

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        boost::asio::connect(socket, resolver.resolve("127.0.0.1", "54000"));

        string vote;
        cout << "Enter your vote: ";
        cin >> vote;
        vote += "\n";

        boost::asio::write(socket, boost::asio::buffer(vote));

        boost::asio::streambuf response;
        boost::asio::read_until(socket, response, "\n");

        istream input(&response);
        string reply;
        getline(input, reply);
        cout << reply << std::endl;

    }
    catch (std::exception& e) {
        cerr << "Client error: " << e.what() << "\n";
    }

    return 0;
}
