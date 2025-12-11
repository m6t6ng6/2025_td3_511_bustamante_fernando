#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/serdev.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#define DRIVER_NAME  "egb_uart_chrdev"
#define DEVICE_NAME  "egb"
#define CLASS_NAME   "egb_class"
#define AUTHOR       "Facu"

/* --- SERDEV y Buffer de Recepción (RX) --- */
#define RX_LINE_BUF_SIZE 128
static char  rx_line_buf[RX_LINE_BUF_SIZE];
static size_t rx_line_len = 0;
static struct serdev_device *egb_serdev; // Referencia global al dispositivo serdev

/* Sincronización para la recepción (wait queue y spinlock) */
static wait_queue_head_t rx_wait;
static spinlock_t rx_lock;

/* --- CHARDEV (Dispositivo de Caracteres) --- */
static dev_t dev_number;
static struct class *egb_class;
static struct cdev egb_cdev;

/*
 * Callback de recepción (SERDEV):
 * Acumula caracteres hasta encontrar '\n' o '\r' y notifica a los lectores.
 */
static size_t egb_uart_receive(struct serdev_device *serdev, const unsigned char *buf, size_t size)
{
    size_t i;
    unsigned long flags;
    bool line_received = false;
    // --- AGREGAR ESTA LÍNEA DE DEBUGGING ---
    dev_info(&serdev->dev, "egb_uart_receive: llamado, recibidos %zu bytes.\n", size);
    // ----------------------------------------
    spin_lock_irqsave(&rx_lock, flags);

    for (i = 0; i < size; i++) {
        unsigned char ch = buf[i];

        if (ch == '\n' || ch == '\r') {
            /* Cerrar string si hay espacio */
            if (rx_line_len < RX_LINE_BUF_SIZE)
                rx_line_buf[rx_line_len] = '\0';
            else
                rx_line_buf[RX_LINE_BUF_SIZE - 1] = '\0';

            /* La línea está completa y lista para ser leída por el usuario */
            dev_info(&serdev->dev,
                     "Linea completa recibida por UART: '%s'\n",
                     rx_line_buf);
            
            line_received = true;
            break; // Solo procesamos la primera línea completa por llamada
        } else {
            /* Acumular mientras haya espacio */
            if (rx_line_len < RX_LINE_BUF_SIZE - 1) {
                rx_line_buf[rx_line_len++] = ch;
            } else {
                /* Si se llena, se descarta el resto de la línea */
            }
        }
    }

    if (line_received) {
        // Despertar a cualquier proceso de usuario esperando en read()
        wake_up_interruptible(&rx_wait);
    }
    
    spin_unlock_irqrestore(&rx_lock, flags);

    /* Indicamos que consumimos todos los bytes (aunque solo procesemos la primera línea) */
    return size;
}

static const struct serdev_device_ops egb_uart_ops = {
    .receive_buf = egb_uart_receive,
};

/* --- OPERACIONES CHARDEV (file_operations) --- */

/* WRITE: Envía datos del espacio de usuario al UART (serdev) */
static ssize_t egb_write(struct file *filep, const char __user *buf, size_t len, loff_t *offset)
{
    char tmp_buf[RX_LINE_BUF_SIZE+2];
    int ret;
    size_t copy_len = min(len, (size_t)RX_LINE_BUF_SIZE);

    if (egb_serdev == NULL)
        return -EIO;

    if (copy_from_user(tmp_buf, buf, copy_len))
        return -EFAULT;
    // --- CORRECCIÓN: LIMPIAR BUFFER ANTES DE ENVIAR ---
    unsigned long flags;
    spin_lock_irqsave(&rx_lock, flags);
    rx_line_len = 0; // Borrar cualquier cosa que haya quedado, como el HELLO
    spin_unlock_irqrestore(&rx_lock, flags);
    //---LOGICA DE CORRECION---
    //1. Asegurar que haya espacio para \n y \0
    if (copy_len < RX_LINE_BUF_SIZE + 1) {
        // 2. Añadir el caracter de fin de línea que el driver espera
        tmp_buf[copy_len] = '\n'; 
        copy_len++; // Aumentar la longitud a enviar
    }
    // Asegurarse que el buffer del kernel esté terminado en null temporalmente para log
    tmp_buf[copy_len] = '\0';
    pr_info("egb_uart_chrdev: recibido de user y enviando por UART: '%.*s'\n",
            (int)copy_len, tmp_buf);

    // Enviar por UART usando serdev
    ret = serdev_device_write_buf(egb_serdev, tmp_buf, copy_len);
    if (ret < 0) {
        pr_err("Error al enviar datos por UART (write_buf): %d\n", ret);
        return ret;
    }

    // Devuelve la cantidad original que el usuario quiso escribir
    return len; 
}

