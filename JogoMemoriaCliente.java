import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.net.*;
import java.util.ArrayList;

public class JogoMemoriaCliente extends JFrame {
    private static final String HOST_SERVIDOR_PADRAO = "localhost";
    private static final int PORTA_SERVIDOR_PADRAO = 8080;
    private static final int TAMANHO_TABULEIRO = 16;
    private static final String ARQUIVO_CONFIG = "server_config.txt";

    private Socket socket;
    private BufferedReader leitor;
    private PrintWriter escritor;
    private boolean conectado = false;

    private JTextField campoNome;
    private JButton botaoConectar;
    private JButton botaoIniciar;
    private JButton botaoConfig;
    private JButton[] botoesCartas;
    private JTextArea areaMensagens;
    private JTextArea areaPontuacao;
    private JLabel labelStatus;
    
    private JTextField campoEntradaChat;
    private JButton botaoEnviarChat;

    private String[] cartasTabuleiro;
    private ArrayList<Integer> cartasSelecionadas;
    private boolean ehMinhaVez = false;
    private String nomeJogador;

    private String hostServidor;
    private int portaServidor;

    public JogoMemoriaCliente() {
        carregarConfigServidor();
        inicializarInterface();
        cartasTabuleiro = new String[TAMANHO_TABULEIRO];
        cartasSelecionadas = new ArrayList<>();

        for (int i = 0; i < TAMANHO_TABULEIRO; i++) {
            cartasTabuleiro[i] = "X";
        }
    }

    private void carregarConfigServidor() {
        File arquivoConfig = new File(ARQUIVO_CONFIG);
        if (arquivoConfig.exists()) {
            try (BufferedReader br = new BufferedReader(new FileReader(arquivoConfig))) {
                String linha = br.readLine();
                if (linha != null && linha.contains(":")) {
                    String[] partes = linha.split(":");
                    hostServidor = partes[0];
                    portaServidor = Integer.parseInt(partes[1]);
                } else {
                    definirConfigPadrao();
                }
            } catch (IOException e) {
                JOptionPane.showMessageDialog(this, "Erro ao ler arquivo de configuração: " + e.getMessage());
                definirConfigPadrao();
            }
        } else {
            definirConfigPadrao();
        }
    }

    private void definirConfigPadrao() {
        hostServidor = HOST_SERVIDOR_PADRAO;
        portaServidor = PORTA_SERVIDOR_PADRAO;
    }

    private void salvarConfigServidor(String host, int porta) {
        try (PrintWriter pw = new PrintWriter(new FileWriter(ARQUIVO_CONFIG))) {
            pw.println(host + ":" + porta);
            hostServidor = host;
            portaServidor = porta;
        } catch (IOException e) {
            JOptionPane.showMessageDialog(this, "Erro ao salvar arquivo de configuração: " + e.getMessage());
        }
    }

