#include <iostream>
#include <boost/asio.hpp>
#include <vector>
#include <string>
#include <queue>
#include <utility>

using boost::asio::ip::tcp;

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
        std::cout << "Shutting down the server gracefully..." << std::endl;
        acceptor_.close();  // Stop accepting new connections
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
        send_maze(socket);  // Send the initial maze to the client
        read_command(socket);  // Start reading commands from the client
    }

    void send_maze(std::shared_ptr<tcp::socket> socket) {
        std::string mazeStr = get_maze_string();
        boost::asio::async_write(*socket, boost::asio::buffer(mazeStr), 
            [this, socket](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                read_command(socket);  // Continue reading commands after sending the maze
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

    void process_command(std::shared_ptr<tcp::socket> socket, std::string& command) {
        std::cout << "Client command: " << command << std::endl; 
        if (!command.empty() && command.back() == '\n') {
            command.erase(command.size() - 1);  // Trim the newline
        }
        
        std::string response;
        if (move_player(command)) {
            response = "Moved " + command + ".\n";
            if (coinsRemaining == 0) {
                response += "Victory! You collected all the coins.\n";
            }
            // Highlight the path after moving
            highlight_path();

            // Find and display the shortest path to the nearest coin
            std::string shortestPath = find_shortest_path();
            response += "Shortest path to the nearest coin: " + shortestPath + "\n";
        } else {
            response = "Invalid move or command.\n";
        }

        // Send updated maze
        send_maze(socket);
    }

    // Function to move the player based on the command
    bool move_player(const std::string& command) {
        int newX = playerX;
        int newY = playerY;

        std::cout << "Current position: (" << playerX << ", " << playerY << "), Command: " << command << std::endl;

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
        playerX = newX;
        playerY = newY;

        // If the player collects a coin
        if (maze[playerX][playerY] == 'C') {
            coinsRemaining--;
            std::cout << "Coin collected! Coins remaining: " << coinsRemaining << std::endl;
        }
        maze[playerX][playerY] = 'P';  // Mark the new position of the player

        std::cout << "New position: (" << playerX << ", " << playerY << "), Coins remaining: " << coinsRemaining << std::endl;

        return true;
    }

    // Function to highlight the path taken by the player
    void highlight_path() {
        // This is a simple demonstration of marking the path
        // In a real scenario, you may want to track the path more intelligently
        // Here, we will just mark the previous position with 'X'
        if (maze[playerX][playerY] != 'P') {
            maze[playerX][playerY] = 'X';  // Mark the path
        }
    }

    // Function to find the shortest path to the nearest coin using BFS
    std::string find_shortest_path() {
        std::queue<std::pair<int, int>> queue;
        std::vector<std::vector<bool>> visited(MAZE_HEIGHT, std::vector<bool>(MAZE_WIDTH, false));
        std::vector<std::vector<std::pair<int, int>>> parent(MAZE_HEIGHT, std::vector<std::pair<int, int>>(MAZE_WIDTH, {-1, -1}));

        queue.push({playerX, playerY});
        visited[playerX][playerY] = true;

        // Directions for moving in the maze
        std::vector<std::pair<int, int>> directions = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        
        while (!queue.empty()) {
            auto [x, y] = queue.front();
            queue.pop();

            // Check for coins
            if (maze[x][y] == 'C') {
                // Backtrack to find the path
                std::string path;
                while (parent[x][y] != std::make_pair(-1, -1)) {
                    path += "(" + std::to_string(x) + "," + std::to_string(y) + ") ";
                    std::tie(x, y) = parent[x][y];
                }
                std::reverse(path.begin(), path.end());
                return path;  // Return the path to the nearest coin
            }

            // Explore neighbors
            for (const auto& [dx, dy] : directions) {
                int nx = x + dx;
                int ny = y + dy;

                if (nx >= 0 && nx < MAZE_HEIGHT && ny >= 0 && ny < MAZE_WIDTH && !visited[nx][ny] && maze[nx][ny] != '#') {
                    visited[nx][ny] = true;
                    parent[nx][ny] = {x, y};  // Track the parent node
                    queue.push({nx, ny});
                }
            }
        }
        return "No path to a coin found.";  // If no path found
    }
};

int main() {
    try {
        std::cout << "Maze Server is running..." << std::endl;
        boost::asio::io_context io_context;
        MazeServer server(io_context, 12345);  // Start the server on port 12345
        io_context.run();  // Run the server loop
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
