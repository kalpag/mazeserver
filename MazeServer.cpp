#include <iostream>
#include <boost/asio.hpp>
#include <vector>
#include <string>

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
    "#....###C",
    "#C......#",
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
    
    void shutdown_server() {
        std::cout << "Shutting down the server gracefully..." << std::endl;

        // Stop accepting new connections and stop the io_context
        acceptor_.close();        // Stop accepting new connections
       // acceptor_.get_executor().context().stop();  // Stop all asynchronous operations

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
       
        std::cout<<"client command : " <<command<<std::endl; 
       
       // Remove any newline characters at the end of the command
        if (!command.empty() && command.back() == '\n') {
            command.erase(command.size() - 1);  // Trim the newline
        }

        // Ensure to also handle the case where the client might send a newline character
        if (command == "\n") {
            return; // or handle accordingly
        }
        
        std::string response;
        if (move_player(command)) {
            response = "Moved " + command.substr(5) + ".\n";
            if (coinsRemaining == 0) {
                response += "Victory! You collected all the coins.\n";
            }
        } else {
            response = "Invalid move or command.\n";
        }

        boost::asio::async_write(*socket, boost::asio::buffer(response), 
            [this, socket](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                send_maze(socket);  // Send the updated maze after processing the move
            }
        });
    }

    // Function to move the player based on the command
bool move_player(const std::string& command) {
    int newX = playerX;
    int newY = playerY;

    std::cout << "Current position: (" << playerX << ", " << playerY << "), Command: " << command << "\n";

    if (command == "kill") {
        shutdown_server();
        return false; // Terminate the move operation after server shutdown
    }

    // Handle movement commands
    if (command == "W") {
        if (playerX > 0) {
            newX--;  // Move up
        }
    } else if (command == "S") {
        if (playerX < MAZE_HEIGHT - 1) {
            newX++;  // Move down
        }
    } else if (command == "A") {
        if (playerY > 0) {
            newY--;  // Move left
        }
    } else if (command == "D") {
        if (playerY < MAZE_WIDTH - 1) {
            newY++;  // Move right
        }
    } else {
        std::cout << "Invalid command: " << command << "\n";
        return false;  // Invalid command
    }

    // Debug output for proposed new position
    std::cout << "Proposed new position: (" << newX << ", " << newY << ")\n";

    // Validate new position before accessing the maze
    if (newX >= 0 && newX < MAZE_HEIGHT && newY >= 0 && newY < MAZE_WIDTH) {
        // Check for wall collision
        if (maze[newX][newY] != '#') {
            // Update player's position in the maze
            maze[playerX][playerY] = '.';  // Mark old position as open space
            playerX = newX;
            playerY = newY;

            // If the player collects a coin
            if (maze[playerX][playerY] == 'C') {
                coinsRemaining--;
                std::cout << "Coin collected! Coins remaining: " << coinsRemaining << "\n";
            }
            maze[playerX][playerY] = 'P';  // Update new player position
            return true;  // Successful move
        } else {
            std::cout << "Hit a wall at: (" << newX << ", " << newY << ")\n";
            return false;  // Move invalid due to wall
        }
    } else {
        std::cout << "Proposed move out of bounds: (" << newX << ", " << newY << ")\n";
    }

    return false;  // If all checks fail
}







    tcp::acceptor acceptor_;
};

int main() {
    
       
        std::cout<<"Maze Server is running......"<<std::endl;
        boost::asio::io_context io_context;
        MazeServer server(io_context, 12345);  // Start the server on port 12345
        io_context.run();  // Run the server loop
}