    private void inicializarInterface() {
        setTitle("Jogo de Memória");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(new BorderLayout(5, 5));

        JPanel painelSuperior = new JPanel(new FlowLayout());

        painelSuperior.add(new JLabel("Nome:"));
        campoNome = new JTextField(15);
        painelSuperior.add(campoNome);

        botaoConectar = new JButton("Conectar");
        botaoConectar.addActionListener(e -> {
            if (!conectado) {
                nomeJogador = campoNome.getText().trim();
                if (nomeJogador.isEmpty()) {
                    JOptionPane.showMessageDialog(JogoMemoriaCliente.this, "Insira seu nome!");
                    return;
                }
                conectarAoServidor();
            } else {
                desconectar();
            }
        });
        painelSuperior.add(botaoConectar);

        botaoConfig = new JButton();
        botaoConfig.setToolTipText("Configurar servidor");
        try {
            botaoConfig.setText("\u2699"); 
            botaoConfig.setFont(new Font("Dialog", Font.PLAIN, 18));
        } catch (Exception ex) {
            botaoConfig.setText("Config");
        }
        botaoConfig.addActionListener(e -> abrirDialogoConfig());
        painelSuperior.add(botaoConfig);

        add(painelSuperior, BorderLayout.NORTH);

        JPanel painelJogo = new JPanel(new GridLayout(4, 4, 5, 5));
        painelJogo.setBorder(BorderFactory.createTitledBorder("Tabuleiro"));
        botoesCartas = new JButton[TAMANHO_TABULEIRO];

        for (int i = 0; i < TAMANHO_TABULEIRO; i++) {
            botoesCartas[i] = new JButton("?");
            botoesCartas[i].setFont(new Font("Arial", Font.BOLD, 24));
            botoesCartas[i].setPreferredSize(new Dimension(80, 80));
            botoesCartas[i].setEnabled(false);

            final int indice = i;
            botoesCartas[i].addActionListener(e -> selecionarCarta(indice));
            painelJogo.add(botoesCartas[i]);
        }
        add(painelJogo, BorderLayout.CENTER);

        JPanel painelDireito = new JPanel(new BorderLayout(5, 5));
        painelDireito.setPreferredSize(new Dimension(300, 0));

        areaPontuacao = new JTextArea(5, 25);
        areaPontuacao.setEditable(false);
        JScrollPane scrollPontuacao = new JScrollPane(areaPontuacao);
        scrollPontuacao.setBorder(BorderFactory.createTitledBorder("Placar"));
        painelDireito.add(scrollPontuacao, BorderLayout.NORTH);
        
        JPanel painelMensagemChat = new JPanel(new BorderLayout());
        painelMensagemChat.setBorder(BorderFactory.createTitledBorder("Mensagens"));

        areaMensagens = new JTextArea(10, 25);
        areaMensagens.setEditable(false);
        JScrollPane scrollMensagens = new JScrollPane(areaMensagens);
        painelMensagemChat.add(scrollMensagens, BorderLayout.CENTER);

        JPanel painelEntrada = new JPanel(new BorderLayout());
        campoEntradaChat = new JTextField();
        botaoEnviarChat = new JButton("➤");
        botaoEnviarChat.setFont(new Font("Arial", Font.BOLD, 16));
        
        campoEntradaChat.addActionListener(e -> enviarMensagem());
        botaoEnviarChat.addActionListener(e -> enviarMensagem());
        
        painelEntrada.add(campoEntradaChat, BorderLayout.CENTER);
        painelEntrada.add(botaoEnviarChat, BorderLayout.EAST);

        painelMensagemChat.add(painelEntrada, BorderLayout.SOUTH);
        painelDireito.add(painelMensagemChat, BorderLayout.CENTER);

        add(painelDireito, BorderLayout.EAST);

        JPanel painelInferior = new JPanel(new FlowLayout(FlowLayout.LEFT, 10, 5));
        
        botaoIniciar = new JButton("Iniciar Jogo");
        botaoIniciar.setEnabled(false);
        botaoIniciar.addActionListener(e -> {
            if (conectado && escritor != null) {
                escritor.println("START|");
                adicionarMensagem("Solicitando início...");
            }
        });
        painelInferior.add(botaoIniciar);

        labelStatus = new JLabel("Desconectado");
        labelStatus.setBorder(BorderFactory.createEmptyBorder(0, 10, 0, 0));
        painelInferior.add(labelStatus);
        
        add(painelInferior, BorderLayout.SOUTH);

        pack();
        setLocationRelativeTo(null);
    }
    
    private void enviarMensagem() {
        String mensagem = campoEntradaChat.getText().trim();
        if (!mensagem.isEmpty() && conectado && escritor != null) {
            escritor.println("CHAT|" + mensagem);
            campoEntradaChat.setText("");
        }
    }

    private void abrirDialogoConfig() {
        JTextField campoIp = new JTextField(hostServidor, 15);
        JTextField campoPorta = new JTextField(String.valueOf(portaServidor), 5);

        JPanel painel = new JPanel(new GridLayout(2, 2, 5, 5));
        painel.add(new JLabel("IP do servidor:"));
        painel.add(campoIp);
        painel.add(new JLabel("Porta:"));
        painel.add(campoPorta);

        int resultado = JOptionPane.showConfirmDialog(this, painel, "Configuração do Servidor",
                JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);

        if (resultado == JOptionPane.OK_OPTION) {
            String ip = campoIp.getText().trim();
            String strPorta = campoPorta.getText().trim();
            if (ip.isEmpty() || strPorta.isEmpty()) {
                JOptionPane.showMessageDialog(this, "IP e porta não podem ser vazios.");
                return;
            }
            int porta;
            try {
                porta = Integer.parseInt(strPorta);
            } catch (NumberFormatException e) {
                JOptionPane.showMessageDialog(this, "Porta inválida.");
                return;
            }
            salvarConfigServidor(ip, porta);
            adicionarMensagem("Configuração do servidor atualizada para " + ip + ":" + porta);
        }
    }

