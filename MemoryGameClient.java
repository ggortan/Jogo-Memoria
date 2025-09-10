import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.net.*;
import java.util.ArrayList;

public class MemoryGameClient extends JFrame {
    private static final String SERVER_HOST_DEFAULT = "192.168.15.38";
    private static final int SERVER_PORT_DEFAULT = 8080;
    private static final int BOARD_SIZE = 16;
    private static final String CONFIG_FILE = "server_config.txt";

    private Socket socket;
    private BufferedReader reader;
    private PrintWriter writer;
    private boolean connected = false;

    // GUI Components
    private JTextField nameField;
    private JButton connectButton;
    private JButton startButton;
    private JButton configButton;
    private JButton[] cardButtons;
    private JTextArea messageArea;
    private JTextArea scoreArea;
    private JLabel statusLabel;
    
    // Novos componentes de chat
    private JTextField chatInputField;
    private JButton sendChatButton;


    // Game state
    private String[] boardCards;
    private ArrayList<Integer> selectedCards;
    private boolean isMyTurn = false;
    private String playerName;

    // Server config
    private String serverHost;
    private int serverPort;

    public MemoryGameClient() {
        loadServerConfig();
        initializeGUI();
        boardCards = new String[BOARD_SIZE];
        selectedCards = new ArrayList<>();

        for (int i = 0; i < BOARD_SIZE; i++) {
            boardCards[i] = "X";
        }
    }

    private void loadServerConfig() {
        File configFile = new File(CONFIG_FILE);
        if (configFile.exists()) {
            try (BufferedReader br = new BufferedReader(new FileReader(configFile))) {
                String line = br.readLine();
                if (line != null && line.contains(":")) {
                    String[] parts = line.split(":");
                    serverHost = parts[0];
                    serverPort = Integer.parseInt(parts[1]);
                } else {
                    setDefaultServerConfig();
                }
            } catch (IOException e) {
                JOptionPane.showMessageDialog(this, "Erro ao ler arquivo de configuração: " + e.getMessage());
                setDefaultServerConfig();
            }
        } else {
            setDefaultServerConfig();
        }
    }

    private void setDefaultServerConfig() {
        serverHost = SERVER_HOST_DEFAULT;
        serverPort = SERVER_PORT_DEFAULT;
    }

    private void saveServerConfig(String host, int port) {
        try (PrintWriter pw = new PrintWriter(new FileWriter(CONFIG_FILE))) {
            pw.println(host + ":" + port);
            serverHost = host;
            serverPort = port;
        } catch (IOException e) {
            JOptionPane.showMessageDialog(this, "Erro ao salvar arquivo de configuração: " + e.getMessage());
        }
    }

