//Versão: 2.0.4 - Linux

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#define MAX_JOGADORES 4
#define TAMANHO_TABULEIRO 16
#define PORTA 8080
#define TAMANHO_BUFFER 1024

typedef struct {
    int socket;
    int id;
    char nome[50];
    int pontuacao;
    int ativo;
} Jogador;

typedef struct {
    int cartas[TAMANHO_TABULEIRO];
    int reveladas[TAMANHO_TABULEIRO];
    int pares_encontrados;
    Jogador jogadores[MAX_JOGADORES];
    int total_jogadores;
    int jogador_atual;
    int jogo_iniciado;
    pthread_mutex_t mutex_jogo;
} EstadoJogo;

void enviar_info_turno();
void* gerenciar_cliente(void* arg);
void imprimir_ip_local(int porta);
void processar_jogada(int id_jogador, int pos1, int pos2);
void transmitir_mensagem(const char* mensagem, int excluir_jogador);
void enviar_estado_tabuleiro(int socket_jogador);
void enviar_pontuacoes();
int verificar_fim_jogo();
int encontrar_proximo_jogador_ativo(int atual);

EstadoJogo estado_jogo;

void inicializar_jogo() {
    printf("Inicializando jogo...\n");
    
    for (int i = 0; i < TAMANHO_TABULEIRO; i += 2) {
        estado_jogo.cartas[i] = (i / 2) + 1;
        estado_jogo.cartas[i + 1] = (i / 2) + 1;
        estado_jogo.reveladas[i] = 0;
        estado_jogo.reveladas[i + 1] = 0;
    }
    
    srand(time(NULL));
    for (int i = TAMANHO_TABULEIRO - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = estado_jogo.cartas[i];
        estado_jogo.cartas[i] = estado_jogo.cartas[j];
        estado_jogo.cartas[j] = temp;
    }
    
    estado_jogo.pares_encontrados = 0;
    estado_jogo.jogador_atual = 0;
    
    printf("Cartas embaralhadas: ");
    for (int i = 0; i < TAMANHO_TABULEIRO; i++) {
        printf("%d ", estado_jogo.cartas[i]);
    }
    printf("\n");
}

void inicializar_servidor() {
    estado_jogo.total_jogadores = 0;
    estado_jogo.jogo_iniciado = 0;
    pthread_mutex_init(&estado_jogo.mutex_jogo, NULL);
    
    for (int i = 0; i < MAX_JOGADORES; i++) {
        estado_jogo.jogadores[i].ativo = 0;
        estado_jogo.jogadores[i].pontuacao = 0;
        strcpy(estado_jogo.jogadores[i].nome, "");
    }
}

void transmitir_mensagem(const char* mensagem, int excluir_jogador) {
    printf("Transmitindo: %s", mensagem);
    pthread_mutex_lock(&estado_jogo.mutex_jogo);
    
    int tamanho_msg = strlen(mensagem);
    
    for (int i = 0; i < estado_jogo.total_jogadores; i++) {
        if (estado_jogo.jogadores[i].ativo && i != excluir_jogador) {
            int total_enviado = 0;
            
            while (total_enviado < tamanho_msg) {
                int enviado = send(estado_jogo.jogadores[i].socket, 
                                mensagem + total_enviado, 
                                tamanho_msg - total_enviado, 0);
                
                if (enviado < 0) {
                    printf("Falha ao enviar para jogador %d, marcando como inativo\n", i);
                    estado_jogo.jogadores[i].ativo = 0;
                    close(estado_jogo.jogadores[i].socket);
                    break;
                }
                
                total_enviado += enviado;
            }
            
            if (total_enviado == tamanho_msg) {
                printf("Enviado com sucesso %d bytes para jogador %d\n", total_enviado, i);
            }
        }
    }
    
    pthread_mutex_unlock(&estado_jogo.mutex_jogo);
}

void enviar_estado_tabuleiro(int socket_jogador) {
    char msg_tabuleiro[2048] = "TABULEIRO|";
    for (int i = 0; i < TAMANHO_TABULEIRO; i++) {
        char info_carta[10];
        if (estado_jogo.reveladas[i]) {
            sprintf(info_carta, "%d", estado_jogo.cartas[i]);
        } else {
            strcpy(info_carta, "X");
        }
        strcat(msg_tabuleiro, info_carta);
        if (i < TAMANHO_TABULEIRO - 1) strcat(msg_tabuleiro, ",");
    }
    strcat(msg_tabuleiro, "\n");
    printf("Enviando estado do tabuleiro: %s", msg_tabuleiro);
    send(socket_jogador, msg_tabuleiro, strlen(msg_tabuleiro), 0);
}

