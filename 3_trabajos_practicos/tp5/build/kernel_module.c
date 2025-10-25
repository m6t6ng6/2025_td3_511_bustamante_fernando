#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#define AUTHOR "utn-fra-td3"

// Estructuras para los hilos
static struct task_struct *hilo_hola;
static struct task_struct *hilo_chau;
static bool ejecutando = true;

/**
 * @brief Hilo que saluda periódicamente cada 500ms
 */
static int hilo_funcion_hola(void *data) {
    int contador = 0;
    
    while (ejecutando) {
        contador++;
        printk(KERN_INFO "%s: Hola desde el kernel! (vez #%d)\n", AUTHOR, contador);
        msleep(500); // Esperar 500 ms = 0.5 segundos
    }
    
    printk(KERN_INFO "%s: Hilo 'Hola' finalizado después de %d saludos\n", 
           AUTHOR, contador);
    return 0;
}

/**
 * @brief Hilo que se despide periódicamente cada 500ms
 */
static int hilo_funcion_chau(void *data) {
    int contador = 0;
    
    while (ejecutando) {
        contador++;
        printk(KERN_INFO "%s: Chau desde el kernel! (vez #%d)\n", AUTHOR, contador);
        msleep(500); // Esperar 500 ms = 0.5 segundos
    }
    
    printk(KERN_INFO "%s: Hilo 'Chau' finalizado después de %d despedidas\n", 
           AUTHOR, contador);
    return 0;
}

/**
 * @brief Función de inicialización del módulo
 */
static int __init kernel_module_init(void) {
    printk(KERN_INFO "%s: === INICIANDO MÓDULO CON HILOS PERIÓDICOS ===\n", AUTHOR);
    printk(KERN_INFO "%s: Creando hilos que ejecutarán cada 500ms...\n", AUTHOR);
    
    // Crear hilo "Hola"
    hilo_hola = kthread_run(hilo_funcion_hola, NULL, "hilo_hola");
    if (IS_ERR(hilo_hola)) {
        printk(KERN_ERR "%s: ERROR creando hilo 'Hola'\n", AUTHOR);
        return PTR_ERR(hilo_hola);
    }
    
    // Crear hilo "Chau"
    hilo_chau = kthread_run(hilo_funcion_chau, NULL, "hilo_chau");
    if (IS_ERR(hilo_chau)) {
        printk(KERN_ERR "%s: ERROR creando hilo 'Chau'\n", AUTHOR);
        kthread_stop(hilo_hola);
        return PTR_ERR(hilo_chau);
    }
    
    printk(KERN_INFO "%s: ✓ Hilos creados exitosamente\n", AUTHOR);
    printk(KERN_INFO "%s: Los hilos ejecutarán concurrentemente cada 500ms...\n", AUTHOR);
    
    return 0;
}

/**
 * @brief Función de salida del módulo
 */
static void __exit kernel_module_exit(void) {
    printk(KERN_INFO "%s: === FINALIZANDO MÓDULO ===\n", AUTHOR);
    
    // Señalizar a los hilos que deben terminar
    ejecutando = false;
    
    // Detener y esperar a que los hilos terminen
    if (hilo_hola && !IS_ERR(hilo_hola)) {
        kthread_stop(hilo_hola);
        printk(KERN_INFO "%s: ✓ Hilo 'Hola' detenido\n", AUTHOR);
    }
    
    if (hilo_chau && !IS_ERR(hilo_chau)) {
        kthread_stop(hilo_chau);
        printk(KERN_INFO "%s: ✓ Hilo 'Chau' detenido\n", AUTHOR);
    }
    
    printk(KERN_INFO "%s: Módulo descargado correctamente\n", AUTHOR);
}

// Registrar funciones de inicialización y salida
module_init(kernel_module_init);
module_exit(kernel_module_exit);

// Metadatos del módulo
MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION("Módulo con dos hilos periódicos que saludan cada 500ms");
MODULE_VERSION("1.0");
