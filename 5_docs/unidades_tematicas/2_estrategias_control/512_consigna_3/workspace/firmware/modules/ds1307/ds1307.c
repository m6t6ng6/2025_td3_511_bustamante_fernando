#include "ds1307.h"

// Direccion del DS1307
static uint8_t ds1307_addr;
// Bus de I2C
static i2c_inst_t *ds1307_i2c;

// Convierte BCD a binario
static uint8_t bcd_to_bin(uint8_t val) {
    return (val & 0x0F) + ((val >> 4) * 10);
}

static inline void ds1307_write(uint8_t reg, uint8_t *src, uint8_t len) {
    // Array completo para enviar
    uint8_t buff[20] = {0};
    // Registro a escribir
    buff[0] = reg;
    // Copio los otros
    for(uint8_t i = 0; i < len; i++) { buff[i + 1] = src[i]; }
    // Inicio la comunicacion
    i2c_write_blocking(ds1307_i2c, ds1307_addr, buff, len + 1, false);
}

static inline void ds1307_read(uint8_t reg, uint8_t *dst, uint8_t len) {
    // Inicio la comunicacion
    i2c_write_blocking(ds1307_i2c, ds1307_addr, &reg, 1, true);
    // Leo los bytes
    i2c_read_blocking(ds1307_i2c, ds1307_addr, dst, len, false);
}

void ds1307_init(ds1307_t *ds1307) {
    // Guardo bus de I2C y direccion
    ds1307_i2c = ds1307->i2c;
    ds1307_addr = ds1307->addr;
}

bool get_time_from_rtc(rtc_time_t *time) {
    uint8_t buffer[7];
    uint8_t start_reg = 0x00;

    if (i2c_write_blocking(i2c0, DS1307_ADDRESS, &start_reg, 1, true) != 1) {
        return false;
    }

    if (i2c_read_blocking(i2c0, DS1307_ADDRESS, buffer, 7, false) != 7) {
        return false;
    }

    time->seconds = bcd_to_bin(buffer[0] & 0x7F);
    time->minutes = bcd_to_bin(buffer[1]);
    time->hours   = bcd_to_bin(buffer[2] & 0x3F);  // Asume formato 24h
    time->day     = bcd_to_bin(buffer[3]);
    time->date    = bcd_to_bin(buffer[4]);
    time->month   = bcd_to_bin(buffer[5]);
    time->year    = bcd_to_bin(buffer[6]);  // Año estimado (no exacto)

    return true;
}

void ds1307_get_time(ds1307_t *ds1307) {
    get_time_from_rtc(&(ds1307->time));
}

static uint8_t bin_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

bool set_time_to_rtc(const rtc_time_t *time) {
    uint8_t buffer[8];

    buffer[0] = 0x00; // Dirección inicial de escritura en el RTC
    buffer[1] = bin_to_bcd(time->seconds);
    buffer[2] = bin_to_bcd(time->minutes);
    buffer[3] = bin_to_bcd(time->hours);        // Modo 24h
    buffer[4] = bin_to_bcd(time->day);          // Día de la semana (1=Dom, 7=Sáb)
    buffer[5] = bin_to_bcd(time->date);         // Día del mes
    buffer[6] = bin_to_bcd(time->month);
    buffer[7] = bin_to_bcd(time->year % 100);   // Solo los últimos 2 dígitos del año

    int written = i2c_write_blocking(i2c0, DS1307_ADDRESS, buffer, 8, false);
    return written == 8;
}

void ds1307_set_time(ds1307_t *ds1307){
    set_time_to_rtc(&(ds1307->time));
    // ds1307_write(0x00, buffer, 7);
}

// int main() {
//     stdio_init_all();

//     // Inicializa I2C0 en GPIO 16 (SDA), GPIO 17 (SCL)
//     i2c_init(I2C_PORT, 400 * 1000);
//     gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
//     gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
//     gpio_pull_up(I2C_SDA);
//     gpio_pull_up(I2C_SCL);

   
//     q_to_storage = xQueueCreate(BUFF_STORAGE, sizeof(int));

//     xTaskCreate(storage_task, "storage_task", configMINIMAL_STACK_SIZE*2, NULL, 2, NULL);
//     xTaskCreate(generador, "generador", configMINIMAL_STACK_SIZE*2, NULL, 2, NULL);

    

//     set_time_to_rtc(&t);
    

//    vTaskStartScheduler();
//    while(1);
// }



// int main() {
//     stdio_init_all();

//     // Inicializa I2C0 en GPIO 16 (SDA), GPIO 17 (SCL)
//     i2c_init(I2C_PORT, 400 * 1000);
//     gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
//     gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
//     gpio_pull_up(I2C_SDA);
//     gpio_pull_up(I2C_SCL);

//     rtc_time_t now;

//     int data_value = 12345;  // Ejemplo: lo que quieras registrar
//     get_time_from_rtc(&now);
//     char log_entry[ENTRY_SIZE];
//     snprintf(log_entry, sizeof(log_entry), "%02d:%02d-%02d/%02d: %d",
//           now.hours, now.minutes, now.date, now.month, data_value);

//     //snprintf(log_entry, sizeof(log_entry), "12:00-25/12: %d",data_value);
//     save_log(log_entry);
//     save_log(log_entry);

//     read_all_logs();

//     while (true) {
//         if (get_time_from_rtc(&now)) {
//             printf("Hora: %02d:%02d:%02d - %02d/%02d/%04d\n",
//                    now.hours, now.minutes, now.seconds,
//                    now.date, now.month, now.year);
//         } else {
//             printf("Error al leer desde el RTC\n");
//         }
//         sleep_ms(1000);
//         read_all_logs();
//     }
// }