void enviar_pontuacoes() {
    char msg_pontuacao[512] = "PONTUACAO|";
    int primeiro = 1;
    for (int i = 0; i < estado_jogo.total_jogadores; i++) {
        if (estado_jogo.jogadores[i].ativo) {
            char pontuacao_jogador[100];
            if (!primeiro) strcat(msg_pontuacao, ",");
            sprintf(pontuacao_jogador, "%s:%d", estado_jogo.jogadores[i].nome, estado_jogo.jogadores[i].pontuacao);
            strcat(msg_pontuacao, pontuacao_jogador);
            primeiro = 0;
        }
    }
    strcat(msg_pontuacao, "\n");
    transmitir_mensagem(msg_pontuacao, -1);
}

void enviar_info_turno() {
    if (estado_jogo.jogadores[estado_jogo.jogador_atual].ativo) {
        char msg_turno[256];
        sprintf(msg_turno, "TURNO|%d|%s\n", estado_jogo.jogador_atual, 
                estado_jogo.jogadores[estado_jogo.jogador_atual].nome);
        transmitir_mensagem(msg_turno, -1);
    }
}

int verificar_fim_jogo() {
    return estado_jogo.pares_encontrados >= (TAMANHO_TABULEIRO / 2);
}

int encontrar_proximo_jogador_ativo(int atual) {
    int proximo = (atual + 1) % estado_jogo.total_jogadores;
    int tentativas = 0;
    
    while (!estado_jogo.jogadores[proximo].ativo && tentativas < estado_jogo.total_jogadores) {
        proximo = (proximo + 1) % estado_jogo.total_jogadores;
        tentativas++;
    }
    
    if (tentativas >= estado_jogo.total_jogadores) {
        return atual;
    }
    
    return proximo;
}

void processar_jogada(int id_jogador, int pos1, int pos2) {
    printf("Processar jogada: jogador %d, posicoes %d,%d\n", id_jogador, pos1, pos2);
    
    pthread_mutex_lock(&estado_jogo.mutex_jogo);
    
    if (id_jogador != estado_jogo.jogador_atual || !estado_jogo.jogo_iniciado) {
        printf("Nao eh turno do jogador ou jogo nao iniciado\n");
        char msg_erro[] = "ERRO|Nao eh sua vez ou o jogo nao comecou\n";
        send(estado_jogo.jogadores[id_jogador].socket, msg_erro, strlen(msg_erro), 0);
        pthread_mutex_unlock(&estado_jogo.mutex_jogo);
        return;
    }
    
    if (pos1 < 0 || pos1 >= TAMANHO_TABULEIRO || pos2 < 0 || pos2 >= TAMANHO_TABULEIRO || 
        pos1 == pos2 || estado_jogo.reveladas[pos1] || estado_jogo.reveladas[pos2]) {
        printf("Jogada invalida\n");
        char msg_erro[] = "ERRO|Invalid move\n";
        send(estado_jogo.jogadores[id_jogador].socket, msg_erro, strlen(msg_erro), 0);
        pthread_mutex_unlock(&estado_jogo.mutex_jogo);
        return;
    }
    
    char msg_revelar[256];
    sprintf(msg_revelar, "REVELA|%d,%d|%d,%d\n", pos1, pos2, 
            estado_jogo.cartas[pos1], estado_jogo.cartas[pos2]);
    transmitir_mensagem(msg_revelar, -1);
    
    if (estado_jogo.cartas[pos1] == estado_jogo.cartas[pos2]) {
        printf("Par encontrado!\n");
        estado_jogo.reveladas[pos1] = 1;
        estado_jogo.reveladas[pos2] = 1;
        estado_jogo.jogadores[id_jogador].pontuacao++;
        estado_jogo.pares_encontrados++;
        
        char msg_par[] = "PAR|Acertou!\n";
        transmitir_mensagem(msg_par, -1);
        
    } else {
        sleep(2);
        printf("Nao eh par\n");
        char msg_nao_par[] = "SEM_PAR|As cartas nao sao iguais!\n";
        transmitir_mensagem(msg_nao_par, -1);

        estado_jogo.jogador_atual = encontrar_proximo_jogador_ativo(estado_jogo.jogador_atual);
        printf("Proximo jogador: %d\n", estado_jogo.jogador_atual);
    }
    
    for (int i = 0; i < estado_jogo.total_jogadores; i++) {
        if (estado_jogo.jogadores[i].ativo) {
            enviar_estado_tabuleiro(estado_jogo.jogadores[i].socket);
        }
    }
    
    enviar_pontuacoes();
    
    if (verificar_fim_jogo()) {
        int vencedor = 0;
        int pontuacao_maxima = -1;
        for (int i = 0; i < estado_jogo.total_jogadores; i++) {
            if (estado_jogo.jogadores[i].ativo && estado_jogo.jogadores[i].pontuacao > pontuacao_maxima) {
                pontuacao_maxima = estado_jogo.jogadores[i].pontuacao;
                vencedor = i;
            }
        }
        
        char msg_fim[256];
        sprintf(msg_fim, "JOGO_FIM|Vencedor: %s com %d acertos!\n", 
                estado_jogo.jogadores[vencedor].nome, pontuacao_maxima);
        transmitir_mensagem(msg_fim, -1);
        estado_jogo.jogo_iniciado = 0;
        printf("Jogo finalizado. Vencedor: %s\n", estado_jogo.jogadores[vencedor].nome);
    } else {
        enviar_info_turno();
    }
    
    pthread_mutex_unlock(&estado_jogo.mutex_jogo);
}

