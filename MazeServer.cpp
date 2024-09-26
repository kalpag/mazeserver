#include <iostream>
#include <vector>
#include <map>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <boost/asio.hpp>      // For Boost.Asio networking

using boost::asio::ip::tcp;

const int MAZE_SIZE = 5;

class MazeServer {
public:
    MazeServer(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), io_context_(io_context) {
        createMaze();
        startAccept();
    }

private:
    void createMaze() {
        std::srand(std::time(0));
        maze_.resize(MAZE_SIZE, std::vector<char>(MAZE_SIZE, '.'));
        for (int i = 0; i < MAZE_SIZE; ++i) {
            for (int j = 0; j < MAZE_SIZE; ++j) {
                if (std::rand() % 5 == 0) {
                    maze_[i][j] = 'C';  // Place a coin
                }
            }
        }
        playerPos_ = {0, 0};  // Start player at the top-left corner
    }

    void startAccept() {
        // Create a new socket using the io_context directly
        auto socket = std::make_shared<tcp::socket>(io_context_);
        acceptor_.async_accept(*socket,
            [this, socket](const boost::system::error_code& error) {
                handleAccept(socket, error);
            });
    }

    void handleAccept(std::shared_ptr<tcp::socket> socket, const boost::system::error_code& error) {
        if (!error) {
            std::string welcomeMessage = "Welcome to the Maze! Move with WASD.\n";
            boost::asio::write(*socket, boost::asio::buffer(welcomeMessage));
            startRead(socket);
            startAccept();
        } else {
            std::cerr << "Accept error: " << error.message() << std::endl;
        }
    }

    void startRead(std::shared_ptr<tcp::socket> socket) {
        auto buffer = std::make_shared<std::vector<char>>(1);  // Read 1 char for movement
        socket->async_read_some(boost::asio::buffer(*buffer),
            [this, socket, buffer](boost::system::error_code ec, std::size_t length) {
                if (!ec && length > 0) {
                    processMove((*buffer)[0], socket);
                    startRead(socket);  // Keep reading for further moves
                }
            });
    }

    void processMove(char direction, std::shared_ptr<tcp::socket> socket) {
        std::map<char, std::pair<int, int>> moves = {
            {'W', {-1, 0}}, {'A', {0, -1}}, {'S', {1, 0}}, {'D', {0, 1}}
        };

        if (moves.find(direction) != moves.end()) {
            // Update player position
            playerPos_.first += moves[direction].first;
            playerPos_.second += moves[direction].second;

            // Ensure player stays within bounds
            playerPos_.first = std::clamp(playerPos_.first, 0, MAZE_SIZE - 1);
            playerPos_.second = std::clamp(playerPos_.second, 0, MAZE_SIZE - 1);

            // Check if player collected a coin
            if (maze_[playerPos_.first][playerPos_.second] == 'C') {
                maze_[playerPos_.first][playerPos_.second] = '.';  // Remove the coin
                std::string message = "You collected a coin!\n";
                boost::asio::write(*socket, boost::asio::buffer(message));
            } else {
                std::string message = "Moved!\n";
                boost::asio::write(*socket, boost::asio::buffer(message));
            }
        } else {
            std::string message = "Invalid move! Use WASD to move.\n";
            boost::asio::write(*socket, boost::asio::buffer(message));
        }
    }

    std::vector<std::vector<char>> maze_;
    std::pair<int, int> playerPos_;  // Track player position
    tcp::acceptor acceptor_;  // For accepting client connections
    boost::asio::io_context& io_context_;  // Reference to the io_context
};

int main() {
    try {
        boost::asio::io_context io_context;
        MazeServer server(io_context, 1234);
        io_context.run();  // Run the I/O context to start the server
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
