#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/dma.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define ADC_MAX_VALUE ((1 << 12) - 1)  // 4095
#define NUM_SAMPLES 16

uint16_t adc_buffer[NUM_SAMPLES];
int dma_chan;

typedef struct {
    uint16_t raw;
    float tempC;
} tempData_t;

// Cola para pasar los datos desde la ISR
QueueHandle_t tempQueue;

// Prototipos
void vTaskDisparaADC(void *params);
void vTaskMuestraTemp(void *params);
void dma_handler();

void dma_handler() {
    dma_hw->ints0 = 1u << dma_chan;  // Limpia bandera de interrupción DMA
    adc_run(false);  // Detiene el ADC

    for (int i = 0; i < NUM_SAMPLES; i++) {
        float voltage = adc_buffer[i] * 3.3f / ADC_MAX_VALUE;
        float tempC = 27.0f - (voltage - 0.706f) / 0.001721f;
        tempData_t data = {.raw = adc_buffer[i], .tempC = tempC};
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(tempQueue, &data, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

int main() {
    stdio_init_all();
    sleep_ms(3000);  // Da tiempo a que se conecte la consola USB

    // Inicializa ADC y su canal
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_fifo_setup(true, true, 1, false, false);
    adc_run(false);  // Lo iniciaremos manualmente

    // Configura DMA
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    dma_channel_configure(
        dma_chan, &cfg,
        adc_buffer, &adc_hw->fifo,
        NUM_SAMPLES, false
    );

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Crea cola
    tempQueue = xQueueCreate(64, sizeof(tempData_t));
    if (tempQueue == NULL) {
        printf("Error creando la cola\n");
        while (1);
    }

    // Crea tareas
    xTaskCreate(vTaskDisparaADC, "DisparaADC", 2 * configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vTaskMuestraTemp, "MuestraTemp", 2 * configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    // Inicia scheduler
    vTaskStartScheduler();

    while (1);
    return 0;
}

// Tarea que dispara la conversión ADC periódicamente con DMA
void vTaskDisparaADC(void *params) {
    while (1) {
        adc_select_input(4);       // Canal del sensor de temperatura interno
        adc_fifo_drain();          // Limpia FIFO antes de nueva medición
        dma_channel_start(dma_chan);  // Inicia transferencia DMA
        adc_run(true);             // Inicia ADC
        vTaskDelay(pdMS_TO_TICKS(5000));  // Espera 5 segundos
    }
}

// Tarea que recibe valores desde la cola y muestra por consola
void vTaskMuestraTemp(void *params) {
    tempData_t data;
    while (1) {
        if (xQueueReceive(tempQueue, &data, portMAX_DELAY) == pdTRUE) {
            printf("Temperatura: %.2f °C (raw: %u)\n", data.tempC, data.raw);
        }
    }
}