/*
    Tic Tac Toe game with multiple-client support and timeout detection
    Code by Akshith Sriram Enadula, CS19B005
    CS3205 Assignment 2.
*/

#include <iostream>
#include <vector>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>
#include <string>
#include <chrono>

using namespace std;

#define MAX_PLAYERS 100
#define PORT 8080
#define SZ sizeof(int)

int playerID = 1;
int gameID = 1;
int active_players = 0;
string log_string = "";

pthread_mutex_t lock_mutex;

int upd = 1;        // 1 : Update Board
int inv = 2;        // 2 : Invalid Move
int win = 3;        // 3 : Won Game (Victory)
int ask = 4;        // 4 : ask for rematch
int lose = 5;       // 5 : Lost Game
int turn = 6;       // 6 : Make Move
int hold = 7;       // 7 : Hold (Wait till other player arrives)
int draw = 8;       // 8 : Draw Game
int wait = 9;       // 9 : Wait for other player to play
int start = 10;     // 10 : Start Game

int invalid_input_code = -1;
int restart_game_code = 100;
int terminate_game_code = 101;
int time_out_code = 102;

struct thread_args {
    int * client_sockfd;
    int player1;
    int player2;
    int current_gameID;
};

// Receives integer from client
int receive_integer(int client_sockfd){
    int rec = 0, n;
    if ((n = read(client_sockfd, &rec, sizeof(int))) != sizeof(int))  return -1;
    return rec;
}

// Function to print the tic tac toe gameboard
void display_gameboard(int gameboard[][3]) {
    char char_board[3][3];
    for(int i=0; i<3; i++){
        for(int j=0; j<3; j++){
            if(gameboard[i][j] == 0) char_board[i][j] = 'X';
            else if(gameboard[i][j] == 1) char_board[i][j] = 'O';
            else char_board[i][j] = ' ';
        }
    }
    for(int i=0; i<5; i++){
        if(i & 1)  printf("-----------\n");
        else printf(" %c | %c | %c \n", char_board[i/2][0], char_board[i/2][1], char_board[i/2][2]);
    }
}

// Starting gameboard configuration
void starting_gameboard() {   
    printf("__ | __ | __\n");
    printf("__ | __ | __\n");
    printf("__ | __ | __\n");
}

// Checks if the current position is a win
bool win_game(int gameboard[][3]){
    display_gameboard(gameboard);
    for(int i = 0; i < 3; i++){
        if(gameboard[i][0] == gameboard[i][1] && gameboard[i][1] == gameboard[i][2]) return 1;
        if(gameboard[0][i] == gameboard[1][i] && gameboard[1][i] == gameboard[2][i]) return 1;
    }
    if(gameboard[0][0] == gameboard[1][1] && gameboard[1][1] == gameboard[2][2]) return 1;
    if(gameboard[0][2] == gameboard[1][1] && gameboard[1][1] == gameboard[2][0]) return 1;
    return 0;
}

// Prints the current configuration of the gameboard
void print_gameboard(char gameboard[][3]){
    char sp = ' ', sep = '|';
    string output = "";
    for(int i=0; i<3; i++){
        for(int j=0; j<3; j++){
            output += sp + gameboard[i][j] + sp + (j == 2) ? '\n' : sep;
        }
    }
    cout << output << endl;
}

// Receives player move from the client
int receive_move(int client_sockfd) {
    int response = 0;
    write(client_sockfd, &turn, sizeof(int));
    response = receive_integer(client_sockfd);
    return response;
}

// Check if the current move is valid
int verify_move(int gameboard[][3], int move) {
    if(move < 0 || move > 8) return 0;
    int x = move/3, y = move%3;
    return (gameboard[x][y] == -1);
}

// Update gameboard with the correct value at the correct position
void update_gameboard(int gameboard[][3], int move, int player_id) {
    int x = move/3, y = move%3;
    gameboard[x][y] = (player_id & 1);
}

// Send the update signal, player id and move (position) to the clients
void update_clients(int * client_sockfd, int move, int player_id) {        
    write(client_sockfd[0], &upd, SZ);
    write(client_sockfd[0], &player_id, SZ);
    write(client_sockfd[0], &move, SZ);

    write(client_sockfd[1], &upd, SZ);
    write(client_sockfd[1], &player_id, SZ);
    write(client_sockfd[1], &move, SZ);
}