/* READ: Lee la última línea recibida por UART (serdev) */
static ssize_t egb_read(struct file *filep, char __user *buf, size_t len, loff_t *offset)
{
    unsigned long flags;
    size_t data_len;
    int ret;

    /*/ Solo leeremos si estamos al inicio del archivo (offset 0)
    if (*offset > 0)
        return 0; // indicar EOF
    */
    // Esperar a que llegue una línea por UART
    ret = wait_event_interruptible(rx_wait, rx_line_len > 0);
    if (ret)
        return ret; // Retornar si fue interrumpido por señal

    spin_lock_irqsave(&rx_lock, flags);

    data_len = rx_line_len;
    
    // Limitar la copia a la longitud del buffer de usuario
    data_len = min(data_len, len);

    // Copiar los datos de la línea recibida al espacio de usuario
    if (copy_to_user(buf, rx_line_buf, data_len)) {
        spin_unlock_irqrestore(&rx_lock, flags);
        return -EFAULT;
    }

    // Reseteamos el buffer de RX para la próxima línea
    rx_line_len = 0;

    spin_unlock_irqrestore(&rx_lock, flags);
    
    // Actualizar offset e indicar bytes leídos
    //*offset = data_len;
    return data_len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = egb_write,
    .read  = egb_read,
};


/* --- SERDEV PROBE (Aquí se inicializa CHARDEV y SERDEV) --- */

static int egb_uart_probe(struct serdev_device *serdev)
{
    int ret;
    const char msg[] = "HELLO desde EGB kernel via serdev!\r\n";

    dev_info(&serdev->dev, "egb_uart_probe: dispositivo serdev encontrado\n");

    // Guardar referencia global al dispositivo serdev
    egb_serdev = serdev;

    /* 1. Inicializar CHARDEV (Dispositivo de Caracteres) */
    pr_info("egb_uart_chrdev: inicializando char device /dev/egb\n");
    
    // Reservar major/minor dinámicamente
    ret = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        dev_err(&serdev->dev, "No pudo reservar major/minor\n");
        goto err_out;
    }

    // Crear clase
    egb_class = class_create(CLASS_NAME);
    if (IS_ERR(egb_class)) {
        ret = PTR_ERR(egb_class);
        goto err_unregister;
    }

    // Crear /dev/egb
    if (IS_ERR(device_create(egb_class, &serdev->dev, dev_number, NULL, DEVICE_NAME))) {
        ret = -EINVAL;
        goto err_class_destroy;
    }

    // Inicializar y añadir cdev
    cdev_init(&egb_cdev, &fops);
    ret = cdev_add(&egb_cdev, dev_number, 1);
    if (ret < 0) {
        goto err_device_destroy;
    }
    
    pr_info("egb_uart_chrdev: /dev/egb creado, Major:%d Minor:%d\n", MAJOR(dev_number), MINOR(dev_number));

    /* 2. Inicializar SERDEV */
    serdev_device_set_client_ops(serdev, &egb_uart_ops);

    ret = serdev_device_open(serdev);
    if (ret) {
        dev_err(&serdev->dev, "No se pudo abrir el dispositivo serdev: %d\n", ret);
        goto err_cdev_del;
    }

    serdev_device_set_baudrate(serdev, 115200);
    serdev_device_set_flow_control(serdev, false);

    /* 3. Inicializar Sincronización */
    init_waitqueue_head(&rx_wait);
    spin_lock_init(&rx_lock);
    
    /* 4. Enviar mensaje de inicialización */
    ret = serdev_device_write_buf(serdev, msg, strlen(msg));
    if (ret < 0) {
        dev_err(&serdev->dev, "Error al enviar datos por UART: %d\n", ret);
        serdev_device_close(serdev);
        goto err_cdev_del;
    } else {
        dev_info(&serdev->dev, "Se escribieron %d bytes por UART (mensaje inicial)\n", ret);
    }

    dev_info(&serdev->dev, "egb_uart_chrdev: listo. La comunicación se realiza via /dev/egb.\n");

    return 0; // Éxito

err_cdev_del:
    cdev_del(&egb_cdev);
err_device_destroy:
    device_destroy(egb_class, dev_number);
err_class_destroy:
    class_destroy(egb_class);
err_unregister:
    unregister_chrdev_region(dev_number, 1);
err_out:
    egb_serdev = NULL;
    return ret;
}

static void egb_uart_remove(struct serdev_device *serdev)
{
    dev_info(&serdev->dev, "egb_uart_remove: cerrando dispositivo\n");
    
    // 1. Cerrar SERDEV
    serdev_device_close(serdev);

    // 2. Limpiar CHARDEV
    cdev_del(&egb_cdev);
    device_destroy(egb_class, dev_number);
    class_destroy(egb_class);
    unregister_chrdev_region(dev_number, 1);

    egb_serdev = NULL;
}

/* --- DRIVER DEFS --- */

static const struct of_device_id egb_uart_of_match[] = {
    { .compatible = "frankie,egb-uart" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, egb_uart_of_match);

static struct serdev_device_driver egb_uart_driver = {
    .probe  = egb_uart_probe,
    .remove = egb_uart_remove,
    .driver = {
        .name           = DRIVER_NAME,
        .of_match_table = of_match_ptr(egb_uart_of_match),
    },
};

module_serdev_device_driver(egb_uart_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION("EGB: driver serdev + char device unificado para UART");