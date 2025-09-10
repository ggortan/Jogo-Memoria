#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>

#define MAX_PLAYERS 4
#define BOARD_SIZE 16 // 4x4 board
#define PORT 8080
#define BUFFER_SIZE 1024

// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

typedef struct {
    SOCKET socket;
    int id;
    char name[50];
    int score;
    int active;
} Player;

typedef struct {
    int cards[BOARD_SIZE];
    int revealed[BOARD_SIZE];
    int pairs_found;
    Player players[MAX_PLAYERS];
    int player_count;
    int current_player;
    int game_started;
    CRITICAL_SECTION game_mutex;
} GameState;

// Prototypes
void send_turn_info();
DWORD WINAPI handle_client(LPVOID lpParam);
void print_local_ip(int port);
void handle_move(int player_id, int pos1, int pos2);
void broadcast_message(const char* message, int exclude_player);
void send_board_state(SOCKET player_socket);
void send_scores();
int check_game_end();
int find_next_active_player(int current);

GameState game_state;

void initialize_game() {
    printf("Initializing game...\n");
    
    for (int i = 0; i < BOARD_SIZE; i += 2) {
        game_state.cards[i] = (i / 2) + 1;
        game_state.cards[i + 1] = (i / 2) + 1;
        game_state.revealed[i] = 0;
        game_state.revealed[i + 1] = 0;
    }
    
    srand(time(NULL));
    for (int i = BOARD_SIZE - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = game_state.cards[i];
        game_state.cards[i] = game_state.cards[j];
        game_state.cards[j] = temp;
    }
    
    game_state.pairs_found = 0;
    game_state.current_player = 0;
    
    printf("Shuffled cards: ");
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%d ", game_state.cards[i]);
    }
    printf("\n");
}

void initialize_server() {
    game_state.player_count = 0;
    game_state.game_started = 0;
    InitializeCriticalSection(&game_state.game_mutex);
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game_state.players[i].active = 0;
        game_state.players[i].score = 0;
        strcpy(game_state.players[i].name, "");
    }
}

void broadcast_message(const char* message, int exclude_player) {
    printf("Broadcasting: %s", message);
    EnterCriticalSection(&game_state.game_mutex);
    
    int msg_len = strlen(message);
    
    for (int i = 0; i < game_state.player_count; i++) {
        if (game_state.players[i].active && i != exclude_player) {
            int total_sent = 0;
            
            while (total_sent < msg_len) {
                int sent = send(game_state.players[i].socket, 
                                message + total_sent, 
                                msg_len - total_sent, 0);
                
                if (sent == SOCKET_ERROR) {
                    printf("Failed to send to player %d, marking as inactive\n", i);
                    game_state.players[i].active = 0;
                    closesocket(game_state.players[i].socket);
                    break;
                }
                
                total_sent += sent;
            }
            
            if (total_sent == msg_len) {
                printf("Successfully sent %d bytes to player %d\n", total_sent, i);
            }
        }
    }
    
    LeaveCriticalSection(&game_state.game_mutex);
}

void send_board_state(SOCKET player_socket) {
    char board_msg[2048] = "BOARD|";
    for (int i = 0; i < BOARD_SIZE; i++) {
        char card_info[10];
        if (game_state.revealed[i]) {
            sprintf(card_info, "%d", game_state.cards[i]);
        } else {
            strcpy(card_info, "X");
        }
        strcat(board_msg, card_info);
        if (i < BOARD_SIZE - 1) strcat(board_msg, ",");
    }
    strcat(board_msg, "\n");
    printf("Sending board state: %s", board_msg);
    send(player_socket, board_msg, strlen(board_msg), 0);
}

void send_scores() {
    char score_msg[512] = "SCORES|";
    int first = 1;
    for (int i = 0; i < game_state.player_count; i++) {
        if (game_state.players[i].active) {
            char player_score[100];
            if (!first) strcat(score_msg, ",");
            sprintf(player_score, "%s:%d", game_state.players[i].name, game_state.players[i].score);
            strcat(score_msg, player_score);
            first = 0;
        }
    }
    strcat(score_msg, "\n");
    broadcast_message(score_msg, -1);
}

void send_turn_info() {
    if (game_state.players[game_state.current_player].active) {
        char turn_msg[256];
        sprintf(turn_msg, "TURN|%d|%s\n", game_state.current_player, 
                game_state.players[game_state.current_player].name);
        broadcast_message(turn_msg, -1);
    }
}

int check_game_end() {
    return game_state.pairs_found >= (BOARD_SIZE / 2);
}

int find_next_active_player(int current) {
    int next = (current + 1) % game_state.player_count;
    int attempts = 0;
    
    while (!game_state.players[next].active && attempts < game_state.player_count) {
        next = (next + 1) % game_state.player_count;
        attempts++;
    }
    
    if (attempts >= game_state.player_count) {
        return current;
    }
    
    return next;
}