    private void initializeGUI() {
        setTitle("Jogo de Memória");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(new BorderLayout(5, 5));

        // Painel superior com nome, botão de conexão e config
        JPanel topPanel = new JPanel(new FlowLayout());

        topPanel.add(new JLabel("Nome:"));
        nameField = new JTextField(15);
        topPanel.add(nameField);

        connectButton = new JButton("Conectar");
        connectButton.addActionListener(e -> {
            if (!connected) {
                playerName = nameField.getText().trim();
                if (playerName.isEmpty()) {
                    JOptionPane.showMessageDialog(MemoryGameClient.this, "Insira seu nome!");
                    return;
                }
                connectToServer();
            } else {
                disconnect();
            }
        });
        topPanel.add(connectButton);

        configButton = new JButton();
        configButton.setToolTipText("Configurar servidor");
        try {
            configButton.setText("\u2699"); 
            configButton.setFont(new Font("Dialog", Font.PLAIN, 18));
        } catch (Exception ex) {
            configButton.setText("Config");
        }
        configButton.addActionListener(e -> openConfigDialog());
        topPanel.add(configButton);

        add(topPanel, BorderLayout.NORTH);

        // Painel do tabuleiro
        JPanel gamePanel = new JPanel(new GridLayout(4, 4, 5, 5));
        gamePanel.setBorder(BorderFactory.createTitledBorder("Tabuleiro"));
        cardButtons = new JButton[BOARD_SIZE];

        for (int i = 0; i < BOARD_SIZE; i++) {
            cardButtons[i] = new JButton("?");
            cardButtons[i].setFont(new Font("Arial", Font.BOLD, 24));
            cardButtons[i].setPreferredSize(new Dimension(80, 80));
            cardButtons[i].setEnabled(false);

            final int index = i;
            cardButtons[i].addActionListener(e -> selectCard(index));
            gamePanel.add(cardButtons[i]);
        }
        add(gamePanel, BorderLayout.CENTER);

        // Painel direito com placar e mensagens
        JPanel rightPanel = new JPanel(new BorderLayout(5, 5));
        rightPanel.setPreferredSize(new Dimension(300, 0));

        scoreArea = new JTextArea(5, 25);
        scoreArea.setEditable(false);
        JScrollPane scoreScroll = new JScrollPane(scoreArea);
        scoreScroll.setBorder(BorderFactory.createTitledBorder("Placar"));
        rightPanel.add(scoreScroll, BorderLayout.NORTH);
        
        // Painel de chat (área de mensagens e campo de input)
        JPanel chatMessagePanel = new JPanel(new BorderLayout());
        chatMessagePanel.setBorder(BorderFactory.createTitledBorder("Mensagens"));

        messageArea = new JTextArea(10, 25);
        messageArea.setEditable(false);
        JScrollPane messageScroll = new JScrollPane(messageArea);
        chatMessagePanel.add(messageScroll, BorderLayout.CENTER);

        // Painel para campo de escrita e botão de envio
        JPanel inputPanel = new JPanel(new BorderLayout());
        chatInputField = new JTextField();
        sendChatButton = new JButton("➤");
        sendChatButton.setFont(new Font("Arial", Font.BOLD, 16));
        
        chatInputField.addActionListener(e -> sendMessage());
        sendChatButton.addActionListener(e -> sendMessage());
        
        inputPanel.add(chatInputField, BorderLayout.CENTER);
        inputPanel.add(sendChatButton, BorderLayout.EAST);

        chatMessagePanel.add(inputPanel, BorderLayout.SOUTH);
        rightPanel.add(chatMessagePanel, BorderLayout.CENTER);

        add(rightPanel, BorderLayout.EAST);

        // Painel inferior com controles do jogo e status
        JPanel bottomPanel = new JPanel(new FlowLayout(FlowLayout.LEFT, 10, 5));
        
        startButton = new JButton("Iniciar Jogo");
        startButton.setEnabled(false);
        startButton.addActionListener(e -> {
            if (connected && writer != null) {
                writer.println("START|");
                addMessage("Solicitando início...");
            }
        });
        bottomPanel.add(startButton);

        statusLabel = new JLabel("Desconectado");
        statusLabel.setBorder(BorderFactory.createEmptyBorder(0, 10, 0, 0));
        bottomPanel.add(statusLabel);
        
        add(bottomPanel, BorderLayout.SOUTH);

        pack();
        setLocationRelativeTo(null);
    }
    
    private void sendMessage() {
        String message = chatInputField.getText().trim();
        if (!message.isEmpty() && connected && writer != null) {
            writer.println("CHAT|" + message);
            chatInputField.setText("");
        }
    }

    private void openConfigDialog() {
        JTextField ipField = new JTextField(serverHost, 15);
        JTextField portField = new JTextField(String.valueOf(serverPort), 5);

        JPanel panel = new JPanel(new GridLayout(2, 2, 5, 5));
        panel.add(new JLabel("IP do servidor:"));
        panel.add(ipField);
        panel.add(new JLabel("Porta:"));
        panel.add(portField);

        int result = JOptionPane.showConfirmDialog(this, panel, "Configuração do Servidor",
                JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);

        if (result == JOptionPane.OK_OPTION) {
            String ip = ipField.getText().trim();
            String portStr = portField.getText().trim();
            if (ip.isEmpty() || portStr.isEmpty()) {
                JOptionPane.showMessageDialog(this, "IP e porta não podem ser vazios.");
                return;
            }
            int port;
            try {
                port = Integer.parseInt(portStr);
            } catch (NumberFormatException e) {
                JOptionPane.showMessageDialog(this, "Porta inválida.");
                return;
            }
            saveServerConfig(ip, port);
            addMessage("Configuração do servidor atualizada para " + ip + ":" + port);
        }
    }

