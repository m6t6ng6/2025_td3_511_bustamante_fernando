/*En esta version se pedira algo lo mismo que en el  version 1 pero ahora
 se debera prender y apagar un led. Para  ello sera nesesario utilizar los
 pines GPIO del  RASPBERRY PI. Ademas el pin a utilizar se debera pasa como parametro, es decir se indicara de forma externa que poin usar.*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include "gpio_driver.h"

// Parámetro del módulo
static unsigned int gpio_led = 17; // Valor por defecto GPIO 17
module_param(gpio_led, uint, 0644);
MODULE_PARM_DESC(gpio_led, "Número de GPIO para el LED (por defecto: 17)");

// Variables para las tareas
static struct task_struct *task_on;
static struct task_struct *task_off;
static int stop_tasks = 0;

/**
 * @brief Tarea que enciende el LED
 */
static int led_on_task(void *data) {
    while (!kthread_should_stop() && !stop_tasks) {
        gpio_set(gpio_led);
        printk(KERN_INFO "LED ON - GPIO %d\n", gpio_led);
        msleep(1000); // Espera 1 segundo
        
        // Pequeña pausa entre ON y OFF
        if (kthread_should_stop() || stop_tasks)
            break;
        msleep(100);
    }
    return 0;
}

/**
 * @brief Tarea que apaga el LED
 */
static int led_off_task(void *data) {
    while (!kthread_should_stop() && !stop_tasks) {
        // Pequeña pausa antes de apagar
        msleep(500);
        
        if (kthread_should_stop() || stop_tasks)
            break;
            
        gpio_clr(gpio_led);
        printk(KERN_INFO "LED OFF - GPIO %d\n", gpio_led);
        msleep(1000); // Espera 1 segundo
    }
    return 0;
}

/**
 * @brief Función de inicialización del módulo
 */
static int __init kernel_module_init(void) {
    int ret = 0;
    
    printk(KERN_INFO "UTN-FRA-TD3-TP5: Inicializando módulo LED para GPIO %d\n", gpio_led);
    
    // Validar que el GPIO esté en rango válido para Raspberry Pi
    if (gpio_led > 53) {
        printk(KERN_ERR "UTN-FRA-TD3-TP5: Error: GPIO %d fuera de rango (0-53)\n", gpio_led);
        return -EINVAL;
    }
    
    // Mapear memoria GPIO
    if (!gpio_map()) {
        printk(KERN_ERR "UTN-FRA-TD3-TP5: Error: No se pudo mapear la memoria GPIO\n");
        return -ENOMEM;
    }
    
    // Configurar GPIO como salida
    gpio_set_dir_output(gpio_led);
    printk(KERN_INFO "UTN-FRA-TD3-TP5: GPIO %d configurado como salida\n", gpio_led);
    
    // Inicialmente apagar el LED
    gpio_clr(gpio_led);
    
    // Crear tareas (kthreads)
    task_on = kthread_run(led_on_task, NULL, "utn_led_on");
    if (IS_ERR(task_on)) {
        printk(KERN_ERR "UTN-FRA-TD3-TP5: Error creando tarea LED ON\n");
        ret = PTR_ERR(task_on);
        goto error_cleanup;
    }
    
    task_off = kthread_run(led_off_task, NULL, "utn_led_off");
    if (IS_ERR(task_off)) {
        printk(KERN_ERR "UTN-FRA-TD3-TP5: Error creando tarea LED OFF\n");
        ret = PTR_ERR(task_off);
        goto error_cleanup_tasks;
    }
    
    printk(KERN_INFO "UTN-FRA-TD3-TP5: Módulo inicializado correctamente. GPIO: %d\n", gpio_led);
    return 0;

error_cleanup_tasks:
    if (!IS_ERR(task_on)) {
        stop_tasks = 1;
        kthread_stop(task_on);
    }
error_cleanup:
    gpio_unmap();
    return ret;
}

/**
 * @brief Función de limpieza del módulo
 */
static void __exit kernel_module_exit(void) {
    printk(KERN_INFO "UTN-FRA-TD3-TP5: Finalizando módulo LED\n");
    
    // Detener tareas
    stop_tasks = 1;
    
    if (task_on && !IS_ERR(task_on)) {
        kthread_stop(task_on);
        printk(KERN_INFO "UTN-FRA-TD3-TP5: Tarea ON detenida\n");
    }
    
    if (task_off && !IS_ERR(task_off)) {
        kthread_stop(task_off);
        printk(KERN_INFO "UTN-FRA-TD3-TP5: Tarea OFF detenida\n");
    }
    
    // Apagar LED antes de salir
    gpio_clr(gpio_led);
    
    // Liberar memoria GPIO
    gpio_unmap();
    
    printk(KERN_INFO "UTN-FRA-TD3-TP5: Módulo finalizado\n");
}

// Registro de funciones
module_init(kernel_module_init);
module_exit(kernel_module_exit);

// Información del módulo
MODULE_LICENSE("GPL");
MODULE_AUTHOR("UTN-FRA");
MODULE_DESCRIPTION("TD3-TP5: Controlador de LED con tareas kernel");
MODULE_VERSION("1.0");
