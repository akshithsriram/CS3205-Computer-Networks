/*
    Tic Tac Toe game with multiple-client support and timeout detection
    Code by Akshith Sriram Enadula, CS19B005
    CS3205 Assignment 2.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <poll.h>

#define PORT 8080
#define TIMEOUT 15
#define SZ sizeof(int)

using namespace std;

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

int restart_game_code = 100;
int terminate_game_code = 101;
int time_out_code = 102;

// Indicates that opponent disconnected.
void partner_disconnect() {
    cout << "Sorry, your partner disconnected.\n";
    exit(0);
}

// Receives integer from the server, used for communication.
int receive_integer(int sockfd) {
    int rec = 0, out = 0;
    if((out = read(sockfd, &rec, SZ)) != SZ) partner_disconnect();
    return rec;
}

// Sends integer to the server, used for communication.
void send_integer(int sockfd, int input) {
    int out = 0;
    if((out = write(sockfd, &input, sizeof(int))) < 0) partner_disconnect();
}

// Creates & connects a socket to the server.
int connect_to_server(){
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Failed to create socket.\n");
        exit(0);
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        printf("Failed to connect to the server.\n");
        exit(0);
    }
    return sockfd;
}

// Starting gameboard configuration.
void starting_gameboard() {   
    printf("__ | __ | __\n");
    printf("__ | __ | __\n");
    printf("__ | __ | __\n");
}

// Prints current configuration of the gameboard.
void draw_gameboard(char gameboard[][3]) {
    for(int i=0; i<5; i++){
        if(i & 1)  printf("-----------\n");
        else printf(" %c | %c | %c \n", gameboard[i/2][0], gameboard[i/2][1], gameboard[i/2][2]);
    }
}

// Reads coordinates from the player & returns the effective move.
// Also polls continuously to check for time out.
int make_move(int sockfd){
    while (1) { 
        cout << "Enter (ROW, COL) for placing your mark: ";
        fflush(stdout);

        // poll for 15 seconds till the input is received.
        struct pollfd pfd[1];
        pfd[0].fd = 0;
        pfd[0].events = POLLIN;   
        pfd[0].revents = 0;
        int res = poll((struct pollfd*)&pfd , 1, TIMEOUT*1000);
        
        // If the input is not given within 15 sec, print TIMEOUT message
        if(res == 0) {
            cout << "TIMEOUT.\n";
            fflush(stdout);
            write(sockfd, &time_out_code, sizeof(int));
            return 0;
        }

        int x, y; cin >> x >> y;
        x--; y--;
        
        int move = 3*x + y;
        if (move < 0 || move > 8){
            cout << "Invalid input. Try again.\n";
        } 
        else {
            printf("\n");
            send_integer(sockfd, move);   
            break;
        }  
    }
    return 1;
}

// Update gameboard at the move position using appropriate character
void update_gameboard(int sockfd, char gameboard[][3]) {
    int player_id = receive_integer(sockfd);
    int move = receive_integer(sockfd);
    int x = move/3, y = move%3;
    gameboard[x][y] = (player_id & 1) ? 'O' : 'X';    
}

int main(int argc, char *argv[]) {
    
    int sockfd = connect_to_server();
    int id, opponent_id, game_id;
    char gameboard[3][3] = { {' ', ' ', ' '}, {' ', ' ', ' '}, {' ', ' ', ' '} };
    int code = -1, stop = 0, return_code, res, new_game_id, time_out_check = 1;
    char c;

    // receive player id from the server
    id = receive_integer(sockfd);
    cout << "Connected to the game server. Your player ID is " + to_string(id);
    fflush(stdout);

    if(id & 1){
        cout << ". Waiting for a partner to join ...";
        fflush(stdout);
    }

    // Wait until the server sends a 'start' signal
    while((code = receive_integer(sockfd)) != start) {
        ;
    }
    
    opponent_id = receive_integer(sockfd);
    game_id = receive_integer(sockfd);
    
    // Identify symbol and start game   
    char symbol = (id & 1) ? 'X' : 'O';
    cout << "\nYour partner’s ID is " + to_string(opponent_id) + 
            ". Your symbol is ‘" + symbol + "’\n";
    cout << "Your Game ID is #" + to_string(game_id) << endl;
    cout << "Starting the game ...\n";
    fflush(stdout);

    starting_gameboard();

    // Repeat operations (based on the signal from the server) until the stop flag is set to 1.
    while(!stop) {
        code = receive_integer(sockfd);
        switch (code) {
            // update gameboard
            case 1: update_gameboard(sockfd, gameboard);
                    draw_gameboard(gameboard);
                    break;
            // Invalid move
            case 2: cout << "Position already marked. Choose another position.\n";
                    break;
            // Win
            case 3: cout << "Congratulations! You win!\n";
                    break;
            // Ask for replay
            case 4: cout << "Game Over.\n";
                    cout << "Do you want to play again? (Enter Y/y for yes): ";
                    fflush(stdout);
                    cin >> c;
                    // Based on user input, send a signal to server
                    send_integer(sockfd, (c == 'y') || (c == 'Y'));
                    return_code = receive_integer(sockfd);

                    // Based on the server response, either replay, or terminate the game.
                    if(return_code == restart_game_code) {
                        new_game_id = receive_integer(sockfd);
                        for(int i=0; i<3; i++) 
                            for(int j=0; j<3; j++) 
                                gameboard[i][j] = ' ';
                        stop = 0;
                        symbol = (id & 1) ? 'X' : 'O';
                        cout << "\nYour partner’s ID is " + to_string(opponent_id) + 
                                ". Your symbol is ‘" + symbol + "’\n";
                        cout << "Your Game ID is #" + to_string(new_game_id) << endl;
                        cout << "Starting the game ...\n";
                        fflush(stdout);
                        draw_gameboard(gameboard);
                        //starting_gameboard();
                    }
                    else stop = 1;
                    break;
            // Lost
            case 5: cout << "Better luck next time. You lost.\n";
                    break;
            // Prompt player to make a move
            case 6: time_out_check = make_move(sockfd);
                    break;
            // Hold
            case 7: break;

            // Draw
            case 8: cout << "Well played, you matched your opponent! It is a Draw.\n";
                    break;
            // Wait
            case 9: cout << "Waiting for Player " + to_string(opponent_id) + "'s move\n";
                    break;
            // Start game
            case 10: for(int i=0; i<3; i++) for(int j=0; j<3; j++) gameboard[i][j] = ' ';
                     break;
            // Error case, code = -1
            default: partner_disconnect(); 
                     stop = 1;
                     break;
        }
    }
    
    cout << "Game over. Terminating Client ...\n";

    close(sockfd);
    return 0;
}