void handle_move(int player_id, int pos1, int pos2) {
    printf("Handle move: player %d, positions %d,%d\n", player_id, pos1, pos2);
    
    EnterCriticalSection(&game_state.game_mutex);
    
    if (player_id != game_state.current_player || !game_state.game_started) {
        printf("Not player's turn or game not started\n");
        char error_msg[] = "ERROR|Not your turn or game not started\n";
        send(game_state.players[player_id].socket, error_msg, strlen(error_msg), 0);
        LeaveCriticalSection(&game_state.game_mutex);
        return;
    }
    
    if (pos1 < 0 || pos1 >= BOARD_SIZE || pos2 < 0 || pos2 >= BOARD_SIZE || 
        pos1 == pos2 || game_state.revealed[pos1] || game_state.revealed[pos2]) {
        printf("Invalid move\n");
        char error_msg[] = "ERROR|Invalid move\n";
        send(game_state.players[player_id].socket, error_msg, strlen(error_msg), 0);
        LeaveCriticalSection(&game_state.game_mutex);
        return;
    }
    
    printf("Valid move - cards: %d and %d\n", game_state.cards[pos1], game_state.cards[pos2]);
    
    char reveal_msg[256];
    sprintf(reveal_msg, "REVEAL|%d,%d|%d,%d\n", pos1, pos2, 
            game_state.cards[pos1], game_state.cards[pos2]);
    broadcast_message(reveal_msg, -1);
    
    if (game_state.cards[pos1] == game_state.cards[pos2]) {
        printf("Match found!\n");
        game_state.revealed[pos1] = 1;
        game_state.revealed[pos2] = 1;
        game_state.players[player_id].score++;
        game_state.pairs_found++;
        
        char match_msg[] = "MATCH|Cards matched!\n";
        broadcast_message(match_msg, -1);
        
    } else {
        Sleep(2000);
        printf("No match\n");
        char no_match_msg[] = "NO_MATCH|Cards don't match!\n";
        broadcast_message(no_match_msg, -1);

        game_state.current_player = find_next_active_player(game_state.current_player);
        printf("Next player: %d\n", game_state.current_player);
    }
    
    for (int i = 0; i < game_state.player_count; i++) {
        if (game_state.players[i].active) {
            send_board_state(game_state.players[i].socket);
        }
    }
    
    send_scores();
    
    if (check_game_end()) {
        int winner = 0;
        int max_score = -1;
        for (int i = 0; i < game_state.player_count; i++) {
            if (game_state.players[i].active && game_state.players[i].score > max_score) {
                max_score = game_state.players[i].score;
                winner = i;
            }
        }
        
        char end_msg[256];
        sprintf(end_msg, "GAME_END|Winner: %s with %d pairs!\n", 
                game_state.players[winner].name, max_score);
        broadcast_message(end_msg, -1);
        game_state.game_started = 0;
        printf("Game ended. Winner: %s\n", game_state.players[winner].name);
    } else {
        send_turn_info();
    }
    
    LeaveCriticalSection(&game_state.game_mutex);
}

