# Jogo da Memória Multijogador

Este projeto é um **Jogo da Memória multijogador** desenvolvido como trabalho para a disciplina de **Sistemas Distribuídos**.

## Descrição

O jogo utiliza a arquitetura cliente-servidor, empregando **sockets** para a comunicação entre as partes. O **servidor** foi desenvolvido em **C** e os **clientes** em **Java**, utilizando a biblioteca **Java Swing** para a interface gráfica.

- **Servidor:** Responsável por controlar a lógica do jogo, gerenciar as conexões dos jogadores e sincronizar o andamento da partida.
- **Cliente:** Permite que os jogadores interajam com o jogo, enviando suas jogadas para o servidor e recebendo atualizações em tempo real.

> **Atenção:** Este projeto foi projetado para ser executado em **Windows**.

## Funcionalidades

- Jogo da memória clássico, adaptado para múltiplos jogadores em rede.
- Comunicação eficiente via sockets TCP/IP.
- Interface gráfica amigável para os clientes (Java Swing).
- Gerenciamento de partidas e pontuação pelo servidor.

## Estrutura do Projeto

```
/
├── cliente/      # Implementação dos clientes em Java
├── servidor/     # Implementação do servidor em C
└── README.md
```

## Como Executar

### Pré-requisitos

- **Servidor:** Compilador C para Windows (por exemplo, MinGW, Code::Blocks, Dev-C++)
- **Cliente:** Java JDK 8+ instalado no Windows

### Passos

1. **Compile o servidor (em C):**
   ```sh
   cd servidor
   gcc -o memory_server_windows.exe memory_server_windows.c
   ```

2. **Execute o servidor:**
   ```sh
   memory_server_windows.exe
   ```

3. **Compile o cliente (em Java):**
   ```sh
   cd ../cliente
   javac JogoMemoriaCliente.java
   ```

4. **Execute o cliente:**
   ```sh
   java JogoMemoriaCliente.java
   ```

5. **Conecte múltiplos clientes** ao servidor para jogar partidas simultâneas.

## Contribuição

Sinta-se à vontade para abrir issues ou enviar pull requests com melhorias, correções de bugs ou sugestões.

## Licença

Este projeto é de uso acadêmico e está aberto para fins educacionais.

---

Desenvolvido por Gabriel Gortan para a disciplina de **Sistemas Distribuídos**.