    private void connectToServer() {
        try {
            socket = new Socket(serverHost, serverPort);
            reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            writer = new PrintWriter(socket.getOutputStream(), true);

            connected = true;
            connectButton.setText("Desconectar");
            nameField.setEnabled(false);
            startButton.setEnabled(true);
            statusLabel.setText("Conectado");

            writer.println("JOIN|" + playerName);

            Thread messageThread = new Thread(this::listenForMessages);
            messageThread.start();

            addMessage("Conectado ao servidor " + serverHost + ":" + serverPort);

        } catch (IOException e) {
            JOptionPane.showMessageDialog(this, "Erro ao conectar: " + e.getMessage());
        }
    }

    private void disconnect() {
        try {
            connected = false;
            if (socket != null) socket.close();
            if (reader != null) reader.close();
            if (writer != null) writer.close();

            connectButton.setText("Conectar");
            nameField.setEnabled(true);
            startButton.setEnabled(false);
            statusLabel.setText("Desconectado");

            resetGameBoard();
            addMessage("Desconectado.");

        } catch (IOException e) {
            System.err.println("Erro ao desconectar: " + e.getMessage());
        }
    }

private void listenForMessages() {
    try {
        String message;
        while (connected && (message = reader.readLine()) != null) {
            System.out.println("<<< RAW DO SERVIDOR: [" + message + "]");
            final String finalMessage = message;
            SwingUtilities.invokeLater(() -> processMessage(finalMessage));
        }
    } catch (IOException e) {
        if (connected) {
            SwingUtilities.invokeLater(() -> {
                addMessage("Conexão perdida!");
                disconnect();
            });
        }
    }
}
// Correções principais no método processMessage:

private void processMessage(String message) {
    String[] parts = message.split("\\|");
    String command = parts[0].trim();
    System.out.println("Command interpretado: [" + command + "]");

    switch (command) {
        case "WELCOME": addMessage("Bem-vindo! " + parts[1]); break;
        case "PLAYER_JOIN": addMessage(parts[1]); break;
        case "PLAYER_LEFT": addMessage(parts[1]); break;
        case "TEST": addMessage("Mensagem de teste recebida!"); break;
        case "CHAT": 
            addMessage(parts[1]);
            break;
        case "GAME_START":
            addMessage("Jogo iniciado!");
            startButton.setEnabled(false);
            break;
        case "BOARD":
            updateBoard(parts[1]);
            break;
        case "SCORES":
            updateScores(parts[1]);
            break;
        case "TURN":
            String currentPlayerName = parts[2];
            isMyTurn = currentPlayerName.equals(playerName);
            if (isMyTurn) {
                statusLabel.setText("Sua vez!");
                enableGameBoard(true);
            } else {
                statusLabel.setText("Vez de: " + currentPlayerName);
                enableGameBoard(false);
            }
            // CORREÇÃO: Limpa as cartas selecionadas quando muda o turno
            selectedCards.clear();
            // Remove highlight das cartas previamente selecionadas
            for (int i = 0; i < BOARD_SIZE; i++) {
                if (boardCards[i].equals("X")) {
                    cardButtons[i].setBackground(null);
                }
            }
            break;
        case "REVEAL":
            String[] positions = parts[1].split(",");
            String[] values = parts[2].split(",");
            int pos1 = Integer.parseInt(positions[0]);
            int pos2 = Integer.parseInt(positions[1]);
            String val1 = values[0];
            String val2 = values[1];

            cardButtons[pos1].setText(val1);
            cardButtons[pos2].setText(val2);
            cardButtons[pos1].setBackground(Color.YELLOW);
            cardButtons[pos2].setBackground(Color.YELLOW);

            addMessage("Cartas: " + val1 + " e " + val2);

            // CORREÇÃO: Timer mais inteligente que verifica se as cartas ainda devem estar visíveis
            Timer timer = new Timer(2000, e -> {
                // Só esconde se não for um match permanente
                if (boardCards[pos1].equals("X")) {
                    cardButtons[pos1].setText("?");
                    cardButtons[pos1].setBackground(null);
                }
                if (boardCards[pos2].equals("X")) {
                    cardButtons[pos2].setText("?");
                    cardButtons[pos2].setBackground(null);
                }
            });
            timer.setRepeats(false);
            timer.start();
            break;
        case "MATCH":
            addMessage("Par encontrado!");
            // CORREÇÃO: Limpa as cartas selecionadas também no caso de match
            selectedCards.clear();
            break;
        case "NO_MATCH":
            addMessage("Não é um par!");
            selectedCards.clear();
            break;
        case "GAME_END":
            addMessage("Fim de jogo! " + parts[1]);
            enableGameBoard(false);
            statusLabel.setText("Finalizado");
            startButton.setEnabled(true);
            // CORREÇÃO: Limpa seleções no fim do jogo
            selectedCards.clear();
            break;
        case "ERROR":
            JOptionPane.showMessageDialog(this, "Erro: " + parts[1]);
            selectedCards.clear();
            break;
    }
}

// CORREÇÃO: Método updateBoard melhorado
private void updateBoard(String boardData) {
    String[] cards = boardData.split(",");
    for (int i = 0; i < BOARD_SIZE && i < cards.length; i++) {
        boardCards[i] = cards[i];
        if (!cards[i].equals("X")) {
            // Carta revelada permanentemente (match encontrado)
            cardButtons[i].setText(cards[i]);
            cardButtons[i].setEnabled(false);
            cardButtons[i].setBackground(Color.GREEN);
        } else {
            // Carta ainda oculta
            cardButtons[i].setText("?");
            cardButtons[i].setBackground(null);
            // Só habilita se for o turno do jogador
            cardButtons[i].setEnabled(isMyTurn);
        }
    }
}

// CORREÇÃO: Método selectCard melhorado
private void selectCard(int index) {
    if (!isMyTurn || !cardButtons[index].isEnabled() || selectedCards.size() >= 2) {
        return;
    }

    // Verifica se a carta já foi selecionada
    for (Integer selectedIndex : selectedCards) {
        if (selectedIndex.intValue() == index) {
            return; // Carta já selecionada
        }
    }

    selectedCards.add(Integer.valueOf(index));
    cardButtons[index].setBackground(Color.CYAN);

    if (selectedCards.size() == 2) {
        int pos1 = selectedCards.get(0).intValue();
        int pos2 = selectedCards.get(1).intValue();
        writer.println("MOVE|" + pos1 + "," + pos2);

        enableGameBoard(false);
        addMessage("Escolheu: " + pos1 + " e " + pos2);
        
        // CORREÇÃO: Não limpa selectedCards aqui, será limpo quando receber a resposta
    }
}

