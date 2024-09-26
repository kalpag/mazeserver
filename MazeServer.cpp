#include <iostream>
#include <boost/asio.hpp>
#include <vector>
#include <string>
#include <queue>
#include <unordered_set>

using boost::asio::ip::tcp;

// Constants for maze dimensions
const int MAZE_HEIGHT = 7;
const int MAZE_WIDTH = 9;

// Maze with multiple coins ('C'), walls ('#'), and open spaces ('.')
std::vector<std::string> maze = {
    "#########",
    "#C.....C#",
    "#.C###..#",
    "#..C....#",
    "#....###C",
    "#C......#",
    "#########"
};

// Player's initial position
int playerX = 1, playerY = 1;

// Count remaining coins in the maze
int coinsRemaining = 5;

// Direction vectors for moving up, down, left, right
const std::vector<std::pair<int, int>> directions = {
    {-1, 0}, // Up
    {1, 0},  // Down
    {0, -1}, // Left
    {0, 1}   // Right
};

class MazeServer {
public:
    MazeServer(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        start_accept();
    }

private:
    void shutdown_server() {
        std::cout << "Shutting down the server gracefully..." << std::endl;
        acceptor_.close(); // Stop accepting new connections
        std::cout << "Server shutdown complete." << std::endl;
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
        send_maze(socket); // Send the initial maze to the client
        read_command(socket); // Start reading commands from the client
    }

    void send_maze(std::shared_ptr<tcp::socket> socket) {
        std::string mazeStr = get_maze_string();
        boost::asio::async_write(*socket, boost::asio::buffer(mazeStr),
            [this, socket](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                read_command(socket); // Continue reading commands after sending the maze
            }
        });
    }

    std::string get_maze_string() {
        std::string mazeState = "Player position: (" + std::to_string(playerX) + "," + std::to_string(playerY) + ")\n";
        mazeState += "Coins remaining: " + std::to_string(coinsRemaining) + "\n";
        for (const auto& row : maze) {
            mazeState += row + "\n";
        }
        return mazeState;
    }

    void read_command(std::shared_ptr<tcp::socket> socket) {
        auto buffer = std::make_shared<boost::asio::streambuf>();
        boost::asio::async_read_until(*socket, *buffer, '\n',
            [this, socket, buffer](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                std::istream is(buffer.get());
                std::string command;
                std::getline(is, command);
                process_command(socket, command);
            }
        });
    }

    void process_command(std::shared_ptr<tcp::socket> socket,std::string& command) {
        std::cout << "Client command: " << command << std::endl;

        // Remove any newline characters at the end of the command
        if (!command.empty() && command.back() == '\n') {
            command.erase(command.size() - 1); // Trim the newline
        }

        std::string response;
        
        if(command=="kill")
            shutdown_server();
        
        if (command == "path") {
            auto path = find_shortest_path_to_coin(); // Find the shortest path
            highlight_path(path); // Highlight the path in the maze
            response = "Path to nearest coin found and highlighted.\n";
        } else if (move_player(command)) {
            response = "Moved " + command + ".\n";
            if (coinsRemaining == 0) {
                response += "Victory! You collected all the coins.\n";
            }
        } else {
            response = "Invalid move or command.\n";
        }

        boost::asio::async_write(*socket, boost::asio::buffer(response),
            [this, socket](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                send_maze(socket); // Send the updated maze after processing the command
            }
        });
    }

    bool move_player(const std::string& command) {
        int newX = playerX;
        int newY = playerY;

        if (command == "W" && playerX > 0 && maze[playerX - 1][playerY] != '#') {
            newX--;
        } else if (command == "S" && playerX < MAZE_HEIGHT - 1 && maze[playerX + 1][playerY] != '#') {
            newX++;
        } else if (command == "A" && playerY > 0 && maze[playerX][playerY - 1] != '#') {
            newY--;
        } else if (command == "D" && playerY < MAZE_WIDTH - 1 && maze[playerX][playerY + 1] != '#') {
            newY++;
        } else {
            return false; // Invalid move (e.g., hitting a wall or invalid command)
        }

        // Update player's position on the maze
        maze[playerX][playerY] = '.'; // Mark the old position as an open space
        playerX = newX;
        playerY = newY;

        // If the player collects a coin
        if (maze[playerX][playerY] == 'C') {
            coinsRemaining--;
        }
        maze[playerX][playerY] = 'P'; // Mark the new position of the player

        return true;
    }

    std::vector<std::pair<int, int>> find_shortest_path_to_coin() {
        std::queue<std::pair<int, int>> q;
        std::unordered_set<std::string> visited;
        std::vector<std::pair<int, int>> path;
        std::pair<int, int> nearestCoin = {-1, -1};

        q.push({playerX, playerY});
        visited.insert(std::to_string(playerX) + "," + std::to_string(playerY));

        while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();

            // Check for nearest coin
            if (maze[x][y] == 'C') {
                nearestCoin = {x, y};
                break; // Stop searching after finding the first coin
            }

            // Explore neighbors
            for (const auto& [dx, dy] : directions) {
                int newX = x + dx;
                int newY = y + dy;

                if (newX >= 0 && newX < MAZE_HEIGHT && newY >= 0 && newY < MAZE_WIDTH &&
                    maze[newX][newY] != '#' && visited.find(std::to_string(newX) + "," + std::to_string(newY)) == visited.end()) {
                    q.push({newX, newY});
                    visited.insert(std::to_string(newX) + "," + std::to_string(newY));
                }
            }
        }

        // If a nearest coin was found, backtrack to create the path
        if (nearestCoin.first != -1) {
            path.push_back(nearestCoin);
            // Optionally implement a backtracking path here or just return the coordinates
        }

        return path;
    }

    void highlight_path(const std::vector<std::pair<int, int>>& path) {
        for (const auto& [x, y] : path) {
            if (maze[x][y] == 'C') {
                maze[x][y] = '*'; // Highlight the coin position
            } else {
                maze[x][y] = '.'; // Highlight the path with dots
            }
        }
    }

    tcp::acceptor acceptor_;
};

int main() {
    try {
        std::cout << "Maze Server is running..." << std::endl;
        boost::asio::io_context io_context;
        MazeServer server(io_context, 12345); // Start the server on port 12345
        io_context.run(); // Run the server loop
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