DWORD WINAPI handle_client(LPVOID lpParam) {
    SOCKET client_socket = *(SOCKET*)lpParam;
    free(lpParam);
    char buffer[BUFFER_SIZE];
    int player_id = -1;
    
    EnterCriticalSection(&game_state.game_mutex);
    if (game_state.player_count < MAX_PLAYERS) {
        player_id = game_state.player_count;
        game_state.players[player_id].socket = client_socket;
        game_state.players[player_id].id = player_id;
        game_state.players[player_id].score = 0;
        game_state.players[player_id].active = 1;
        game_state.player_count++;
    }
    LeaveCriticalSection(&game_state.game_mutex);
    
    if (player_id == -1) {
        char full_msg[] = "ERROR|Server full\n";
        send(client_socket, full_msg, strlen(full_msg), 0);
        closesocket(client_socket);
        return 1;
    }
    
    printf("Player %d connected\n", player_id);
    
    while (1) {
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            break;
        }
        
        // buffer[bytes_received] = '\0';
        // printf("Received from player %d: %s", player_id, buffer);
        
        char* token = strtok(buffer, "|\n\r");
        if (token == NULL) continue;
        
        if (strcmp(token, "JOIN") == 0) {
            token = strtok(NULL, "|\n\r");
            if (token != NULL) {
                strcpy(game_state.players[player_id].name, token);
                char welcome_msg[256];
                sprintf(welcome_msg, "WELCOME|Player %d: %s\n", player_id, token);
                send(client_socket, welcome_msg, strlen(welcome_msg), 0);
                
                char join_msg[256];
                sprintf(join_msg, "PLAYER_JOIN|%s joined the game\n", token);
                broadcast_message(join_msg, player_id);
                printf("Player %d (%s) joined\n", player_id, token);
            }
        } else if (strcmp(token, "START") == 0) {
            EnterCriticalSection(&game_state.game_mutex);
            if (!game_state.game_started && game_state.player_count >= 1) {
                game_state.game_started = 1;
                initialize_game();
                char start_msg[] = "GAME_START|Game started!\n";
                broadcast_message(start_msg, -1);
                
                for (int i = 0; i < game_state.player_count; i++) {
                    if (game_state.players[i].active) {
                        send_board_state(game_state.players[i].socket);
                    }
                }
                send_scores();
                send_turn_info();
                printf("Game started with %d players\n", game_state.player_count);
            }
            LeaveCriticalSection(&game_state.game_mutex);
        } else if (strcmp(token, "MOVE") == 0) {
            token = strtok(NULL, "|\n\r");
            if (token != NULL) {
                int pos1, pos2;
                if (sscanf(token, "%d,%d", &pos1, &pos2) == 2) {
                    printf("Parsed move: %d,%d\n", pos1, pos2);
                    handle_move(player_id, pos1, pos2);
                } else {
                    printf("Failed to parse move: %s\n", token);
                }
            }
        } else if (strcmp(token, "CHAT") == 0) {
            char* chat_message = strtok(NULL, "|\n\r");
            if (chat_message != NULL) {
                char chat_broadcast_msg[512];
                sprintf(chat_broadcast_msg, "CHAT|%s: %s\n", 
                        game_state.players[player_id].name, chat_message);
                broadcast_message(chat_broadcast_msg, -1);
                printf("Chat message from %s: %s\n", game_state.players[player_id].name, chat_message);
            }
        }
    }
    
    EnterCriticalSection(&game_state.game_mutex);
    game_state.players[player_id].active = 0;
    printf("Player %d disconnected\n", player_id);
    
    char disconnect_msg[256];
    sprintf(disconnect_msg, "PLAYER_LEFT|%s left the game\n", 
            game_state.players[player_id].name);
    broadcast_message(disconnect_msg, player_id);
    LeaveCriticalSection(&game_state.game_mutex);
    
    closesocket(client_socket);
    return 0;
}

void print_local_ip(int port) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Failed to create socket with error: %d\n", WSAGetLastError());
        printf("Memory Game Server started on port %d on host 0.0.0.0 (could not determine local IP)\n", port);
        return;
    }

    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = inet_addr("8.8.8.8");
    remote_addr.sin_port = htons(53);

    if (connect(sock, (SOCKADDR*)&remote_addr, sizeof(remote_addr)) == SOCKET_ERROR) {
        printf("Failed to connect to external server with error: %d\n", WSAGetLastError());
        closesocket(sock);
        printf("Memory Game Server started on port %d on host 0.0.0.0 (could not determine local IP)\n", port);
        return;
    }

    struct sockaddr_in local_addr;
    int addr_len = sizeof(local_addr);
    if (getsockname(sock, (SOCKADDR*)&local_addr, &addr_len) == SOCKET_ERROR) {
        printf("getsockname failed with error: %d\n", WSAGetLastError());
        closesocket(sock);
        printf("Memory Game Server started on port %d on host 0.0.0.0 (could not determine local IP)\n", port);
        return;
    }

    char ip_string[INET_ADDRSTRLEN];
    DWORD ip_string_len = INET_ADDRSTRLEN;
    if (WSAAddressToString((SOCKADDR*)&local_addr, sizeof(local_addr), NULL, ip_string, &ip_string_len) == 0) {
        printf("Memory Game Server started on port %d on host %s\n", port, ip_string);
    } else {
        printf("WSAAddressToString failed with error: %d\n", WSAGetLastError());
        printf("Memory Game Server started on port %d on host 0.0.0.0 (could not determine local IP)\n", port);
    }
    
    closesocket(sock);
}


int main() {
    WSADATA wsaData;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr);
    
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return 1;
    }

    initialize_server();
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed with error: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    
    if (listen(server_socket, MAX_PLAYERS) == SOCKET_ERROR) {
        printf("Listen failed with error: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    
    print_local_ip(PORT);
    printf("Waiting for players...\n");
    
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) {
            printf("Accept failed with error: %d\n", WSAGetLastError());
            continue;
        }
        
        printf("New connection accepted\n");
        
        HANDLE hThread;
        SOCKET* client_sock_ptr = (SOCKET*)malloc(sizeof(SOCKET));
        if (client_sock_ptr == NULL) {
            printf("Failed to allocate memory for client socket.\n");
            closesocket(client_socket);
            continue;
        }
        *client_sock_ptr = client_socket;
        
        hThread = CreateThread(NULL, 0, handle_client, client_sock_ptr, 0, NULL);
        if (hThread == NULL) {
            printf("Thread creation failed with error: %d\n", GetLastError());
            free(client_sock_ptr);
            closesocket(client_socket);
        } else {
            CloseHandle(hThread);
        }
    }
    
    closesocket(server_socket);
    DeleteCriticalSection(&game_state.game_mutex);
    WSACleanup();
    return 0;
}