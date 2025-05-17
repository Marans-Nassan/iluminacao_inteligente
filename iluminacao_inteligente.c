#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwipopts.h"

// Definições de rede e pinos
#define WIFI_SSID "S.F.C 2"
#define WIFI_PASSWORD "857aj431"
#define LED_PIN CYW43_WL_GPIO_LED_PIN  // LED de status Wi-Fi (Lâmpada Especial)
#define green_led 11                   // LED verde (Lâmpada 2)
#define blue_led 12                    // LED azul (Lâmpada 1)
#define red_led 13                     // LED vermelho (Lâmpada 3)
#define buzzer_a 21                    // Pino do buzzer (Sirene)
#define tamanho_max 64
#define timeout_max 1000

// Configuração PWM para buzzer
uint8_t slice = 0;
typedef struct {
    float dc;           // wrap value do PWM
    float div;          // divisor de clock
    bool alarm_state;   // estado da sirene
} pwm_struct;
pwm_struct pw = {7812.5, 32.0, false};

bool led_pin_e = false;

char rede[tamanho_max / 2];
char senha[tamanho_max];


// Protótipos de funções
void ledinit(void);
void pwm_setup(void);
void pwm_on(uint8_t duty_cycle);
void pwm_off(void);
void read_line(char *buffer, size_t max_len);
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
float temp_read(void);
void user_request(char **request);

int main() {
    stdio_init_all();         // Init UART padrão
    ledinit();                // Configura LEDs GPIO
    pwm_setup();              // Configura PWM para buzzer
    sleep_ms(3000);           // Tempo para o stdio_init_all() iniciar a conexão UART + Tempo para verificar no Monitor.
    printf("Digite o nome da rede: \n");
    read_line(rede, tamanho_max / 2);
    printf("Rede: %s\n", rede);

    printf("Digite a senha: \n");
    read_line(senha, tamanho_max);
    printf("Senha: %s\n", senha);


    // Inicializa Wi-Fi
    if (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        return -1;
    }

    cyw43_arch_gpio_put(LED_PIN, 0);
    cyw43_arch_enable_sta_mode();  // Modo estação

    printf("Conectando ao Wi-Fi...\n");
    // Tenta conectar com timeout
    if (cyw43_arch_wifi_connect_timeout_ms(rede, senha, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        cyw43_arch_deinit();
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    //WIFI_SSID, WIFI_PASSWORD

    // Exibe IP se existir interface
    if (netif_default) {
        printf("IP do dispositivo: %s\n", ip4addr_ntoa(&netif_default->ip_addr));
    }

    // Cria servidor TCP na porta 80
    struct tcp_pcb *server = tcp_new();
    if (!server) {
        printf("Falha ao criar servidor TCP\n");
        cyw43_arch_deinit();
        return -1;
    }
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Falha ao associar servidor TCP à porta 80\n");
        tcp_close(server);
        cyw43_arch_deinit();
        return -1;
    }
    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    // Inicializa ADC para leitura de temperatura
    adc_init();
    adc_set_temp_sensor_enabled(true);

    // Loop principal: trata eventos Wi-Fi e mantém o programa ativo
    while (true) {
        cyw43_arch_poll();
        sleep_ms(20);
    }

    cyw43_arch_deinit();
    return 0;
}

// Configura pinos dos LEDs
void ledinit(void) {
    for (uint8_t i = 11; i < 14; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
        gpio_put(i, 0);
    }
}

// Configura PWM no pino do buzzer
void pwm_setup(void) {
    gpio_set_function(buzzer_a, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(buzzer_a);
    pwm_set_clkdiv(slice, pw.div);
    pwm_set_wrap(slice, pw.dc);
    pwm_set_enabled(slice, false);
}

// Liga PWM com duty cycle em %
void pwm_on(uint8_t duty_cycle) {
    gpio_set_function(buzzer_a, GPIO_FUNC_PWM);
    pwm_set_gpio_level(buzzer_a, (uint16_t)((pw.dc * duty_cycle) / 100));
    pwm_set_enabled(slice, true);
}

// Desliga PWM e zera pino
void pwm_off(void) {
    pwm_set_enabled(slice, false);
    gpio_set_function(buzzer_a, GPIO_FUNC_SIO);
    gpio_put(buzzer_a, 0);
}

void read_line(char *buffer, size_t max_len){
    size_t idx = 0;
    while(idx < max_len - 1){
        int c = getchar_timeout_us(timeout_max);
        if(c == PICO_ERROR_TIMEOUT){
            if(idx > 0)break;
            else continue;
        }
        if (c == '\r' || c == '\n' || c < 0) break;
        buffer[idx++] = (char)c;
    }
    buffer[idx] = '\0';
}

// Callback ao aceitar conexão TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Processa requisição de controle das luzes e sirene
void user_request(char **request) {
    if (strstr(*request, "GET /luz_1") != NULL) gpio_put(blue_led, !gpio_get(blue_led));
    else if (strstr(*request, "GET /luz_2") != NULL) gpio_put(green_led, !gpio_get(green_led));
    else if (strstr(*request, "GET /luz_3") != NULL) gpio_put(red_led, !gpio_get(red_led));
    else if (strstr(*request, "GET /luz_e") != NULL) {
        led_pin_e = !led_pin_e;
        cyw43_arch_gpio_put(LED_PIN, led_pin_e);
    } else if (strstr(*request, "GET /sirene") != NULL) {
        pw.alarm_state = !pw.alarm_state;
        if (pw.alarm_state) pwm_on(50);
        else pwm_off();
    }
}

// Lê temperatura interna do sensor
float temp_read(void) {
    adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float voltage = raw_value * conversion_factor;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

// Callback ao receber dados TCP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Copia requisição HTTP para string
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    user_request(&request);  // Atualiza estado conforme URL

    // Gera página HTML com botões e temperatura
    float temperature = temp_read();
    char html[1024];
    snprintf(html, sizeof(html),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n\r\n"
             "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
             "<title>Iluminação Inteligente</title>"
             "<style>body{background-color:#012361;color:white;font-family:Arial;text-align:center;}"
             "button{background-color:lightgray;font-size:36px;padding:20px 40px;margin:10px;border-radius:10px;}"
             ".temperature{font-size:48px;margin-top:30px;}</style></head>"
             "<body><h1>Iluminação: Lâmpadas</h1><h2>Sirene Especial</h2>"
             "<form action=\"./luz_1\"><button>%s Lâmpada 1</button></form>"
             "<form action=\"./luz_2\"><button>%s Lâmpada 2</button></form>"
             "<form action=\"./luz_3\"><button>%s Lâmpada 3</button></form>"
             "<form action=\"./luz_e\"><button>%s Lâmpada E</button></form>"
             "<form action=\"./sirene\"><button>%s Sirene</button></form>"
             "<p class='temperature'>Temperatura: %.2f°C</p></body></html>",
             gpio_get(blue_led) ? "Desativar" : "Ativar",
             gpio_get(green_led) ? "Desativar" : "Ativar",
             gpio_get(red_led) ? "Desativar" : "Ativar",
             led_pin_e ? "Desativar" : "Ativar",
             pw.alarm_state ? "Desativar" : "Ativar", temperature);

    // Envia resposta e libera memória
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    free(request);
    pbuf_free(p);
    return ERR_OK;
}