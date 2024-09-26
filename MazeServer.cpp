#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <utility>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using namespace std;

// Constants for maze dimensions
const int MAZE_HEIGHT = 7, MAZE_WIDTH = 9;
std::vector<std::string> maze = {
    "#########", 
    "#P.....C#",
    "#.C###..#", 
    "#..C....#", 
    "#....####",
    "#C....C.#",
    "#########"
};
int playerX = 1, playerY = 1, coinsRemaining = 5;

class MazeServer {
public:
    MazeServer(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        start_accept();
    }

private:
    tcp::acceptor acceptor_;

    void start_accept() {
        auto socket = make_shared<tcp::socket>(acceptor_.get_executor());
        acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec) {
            if (!ec) 
                send_maze(socket);
            start_accept();
        });
    }

    string get_maze_string() {
        string mazeState = "Position: (" + to_string(playerX) + "," + to_string(playerY) + ") Coins: " + to_string(coinsRemaining) + "\n";
        for (auto& row : maze) mazeState += row + "\n";
        return mazeState;
    }

    void read_command(shared_ptr<tcp::socket> socket) {
        auto buffer = make_shared<boost::asio::streambuf>();
        boost::asio::async_read_until(*socket, *buffer, '\n', [this, socket, buffer](boost::system::error_code ec, size_t) {
            if (!ec) {
                istream is(buffer.get());
                string command;
                getline(is, command);
                process_command(socket, command);
            }
        });
    }

     void shutdown_server() {
        cout << "Shutting down the server gracefully..." << endl;
        acceptor_.close();  // Stop accepting new connections
        cout << "Server shutdown complete." << endl;
        exit(1);
    }

void process_command(shared_ptr<tcp::socket> socket, string& command) {
    // Remove trailing newline if present
    if (!command.empty() && command.back() == '\n') 
        command.pop_back();

    string response;

    // Check if the command is to find the nearest coin
    if (command == "find") {
        response = find_nearest_coin();
    } 
    // Check if the command is to kill the server
    else if (command == "kill") {
        shutdown_server();
        return;  // Exit after shutting down the server
    } 
    // Check if the command is a valid move (W, A, S, D)
    else if (move_player(command)) {
        // Player successfully moved
        if (coinsRemaining == 0) {
            // Player has won by collecting all the coins
            response = "You have won and there are no more coins left.\n";
        } else {
            // Player moved, but coins are still remaining
            response = "Moved " + command + ".\n";
        }
    } 
    // Invalid move or unknown command
    else {
        response = "Invalid move.\n";
    }

    // Append the updated maze state to the response
    response += get_maze_string() + "<END>\n";  // Add the end marker for the maze

    // Send the response to the client
    boost::asio::async_write(*socket, boost::asio::buffer(response),
        [this, socket](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                read_command(socket);  // Read the next command once the response is sent
            }
        });
}

void send_maze(shared_ptr<tcp::socket> socket) {
    // Send the initial maze on connection
    std::string maze_data = get_maze_string() + "<END>\n";  // Add the end marker
    boost::asio::async_write(*socket, boost::asio::buffer(maze_data),
        [this, socket](boost::system::error_code ec, std::size_t) {
        if (!ec) {
            read_command(socket);  // Once the maze is sent, listen for a new command
        }
    });
}




    bool move_player(const string& command) {
        int newX = playerX,
        newY = playerY;

        if (command == "W" && valid_move(newX - 1, newY)) newX--;
        else if (command == "S" && valid_move(newX + 1, newY)) newX++;
        else if (command == "A" && valid_move(newX, newY - 1)) newY--;
        else if (command == "D" && valid_move(newX, newY + 1)) newY++;
        else return false;

        maze[playerX][playerY] = '.';
        playerX = newX, playerY = newY;
        if (maze[playerX][playerY] == 'C')
            coinsRemaining--;
        maze[playerX][playerY] = 'P';
        
        
        return true;
    }

    bool valid_move(int x, int y) {
        return x >= 0 && x < MAZE_HEIGHT && y >= 0 && y < MAZE_WIDTH && maze[x][y] != '#';
    }

    string find_nearest_coin() {
        queue<pair<int, int>> queue;
        vector<vector<bool>> visited(MAZE_HEIGHT, vector<bool>(MAZE_WIDTH, false));
        queue.push({playerX, playerY});
        visited[playerX][playerY] = true;
        vector<pair<int, int>> directions = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        while (!queue.empty()) {
            auto [x, y] = queue.front();
            queue.pop();
            if (maze[x][y] == 'C') return "Nearest coin at: (" + to_string(x) + "," + to_string(y) + ")\n";
            for (auto& [dx, dy] : directions) {
                int nx = x + dx, ny = y + dy;
                if (valid_move(nx, ny) && !visited[nx][ny]) visited[nx][ny] = true, queue.push({nx, ny});
            }
        }
        return "No coins remaining!\n";
    }
};

int main() {
 cout<<"Server Running......"<<endl;

    try {
        boost::asio::io_context io_context;
        MazeServer server(io_context, 12345);
        io_context.run();
       
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }
}