    private void updateScores(String scoreData) {
        String[] scores = scoreData.split(",");
        StringBuilder scoreText = new StringBuilder();

        for (String score : scores) {
            if (!score.trim().isEmpty()) {
                String[] playerScore = score.split(":");
                if (playerScore.length == 2) {
                    scoreText.append(playerScore[0]).append(": ")
                            .append(playerScore[1]).append(" pares\n");
                }
            }
        }

        scoreArea.setText(scoreText.toString());
    }

    private void enableGameBoard(boolean enable) {
        for (int i = 0; i < BOARD_SIZE; i++) {
            if (boardCards[i].equals("X")) {
                cardButtons[i].setEnabled(enable);
            }
        }
    }

    private void resetGameBoard() {
        selectedCards.clear();
        isMyTurn = false;

        for (int i = 0; i < BOARD_SIZE; i++) {
            boardCards[i] = "X";
            cardButtons[i].setText("?");
            cardButtons[i].setEnabled(false);
            cardButtons[i].setBackground(null);
        }

        scoreArea.setText("");
        statusLabel.setText("Aguardando...");
    }

    private void addMessage(String msg) {
        messageArea.append(msg + "\n");
        messageArea.setCaretPosition(messageArea.getDocument().getLength());
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new MemoryGameClient().setVisible(true));
    }
}