// Check if the last played move leads to a winning condition
int check_for_win(int gameboard[][3], int last_move) {   
    int x = last_move/3;
    int y = last_move%3;

    int flag = 0;
    flag |= ((gameboard[x][0] == gameboard[x][1]) && (gameboard[x][1] == gameboard[x][2]));
    flag |= ((gameboard[0][y] == gameboard[1][y]) && (gameboard[1][y] == gameboard[2][y]));

    if(flag) return flag;

    if((last_move & 1) == 0){
        if(last_move == 0 || last_move == 4 || last_move == 8)
            flag |= ((gameboard[1][1] == gameboard[0][0]) && (gameboard[1][1] == gameboard[2][2]));
        else {
            flag |= ((gameboard[1][1] == gameboard[0][2]) && (gameboard[1][1] == gameboard[2][0]));
        }
    }
    return flag;
}

// Create a socket for server and bind
int create_socket(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Failure in Socket Creation.\n");
        exit(-1);
    } 

    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;    
    server_address.sin_addr.s_addr = INADDR_ANY;    
    server_address.sin_port = htons(PORT);      

    if (bind(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0){
        cout << "Error in binding socket.\n";
        return -1;
    }
    return sockfd;
}

// Create client connections to the server
void initialize_game(int server_sockfd, int * client_sockfd, int player1, int player2) {
    socklen_t client_sockfd_len;
    struct sockaddr_in serv_addr, cli_addr;

    cout << "Game server started. Waiting for players.\n";

    int conn_count = 0;
    while(conn_count < 2){
        listen(server_sockfd, MAX_PLAYERS - active_players + 1);
        bzero(&cli_addr, sizeof(cli_addr));
        client_sockfd_len = sizeof(cli_addr);

        if(conn_count == 0){
            client_sockfd[0] = accept(server_sockfd, (struct sockaddr*) &cli_addr, &client_sockfd_len);
            if (client_sockfd[0] < 0) 
                cout << "Client connection error (Player " + to_string(player1) + ")\n";
            cout << "Player " + to_string(player1) + " connected to the server.\n";
            write(client_sockfd[0], &player1, SZ);
        } else {
            client_sockfd[1] = accept(server_sockfd, (struct sockaddr*) &cli_addr, &client_sockfd_len);
            if (client_sockfd[1] < 0) 
                cout << "Client connection error (Player " + to_string(player2) + ")\n";
            cout << "Player " + to_string(player2) + " connected to the server.\n";
            write(client_sockfd[1], &player2, SZ);
        }
        conn_count++;
    } 
}

// Send 'start' signal to the clients
void start_client_game(int client_sockfd, int start, int player, int game_id){
    write(client_sockfd, &start, SZ);
    write(client_sockfd, &player, SZ);
    write(client_sockfd, &game_id, SZ);
}