void* gerenciar_cliente(void* arg) {
    int socket_cliente = *(int*)arg;
    free(arg);
    char buffer[TAMANHO_BUFFER];
    int id_jogador = -1;
    
    pthread_mutex_lock(&estado_jogo.mutex_jogo);
    if (estado_jogo.total_jogadores < MAX_JOGADORES) {
        id_jogador = estado_jogo.total_jogadores;
        estado_jogo.jogadores[id_jogador].socket = socket_cliente;
        estado_jogo.jogadores[id_jogador].id = id_jogador;
        estado_jogo.jogadores[id_jogador].pontuacao = 0;
        estado_jogo.jogadores[id_jogador].ativo = 1;
        estado_jogo.total_jogadores++;
    }
    pthread_mutex_unlock(&estado_jogo.mutex_jogo);
    
    if (id_jogador == -1) {
        char msg_cheio[] = "ERRO|Servidor cheio\n";
        send(socket_cliente, msg_cheio, strlen(msg_cheio), 0);
        close(socket_cliente);
        return NULL;
    }
    
    printf("Jogador %d conectado\n", id_jogador);
    
    while (1) {
        int bytes_recebidos = recv(socket_cliente, buffer, TAMANHO_BUFFER - 1, 0);
        if (bytes_recebidos <= 0) {
            break;
        }
        
        buffer[bytes_recebidos] = '\0';
        
        char* token = strtok(buffer, "|\n\r");
        if (token == NULL) continue;
        
        if (strcmp(token, "ENTRADA") == 0) {
            token = strtok(NULL, "|\n\r");
            if (token != NULL) {
                strcpy(estado_jogo.jogadores[id_jogador].nome, token);
                char msg_boas_vindas[256];
                sprintf(msg_boas_vindas, "BEM_VINDO|Jogador %d: %s\n", id_jogador, token);
                send(socket_cliente, msg_boas_vindas, strlen(msg_boas_vindas), 0);
                
                char msg_entrou[256];
                sprintf(msg_entrou, "JOGADOR_ENTRADA|%s entrou no jogo\n", token);
                transmitir_mensagem(msg_entrou, id_jogador);
                printf("Jogador %d (%s) entrou\n", id_jogador, token);
            }
        } else if (strcmp(token, "INICIO") == 0) {
            pthread_mutex_lock(&estado_jogo.mutex_jogo);
            if (!estado_jogo.jogo_iniciado && estado_jogo.total_jogadores >= 1) {
                estado_jogo.jogo_iniciado = 1;
                inicializar_jogo();
                char msg_inicio[] = "JOGO_INICIO|Comecou o jogo!\n";
                transmitir_mensagem(msg_inicio, -1);
                
                for (int i = 0; i < estado_jogo.total_jogadores; i++) {
                    if (estado_jogo.jogadores[i].ativo) {
                        enviar_estado_tabuleiro(estado_jogo.jogadores[i].socket);
                    }
                }
                enviar_pontuacoes();
                enviar_info_turno();
                printf("Jogo iniciado com %d jogadores\n", estado_jogo.total_jogadores);
            }
            pthread_mutex_unlock(&estado_jogo.mutex_jogo);
        } else if (strcmp(token, "MOVIMENTO") == 0) {
            token = strtok(NULL, "|\n\r");
            if (token != NULL) {
                int pos1, pos2;
                if (sscanf(token, "%d,%d", &pos1, &pos2) == 2) {
                    printf("Jogada analisada: %d,%d\n", pos1, pos2);
                    processar_jogada(id_jogador, pos1, pos2);
                } else {
                    printf("Falha ao analisar jogada: %s\n", token);
                }
            }
        } else if (strcmp(token, "CHAT") == 0) {
            char* mensagem_chat = strtok(NULL, "|\n\r");
            if (mensagem_chat != NULL) {
                char msg_chat_transmitir[512];
                sprintf(msg_chat_transmitir, "CHAT|%s: %s\n", 
                        estado_jogo.jogadores[id_jogador].nome, mensagem_chat);
                transmitir_mensagem(msg_chat_transmitir, -1);
                printf("Mensagem de chat de %s: %s\n", estado_jogo.jogadores[id_jogador].nome, mensagem_chat);
            }
        }
    }
    
    pthread_mutex_lock(&estado_jogo.mutex_jogo);
    estado_jogo.jogadores[id_jogador].ativo = 0;
    printf("Jogador %d desconectou\n", id_jogador);
    
    char msg_desconexao[256];
    sprintf(msg_desconexao, "JOGADOR_SAIDA|%s saiu do jogo\n", 
            estado_jogo.jogadores[id_jogador].nome);
    transmitir_mensagem(msg_desconexao, id_jogador);
    pthread_mutex_unlock(&estado_jogo.mutex_jogo);
    
    close(socket_cliente);
    return NULL;
}


