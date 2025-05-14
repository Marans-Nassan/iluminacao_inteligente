# Iluminação Inteligente via Web com Raspberry Pi Pico W

## Descrição

Projeto para controle de iluminação e sirene via interface web, utilizando Raspberry Pi Pico W (RP2040) e LWIP/TCP. O sistema permite ligar/desligar lâmpadas, acionar sirene e monitorar temperatura do sensor interno.

## Componentes e Pinos

| Componente            | GPIO/Pino                 | Função                         |
| --------------------- | ------------------------- | ------------------------------ |
| LED de Estado Wi‑Fi   | CYW43\_WL\_GPIO\_LED\_PIN | Indica conexão Wi‑Fi           |
| Lâmpada 1 (Azul)      | GPIO 12                   | Controle via web (GET /luz\_1) |
| Lâmpada 2 (Verde)     | GPIO 11                   | Controle via web (GET /luz\_2) |
| Lâmpada 3 (Vermelho)  | GPIO 13                   | Controle via web (GET /luz\_3) |
| LED Extra             | LED\_PIN alternativo      | Toggle via GET /luz\_e         |
| Buzzer/Sirene         | GPIO 21                   | Alerta sonoro (GET /sirene)    |
| Sensor de Temperatura | ADC\_CH4                  | Leitura interna do RP2040      |

## Operação

1. **Inicialização**

   * Conecta ao Wi‑Fi (SSID/Senha definidos).
   * Inicia servidor TCP ouvindo na porta 80.
2. **Interface Web**

   * Acessar `http://<IP_do_Pico>/` no navegador.
   * Botões na página enviam requisições HTTP GET:

     * `/luz_1`, `/luz_2`, `/luz_3`: alterna estado das lâmpadas 1, 2 e 3.
     * `/luz_e`: alterna LED de status extra.
     * `/sirene`: ativa/desativa o buzzer.
   * A página exibe a temperatura atual do sensor interno.
3. **Lógica Interna**

   * Requisições recebidas em `tcp_server_recv()`, parse do request e aciona `user_request()`.
   * Leitura de temperatura com `temp_read()`.
   * Responde com HTML gerado dinamicamente.

## Instalação

1. **Pré-requisitos**

   * Raspberry Pi Pico W e cabos USB.
   * Pico SDK, CMake e toolchain ARM.
2. **Configuração**

   * Ajustar `WIFI_SSID` e `WIFI_PASSWORD` no código.
3. **Compilação**

   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

   Ou import utilizando a extensão do Raspberry Pi Pico do VSCode.

4. **Deploy**

   * Copie o `.uf2` para o Pico W em modo bootloader.
   * Pico iniciará e exibirá IP no terminal serial.

## Autor

Hugo Martins Santana (TIC370101267)