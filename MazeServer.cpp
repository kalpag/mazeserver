#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <utility>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using namespace std;

// Constants for maze dimensions
const int MAZE_HEIGHT = 7;
const int MAZE_WIDTH = 9;

// Maze with multiple coins ('C'), walls ('#'), and open spaces ('.')
std::vector<std::string> maze = {
    "#########",
    "#P.....C#",
    "#.C###..#",
    "#..C....#",
    "#....####",
    "#C....C.#",
    "#########"
};

// Player's initial position
int playerX = 1, playerY = 1;
// Count remaining coins in the maze
int coinsRemaining = 5;

class MazeServer {
public:
    MazeServer(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        start_accept();
    }

private:
    tcp::acceptor acceptor_;

    void shutdown_server() {
        cout << "Shutting down the server gracefully..." << endl;
        acceptor_.close();  // Stop accepting new connections
        cout << "Server shutdown complete." << endl;
        exit(1);
    }

    void start_accept() {
        auto socket = std::make_shared<tcp::socket>(acceptor_.get_executor());
        acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec) {
            if (!ec) {
                handle_client(socket);
            }
            start_accept();
        });
    }

    void handle_client(std::shared_ptr<tcp::socket> socket) {
        send_maze(socket);  // Send the initial maze to the client
    }

    void send_maze(std::shared_ptr<tcp::socket> socket) {
    string mazeStr = get_maze_string() + "\n";  // Add newline at the end
    boost::asio::async_write(*socket, boost::asio::buffer(mazeStr),
        [this, socket](boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec) {
            read_command(socket);  // Continue reading commands after sending the maze
        }
    });
}


    string get_maze_string() {
        string mazeState = "Player position: (" + to_string(playerX) + "," + to_string(playerY) + ")\n";
        mazeState += "Coins remaining: " + to_string(coinsRemaining) + "\n";
        for (const auto& row : maze) {
            mazeState += row + "\n";
        }
        return mazeState;
    }

    void read_command(std::shared_ptr<tcp::socket> socket) {
        auto buffer = make_shared<boost::asio::streambuf>();
        boost::asio::async_read_until(*socket, *buffer, '\n',
            [this, socket, buffer](boost::system::error_code ec, size_t /*length*/) {
            if (!ec) {
                istream is(buffer.get());
                string command;
                getline(is, command);
                process_command(socket, command);
            }
        });
    }

    void process_command(shared_ptr<tcp::socket> socket, string& command) {
        if (!command.empty() && command.back() == '\n') {
            command.erase(command.size() - 1);  // Trim the newline
        }

        string response;
        if (command == "find") {
            // Find the nearest coin
            std::pair<int, int> nearestCoin = find_nearest_coin();
            if (nearestCoin.first != -1) {
                response = "Nearest coin is at: (" + to_string(nearestCoin.first) + "," + to_string(nearestCoin.second) + ")\n";
            } else {
                response = "No coins remaining!\n";
            }
        } else if (move_player(command)) {
            response = "Moved " + command + ".\n";
            if (coinsRemaining == 0) {
                response += "Victory! You collected all the coins.\n";
            }
        } else {
            response = "Invalid move or command.\n";
        }

        boost::asio::write(*socket, boost::asio::buffer(response));

        // Send updated maze
        send_maze(socket);
    }

    // Function to move the player based on the command
    bool move_player(const string& command) {
        int newX = playerX;
        int newY = playerY;

       // int prevX = playerX;
       // int prevY = playerY;

        cout << "Current position: (" << playerX << ", " << playerY << "), Command: " << command << endl;

        if (command == "kill") {
            shutdown_server();
        }
        if (command == "W" && playerX > 0 && maze[playerX - 1][playerY] != '#') {
            newX--;
        } else if (command == "S" && playerX < MAZE_HEIGHT - 1 && maze[playerX + 1][playerY] != '#') {
            newX++;
        } else if (command == "A" && playerY > 0 && maze[playerX][playerY - 1] != '#') {
            newY--;
        } else if (command == "D" && playerY < MAZE_WIDTH - 1 && maze[playerX][playerY + 1] != '#') {
            newY++;
        } else {
            return false;  // Invalid move (e.g., hitting a wall or invalid command)
        }

        // Update player's position on the maze
        maze[playerX][playerY] = '.';  // Mark the old position as an open space
       // maze[prevX][prevY] = 'X';  // Optional: mark previous position with 'X'

        playerX = newX;
        playerY = newY;

        // If the player collects a coin
        if (maze[playerX][playerY] == 'C') {
            coinsRemaining--;
            cout << "Coin collected! Coins remaining: " << coinsRemaining << endl;
        }
        maze[playerX][playerY] = 'P';  // Mark the new position of the player

        cout << "New position: (" << playerX << ", " << playerY << "), Coins remaining: " << coinsRemaining << endl;

        return true;
    }

    // Function to find the nearest coin using BFS
    std::pair<int, int> find_nearest_coin() {
        std::queue<std::pair<int, int>> queue;
        std::vector<std::vector<bool>> visited(MAZE_HEIGHT, std::vector<bool>(MAZE_WIDTH, false));

        queue.push({playerX, playerY});
        visited[playerX][playerY] = true;

        // Directions for moving in the maze
        std::vector<std::pair<int, int>> directions = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        
        while (!queue.empty()) {
            auto [x, y] = queue.front();
            queue.pop();

            // If a coin is found, return its position
            if (maze[x][y] == 'C') {
                return {x, y};
            }

            // Explore neighbors
            for (const auto& [dx, dy] : directions) {
                int nx = x + dx;
                int ny = y + dy;

                if (nx >= 0 && nx < MAZE_HEIGHT && ny >= 0 && ny < MAZE_WIDTH && !visited[nx][ny] && maze[nx][ny] != '#') {
                    visited[nx][ny] = true;
                    queue.push({nx, ny});
                }
            }
        }

        return {-1, -1};  // Return invalid coordinates if no coin is found
    }
};

int main() {
    try {
        cout << "Kalpa Maze Server is running..." << endl;
        boost::asio::io_context io_context;
        MazeServer server(io_context, 12345);  // Start the server on port 12345
        io_context.run();  // Run the server loop
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }
}