int main() {
    int socket_servidor, socket_cliente;
    struct sockaddr_in endereco_servidor, endereco_cliente;
    socklen_t tamanho_cliente = sizeof(endereco_cliente);
    
    inicializar_servidor();
    
    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_servidor < 0) {
        perror("Erro ao criar socket");
        return 1;
    }
    
    // Permite reutilizar a porta imediatamente
    int opt = 1;
    if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(socket_servidor);
        return 1;
    }
    
    endereco_servidor.sin_family = AF_INET;
    endereco_servidor.sin_addr.s_addr = INADDR_ANY;
    endereco_servidor.sin_port = htons(PORTA);
    
    if (bind(socket_servidor, (struct sockaddr*)&endereco_servidor, sizeof(endereco_servidor)) < 0) {
        perror("Erro no bind");
        close(socket_servidor);
        return 1;
    }
    
    if (listen(socket_servidor, MAX_JOGADORES) < 0) {
        perror("Erro no listen");
        close(socket_servidor);
        return 1;
    }

    printf("Aguardando jogadores...\n");
    
    while (1) {
        socket_cliente = accept(socket_servidor, (struct sockaddr*)&endereco_cliente, &tamanho_cliente);
        if (socket_cliente < 0) {
            perror("Erro no accept");
            continue;
        }
        
        printf("Nova conexao aceita\n");
        
        pthread_t thread_id;
        int* ptr_socket_cliente = (int*)malloc(sizeof(int));
        if (ptr_socket_cliente == NULL) {
            printf("Falha ao alocar memoria para socket do cliente.\n");
            close(socket_cliente);
            continue;
        }
        *ptr_socket_cliente = socket_cliente;
        
        if (pthread_create(&thread_id, NULL, gerenciar_cliente, ptr_socket_cliente) != 0) {
            perror("Erro ao criar thread");
            free(ptr_socket_cliente);
            close(socket_cliente);
        } else {
            pthread_detach(thread_id);
        }
    }
    
    close(socket_servidor);
    pthread_mutex_destroy(&estado_jogo.mutex_jogo);
    return 0;
}