// Start the game between two players in the current thread
void *run_game(void *thread_data) {

    // Extract data from the thread_data
    chrono::high_resolution_clock::time_point start_time, end_time;
    struct thread_args * args = (struct thread_args*) thread_data;
    int *client_sockfd = args->client_sockfd;
    int player1 = args->player1;
    int player2 = args->player2;
    //int current_gameID = args->current_gameID;
    int current_gameID = gameID++;
    int repeat = 0;

    // Log String to print to the log file corresponding to this game.
    log_string = "";
    log_string += "Current game ID is " + to_string(current_gameID) + "\n";
    log_string += "Game started between Player " + to_string(player1) + " and Player " + to_string(player2) + "\n";
    
    start_time = chrono::high_resolution_clock::now();

    // Create and initialize gameboard for the game
    int gameboard[3][3];
    for(int i=0; i<3; i++) for(int j=0; j<3; j++) gameboard[i][j] = -1;
    display_gameboard(gameboard);

    start_client_game(client_sockfd[0], start, player2, current_gameID);
    start_client_game(client_sockfd[1], start, player1, current_gameID);

    do {
        cout << "Starting the game #" + to_string(current_gameID) + " between Player " + 
                to_string(player1) + " and Player " + to_string(player2) + "\n";

        
        for(int i=0; i<3; i++) for(int j=0; j<3; j++) gameboard[i][j] = -1;
        display_gameboard(gameboard);
        
        int next_player_turn = 1, possible = 1;
        int current_player_turn = 0, terminate_game = 0, turn_idx = 0, response1 = 0, response2 = 0;
        
        while(!terminate_game) {
            int not_current_player_turn = (current_player_turn + 1) & 1;
            if (next_player_turn != current_player_turn)
                write(client_sockfd[not_current_player_turn], &wait, SZ);

            int valid = 0;
            int move = 0;

            string current_player = "", not_current_player = "";
            if(current_player_turn == 0) {
                current_player = to_string(player1);
                not_current_player = to_string(player2);
            } else {
                current_player = to_string(player2);
                not_current_player = to_string(player1);
            }
            
            while(!valid) {             
                move = receive_move(client_sockfd[current_player_turn]);
                
                if (move == -1 || move == 102) break; 

                cout << "Player " << current_player << " played position " << "(" << 1+move/3 << ", " << 1+move%3 << ")\n";

                valid = verify_move(gameboard, move);
                if (!valid) { 
                    cout <<  "Player " << current_player << " made an invalid Move. Prompting to try again.\n";
                    log_string += "Player " + current_player + " made an invalid move.\n";
                    write(client_sockfd[current_player_turn], &inv, SZ);
                } else {
                    log_string += "Player " + current_player + " marked (" + 
                    to_string(move/3 + 1) + ", " + to_string(1 + move%3) + ")\n";
                }
            }

            if (move == invalid_input_code) { 
                cout << "Player " + current_player + " disconnected.\n";
                end_time = chrono::high_resolution_clock::now();
                log_string += "Player " + current_player + " disconnected.\n";
                
                double time_span = std::chrono::duration<double, std::milli>(end_time-start_time).count();
                time_span = time_span/1000;
                log_string += "Total duration of the game " + to_string(time_span) + " seconds\n";
                possible = 0;
                break;
            } else if(move == time_out_code){
                end_time = chrono::high_resolution_clock::now();
                cout << "Player " + current_player + " Timeout.\n";

                log_string += "Player " + current_player + " Timeout.\n";
                double time_span = std::chrono::duration<double, std::milli>(end_time-start_time).count();
                time_span = time_span/1000;
                log_string += "Total duration of the game " + to_string(time_span) + " seconds\n";
                int not_current_player_turn = (current_player_turn + 1) & 1;
                write(client_sockfd[current_player_turn], &ask, SZ);
                write(client_sockfd[not_current_player_turn], &ask, SZ);
                response1 = receive_integer(client_sockfd[0]);
                response2 = receive_integer(client_sockfd[1]);
                
                repeat = (response1 & response2);
                if(repeat) {
                    log_string += "\nRematch starting\n";
                    int repeat_code = 100;
                    write(client_sockfd[0], &repeat_code, SZ);
                    write(client_sockfd[1], &repeat_code, SZ);

                    int new_game_id = gameID++;
                    write(client_sockfd[0], &new_game_id, SZ);
                    write(client_sockfd[1], &new_game_id, SZ);
                    start_time = chrono::high_resolution_clock::now();

                } else {
                    cout << "No rematch.\n";
                    log_string += "\nPlayers didn't want to play again, no rematch.\n";
                    end_time = chrono::high_resolution_clock::now();
                    double time_span = std::chrono::duration<double, std::milli>(end_time-start_time).count();
                    time_span = time_span/1000;
                    log_string += "Total duration of the game " + to_string(time_span) + " seconds\n";
                    int stop_code = 101;
                    write(client_sockfd[0], &stop_code, SZ);
                    write(client_sockfd[1], &stop_code, SZ);
                    terminate_game = 1;
                    possible = 0;
                    break;
                }
                
            }
            else {
                
                update_gameboard(gameboard, move, current_player_turn);
                update_clients(client_sockfd, move, current_player_turn);
                display_gameboard(gameboard);
                
                if (check_for_win(gameboard, move) == 1) {
                    int not_current_player_turn = (current_player_turn + 1) & 1;
                    write(client_sockfd[current_player_turn], &win, SZ);
                    write(client_sockfd[not_current_player_turn], &lose, SZ);
                    cout << "Player " << current_player << " won.\n";
                    log_string += "Player " + current_player + " won the game.\n";
                    end_time = chrono::high_resolution_clock::now();
                    double time_span = std::chrono::duration<double, std::milli>(end_time-start_time).count();
                    time_span = time_span/1000;
                    log_string += "Total duration of the game " + to_string(time_span) + " seconds\n";
                    terminate_game = 1;
                }
                else if (turn_idx == 8) {                
                    cout << "Game #" << current_gameID << " ended in a draw.\n";
                    log_string += "The game ended in a draw.\n";
                    end_time = chrono::high_resolution_clock::now();
                    double time_span = std::chrono::duration<double, std::milli>(end_time-start_time).count();
                    time_span = time_span/1000;
                    log_string += "Total duration of the game " + to_string(time_span) + " seconds\n";
                    write(client_sockfd[0], &draw, SZ);
                    write(client_sockfd[1], &draw, SZ);
                    terminate_game = 1;
                }

                next_player_turn = current_player_turn;
                current_player_turn = (current_player_turn + 1) % 2;
                turn_idx++;
            }
        }

        if(possible){
            write(client_sockfd[0], &ask, SZ);
            write(client_sockfd[1], &ask, SZ);
            response1 = receive_integer(client_sockfd[0]);
            response2 = receive_integer(client_sockfd[1]);
            
            repeat = (response1 & response2);
            if(repeat) {
                log_string += "\nRematch starting\n";
                int repeat_code = restart_game_code;
                write(client_sockfd[0], &repeat_code, SZ);
                write(client_sockfd[1], &repeat_code, SZ);

                int new_game_id = gameID++;
                write(client_sockfd[0], &new_game_id, SZ);
                write(client_sockfd[1], &new_game_id, SZ);

                start_time = chrono::high_resolution_clock::now();

            } else {
                int stop_code = terminate_game_code;
                log_string += "\nPlayers didn't want to play again, no rematch.\n";
                end_time = chrono::high_resolution_clock::now();
                double time_span = std::chrono::duration<double, std::milli>(end_time-start_time).count();
                time_span = time_span/1000;
                log_string += "Total duration of the game " + to_string(time_span) + " seconds\n";
                write(client_sockfd[0], &stop_code, SZ);
                write(client_sockfd[1], &stop_code, SZ);
            }
        }
        
    } while(repeat);

    printf("Game over.\n");

    close(client_sockfd[0]);
    close(client_sockfd[1]);

    pthread_mutex_lock(&lock_mutex);
    active_players -= 2;
    pthread_mutex_unlock(&lock_mutex);

    ofstream file;
    string file_name = "log_game" + to_string(current_gameID) + ".txt";
    file.open(file_name);
    file << log_string << endl;
    file.close();

    free(client_sockfd);
    pthread_exit(NULL);
}


int main(){
    
    pthread_mutex_init(&lock_mutex, NULL);
    int server_socket = create_socket();
    if(server_socket == -1) return 0;

    do {
        struct thread_args * args = (struct thread_args *)malloc(sizeof(struct thread_args));
        
        int *temp = (int*)malloc(2*SZ); 
        bzero(temp, sizeof(temp));
        args->client_sockfd = temp;
        args->player1 = playerID++;
        args->player2 = playerID++;
        //args->current_gameID = gameID++;
       
        initialize_game(server_socket, temp, args->player1, args->player2);
        pthread_t thread;
        log_string = "";
        int rc = pthread_create(&thread, NULL, run_game, (void *)args);
        if (rc){
            cout << "Failure in thread creation: return code = " << rc << endl;
            continue;
        }  else {
            pthread_mutex_lock(&lock_mutex);
            active_players += 2;
            pthread_mutex_unlock(&lock_mutex);
        }
    } while(active_players <= MAX_PLAYERS);

    close(server_socket);

    pthread_exit(NULL);
    pthread_mutex_destroy(&lock_mutex);
    return 0;
}