    private void conectarAoServidor() {
        try {
            socket = new Socket(hostServidor, portaServidor);
            leitor = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            escritor = new PrintWriter(socket.getOutputStream(), true);

            conectado = true;
            botaoConectar.setText("Desconectar");
            campoNome.setEnabled(false);
            botaoIniciar.setEnabled(true);
            labelStatus.setText("Conectado");

            escritor.println("JOIN|" + nomeJogador);

            Thread threadMensagens = new Thread(this::escutarMensagens);
            threadMensagens.start();

            adicionarMensagem("Conectado ao servidor " + hostServidor + ":" + portaServidor);

        } catch (IOException e) {
            JOptionPane.showMessageDialog(this, "Erro ao conectar: " + e.getMessage());
        }
    }

    private void desconectar() {
        try {
            conectado = false;
            if (socket != null) socket.close();
            if (leitor != null) leitor.close();
            if (escritor != null) escritor.close();

            botaoConectar.setText("Conectar");
            campoNome.setEnabled(true);
            botaoIniciar.setEnabled(false);
            labelStatus.setText("Desconectado");

            reiniciarTabuleiro();
            adicionarMensagem("Desconectado.");

        } catch (IOException e) {
            System.err.println("Erro ao desconectar: " + e.getMessage());
        }
    }

    private void escutarMensagens() {
        try {
            String mensagem;
            while (conectado && (mensagem = leitor.readLine()) != null) {
                System.out.println("<<< RAW DO SERVIDOR: [" + mensagem + "]");
                final String mensagemFinal = mensagem;
                SwingUtilities.invokeLater(() -> processarMensagem(mensagemFinal));
            }
        } catch (IOException e) {
            if (conectado) {
                SwingUtilities.invokeLater(() -> {
                    adicionarMensagem("Conexão perdida!");
                    desconectar();
                });
            }
        }
    }

    private void processarMensagem(String mensagem) {
        String[] partes = mensagem.split("\\|");
        String comando = partes[0].trim();
        System.out.println("Comando interpretado: [" + comando + "]");

        switch (comando) {
            case "WELCOME": adicionarMensagem("Bem-vindo! " + partes[1]); break;
            case "PLAYER_JOIN": adicionarMensagem(partes[1]); break;
            case "PLAYER_LEFT": adicionarMensagem(partes[1]); break;
            case "TEST": adicionarMensagem("Mensagem de teste recebida!"); break;
            case "CHAT": 
                adicionarMensagem(partes[1]);
                break;
            case "GAME_START":
                adicionarMensagem("Jogo iniciado!");
                botaoIniciar.setEnabled(false);
                break;
            case "BOARD":
                atualizarTabuleiro(partes[1]);
                break;
            case "SCORES":
                atualizarPontuacoes(partes[1]);
                break;
            case "TURN":
                String nomeJogadorAtual = partes[2];
                ehMinhaVez = nomeJogadorAtual.equals(nomeJogador);
                if (ehMinhaVez) {
                    labelStatus.setText("Sua vez!");
                    habilitarTabuleiro(true);
                } else {
                    labelStatus.setText("Vez de: " + nomeJogadorAtual);
                    habilitarTabuleiro(false);
                }
                cartasSelecionadas.clear();
                for (int i = 0; i < TAMANHO_TABULEIRO; i++) {
                    if (cartasTabuleiro[i].equals("X")) {
                        botoesCartas[i].setBackground(null);
                    }
                }
                break;
            case "REVEAL":
                String[] posicoes = partes[1].split(",");
                String[] valores = partes[2].split(",");
                int pos1 = Integer.parseInt(posicoes[0]);
                int pos2 = Integer.parseInt(posicoes[1]);
                String val1 = valores[0];
                String val2 = valores[1];

                botoesCartas[pos1].setText(val1);
                botoesCartas[pos2].setText(val2);
                botoesCartas[pos1].setBackground(Color.YELLOW);
                botoesCartas[pos2].setBackground(Color.YELLOW);

                adicionarMensagem("Cartas: " + val1 + " e " + val2);

                Timer timer = new Timer(2000, e -> {
                    if (cartasTabuleiro[pos1].equals("X")) {
                        botoesCartas[pos1].setText("?");
                        botoesCartas[pos1].setBackground(null);
                    }
                    if (cartasTabuleiro[pos2].equals("X")) {
                        botoesCartas[pos2].setText("?");
                        botoesCartas[pos2].setBackground(null);
                    }
                });
                timer.setRepeats(false);
                timer.start();
                break;
            case "MATCH":
                adicionarMensagem("Par encontrado!");
                cartasSelecionadas.clear();
                break;
            case "NO_MATCH":
                adicionarMensagem("Não é um par!");
                cartasSelecionadas.clear();
                break;
            case "GAME_END":
                adicionarMensagem("Fim de jogo! " + partes[1]);
                habilitarTabuleiro(false);
                labelStatus.setText("Finalizado");
                botaoIniciar.setEnabled(true);
                cartasSelecionadas.clear();
                break;
            case "ERROR":
                JOptionPane.showMessageDialog(this, "Erro: " + partes[1]);
                cartasSelecionadas.clear();
                break;
        }
    }

