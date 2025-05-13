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

#define WIFI_SSID "S.F.C 2"
#define WIFI_PASSWORD "857aj431"
#define LED_PIN CYW43_WL_GPIO_LED_PIN
#define green_led 11
#define blue_led 12
#define red_led 13
#define buzzer_a 21

uint8_t slice = 0;
typedef struct{
    float dc;
    float div;
    bool vol_up_down;
    bool alarm_state;
} pwm_struct;
pwm_struct p = {39062.5, 1.0, false, false};

void ledinit(void);
int pwm_setup(void);
void pwm_on(void);
void pwm_off(void);
int64_t controle_pwm_crescendo(alarm_id_t id, void *user_data);
int64_t controle_pwm_decrescendo(alarm_id_t id, void *user_data);
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
float temp_read(void);
void user_request(char **request);

int main(){
    stdio_init_all();
    ledinit();
    pwm_setup();

    while (cyw43_arch_init()){
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    cyw43_arch_gpio_put(LED_PIN, 0);
    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)){
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    if (netif_default){
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    struct tcp_pcb *server = tcp_new();
    if (!server){
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK){
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    adc_init();
    adc_set_temp_sensor_enabled(true);

    while (true){
        cyw43_arch_poll();
        sleep_ms(20);
    }

    cyw43_arch_deinit();
    return 0;
}

void ledinit(void){
    for(uint8_t i = 11; i < 14; i ++){
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
        gpio_put (i, 0);
    }
}

int pwm_setup(void){
    gpio_set_function(buzzer_a, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(buzzer_a);
    pwm_set_clkdiv(slice, p.div);
    pwm_set_wrap(slice, p.dc);
    pwm_set_enabled(slice, false);
    return slice;
}

void pwm_on(void){
    gpio_set_function(buzzer_a, GPIO_FUNC_PWM);
    pwm_set_enabled(slice, true);
    pwm_set_gpio_level(slice, p.dc/2);
}

void pwm_off(void){
    pwm_set_enabled(slice, false);
    gpio_set_function(buzzer_a, GPIO_FUNC_SIO);
    gpio_set_dir(buzzer_a, GPIO_OUT);
    gpio_put(buzzer_a, 0);
}

int64_t controle_pwm_crescendo(alarm_id_t id, void *user_data){
    p.vol_up_down = true;
    if(p.dc >= 3906.5 && p.alarm_state == false){
        p.dc -= 39.0;
        pwm_set_wrap(slice, p.dc);
        pwm_set_gpio_level(slice, p.dc/2);
        add_alarm_in_ms(10, controle_pwm_crescendo, NULL, false);
    } else if(p.dc <= 3906.5 && p.alarm_state == false) {
        p.dc = 3900;
        pwm_set_wrap(slice, p.dc);
        pwm_set_gpio_level(slice, p.dc/2); 
        p.alarm_state = true;
        add_alarm_in_ms (4000, controle_pwm_decrescendo, NULL, false);

    }
    return 0;
}

int64_t controle_pwm_decrescendo(alarm_id_t id, void *user_data){
    if(p.dc <= 3906.5 && p.alarm_state == true){
        p.dc += 39.0;
        pwm_set_wrap(slice, p.dc);
        pwm_set_gpio_level(slice, p.dc/2); 
        add_alarm_in_ms(10, controle_pwm_decrescendo, NULL, false);
    } else if(p.dc >= 39062.5 && p.alarm_state == true){
        p.dc = 39062.5;
        pwm_set_wrap(slice, p.dc);
        pwm_set_gpio_level(slice, p.dc/2);
        p.alarm_state = false;
        p.vol_up_down = false;
        pwm_off();
    }
    return 0;
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err){
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

void user_request(char **request){
    static bool led_pin_e = false;
    if (strstr(*request, "GET /luz_1") != NULL) gpio_put(blue_led, (!gpio_get(blue_led)));
    else if (strstr(*request, "GET /luz_2") != NULL) gpio_put(green_led, (!gpio_get(green_led)));
    else if (strstr(*request, "GET /luz_3") != NULL) gpio_put(red_led, (!gpio_get(red_led)));
    else if (strstr(*request, "GET /luz_e") != NULL) {
        led_pin_e = !led_pin_e;
        cyw43_arch_gpio_put(LED_PIN, led_pin_e);
    } else if (strstr(*request, "GET /sirene") != NULL) {
        if(p.vol_up_down == false){
            pwm_on();
            add_alarm_in_ms (10, controle_pwm_crescendo, NULL, false);
        }
    }

}

float temp_read(void){
    adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;
    return temperature;
}

static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
    if (!p){
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);
    user_request(&request);

    float temperature = temp_read();
    char html[1024];

    snprintf(html, sizeof(html),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n"
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<meta charset=\"UTF-8\">"
             "<title> Iluminação Inteligente (Com sirene) </title>\n"
             "<style>\n"
             "body { background-color:rgb(1, 35, 97); font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
             "h1 { font-size: 64px; margin-bottom: 30px; }\n"
             "button { background-color: LightGray; font-size: 36px; margin: 10px; padding: 20px 40px; border-radius: 10px; }\n"
             ".temperature { font-size: 48px; margin-top: 30px; color: #333; }\n"
             "</style>\n"
             "</head>\n"
             "<body>\n"
             "<h1>Iluminação: Lâmpadas </h1>\n"
             "<h2>Sirene especial com ativação remota </h2>\n"
             "<form action=\"./luz_1\"><button>Lâmpada 1</button></form>\n"
             "<form action=\"./luz_2\"><button>Lâmpada 2</button></form>\n"
             "<form action=\"./luz_3\"><button>Lâmpada 3</button></form>\n"
             "<form action=\"./luz_e\"><button>Lâmpada E</button></form>\n"
             "<form action=\"./sirene\"><button>Ativar Sirene</button></form>\n"
             "<p class=\"temperature\">Temperatura Interna: %.2f &deg;C</p>\n"
             "</body>\n"
             "</html>\n",
             temperature);

    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    free(request);
    pbuf_free(p);

    return ERR_OK;
}