    private void atualizarTabuleiro(String dadosTabuleiro) {
        String[] cartas = dadosTabuleiro.split(",");
        for (int i = 0; i < TAMANHO_TABULEIRO && i < cartas.length; i++) {
            cartasTabuleiro[i] = cartas[i];
            if (!cartas[i].equals("X")) {
                botoesCartas[i].setText(cartas[i]);
                botoesCartas[i].setEnabled(false);
                botoesCartas[i].setBackground(Color.GREEN);
            } else {
                botoesCartas[i].setText("?");
                botoesCartas[i].setBackground(null);
                botoesCartas[i].setEnabled(ehMinhaVez);
            }
        }
    }

    private void selecionarCarta(int indice) {
        if (!ehMinhaVez || !botoesCartas[indice].isEnabled() || cartasSelecionadas.size() >= 2) {
            return;
        }

        for (Integer indiceSelecionado : cartasSelecionadas) {
            if (indiceSelecionado.intValue() == indice) {
                return;
            }
        }

        cartasSelecionadas.add(Integer.valueOf(indice));
        botoesCartas[indice].setBackground(Color.CYAN);

        if (cartasSelecionadas.size() == 2) {
            int pos1 = cartasSelecionadas.get(0).intValue();
            int pos2 = cartasSelecionadas.get(1).intValue();
            escritor.println("MOVE|" + pos1 + "," + pos2);

            habilitarTabuleiro(false);
            adicionarMensagem("Escolheu: " + pos1 + " e " + pos2);
        }
    }

    private void atualizarPontuacoes(String dadosPontuacao) {
        String[] pontuacoes = dadosPontuacao.split(",");
        StringBuilder textoPontuacao = new StringBuilder();

        for (String pontuacao : pontuacoes) {
            if (!pontuacao.trim().isEmpty()) {
                String[] pontuacaoJogador = pontuacao.split(":");
                if (pontuacaoJogador.length == 2) {
                    textoPontuacao.append(pontuacaoJogador[0]).append(": ")
                            .append(pontuacaoJogador[1]).append(" pares\n");
                }
            }
        }

        areaPontuacao.setText(textoPontuacao.toString());
    }

    private void habilitarTabuleiro(boolean habilitar) {
        for (int i = 0; i < TAMANHO_TABULEIRO; i++) {
            if (cartasTabuleiro[i].equals("X")) {
                botoesCartas[i].setEnabled(habilitar);
            }
        }
    }

    private void reiniciarTabuleiro() {
        cartasSelecionadas.clear();
        ehMinhaVez = false;

        for (int i = 0; i < TAMANHO_TABULEIRO; i++) {
            cartasTabuleiro[i] = "X";
            botoesCartas[i].setText("?");
            botoesCartas[i].setEnabled(false);
            botoesCartas[i].setBackground(null);
        }

        areaPontuacao.setText("");
        labelStatus.setText("Aguardando...");
    }

    private void adicionarMensagem(String msg) {
        areaMensagens.append(msg + "\n");
        areaMensagens.setCaretPosition(areaMensagens.getDocument().getLength());
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new JogoMemoriaCliente().setVisible(true));
    }
}
