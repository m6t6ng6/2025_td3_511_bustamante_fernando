import serial
import csv
import matplotlib.pyplot as plt
import signal
import sys

# Configuración
PORT = 'COM8'       # Cambiar a tu puerto (ej: '/dev/ttyUSB0' en Linux)
BAUDRATE = 115200   # Ajustar según el dispositivo
CSV_FILE = 'led_no_kalman_samples.csv'
TIMEOUT = 1         # Tiempo de espera para lectura (segundos)

# Variables globales
running = True
timestamps = []
values = []

def signal_handler(sig, frame):
    """Maneja la señal de interrupción (Ctrl+C) para terminar el programa."""
    global running
    print("\nDeteniendo la adquisición...")
    running = False

def save_to_csv(filename, timestamps, values):
    """Guarda los datos en un archivo CSV."""
    with open(filename, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Timestamp', 'Value'])  # Encabezado
        for t, v in zip(timestamps, values):
            writer.writerow([t, v])
    print(f"Datos guardados en {filename}")

def plot_data(timestamps, values):
    """Genera un gráfico con los datos almacenados."""
    plt.figure(figsize=(10, 5))
    plt.plot(timestamps, values, 'b-', label='Valor del Sensor')
    plt.xlabel('Tiempo')
    plt.ylabel('Valor')
    plt.title('Datos del Puerto Serial')
    plt.legend()
    plt.grid(True)
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.show()

def main():
    global running, timestamps, values

    # Configurar el manejador de señales (Ctrl+C)
    signal.signal(signal.SIGINT, signal_handler)

    # Inicializar conexión serial
    try:
        ser = serial.Serial(PORT, BAUDRATE, timeout=TIMEOUT)
        print(f"Conectado a {ser.name}. Presiona Ctrl+C para detener y graficar.")
    except serial.SerialException as e:
        print(f"Error al abrir el puerto serial: {e}")
        sys.exit(1)

    # Leer datos hasta que se presione Ctrl+C
    try:
        time = 0
        while running:

            line = ser.readline().decode('utf-8').strip()
            if line:  # Ignorar líneas vacías
                try:
                    value = float(line)  # Convertir a float
                    timestamps.append(time)  # Registrar timestamp
                    values.append(value)
                    print(f"Valor leído: {value}")  # Opcional: mostrar progreso
                    time += 1
                    if time == 2000: 
                        break
                except ValueError:
                    print(f"Dato no válido: {line}")
    finally:
        ser.close()
        if timestamps:  # Solo graficar si hay datos
            save_to_csv(CSV_FILE, timestamps, values)
            plot_data(timestamps, values)
        else:
            print("No se recibieron datos.")

if __name__ == "__main__":
    main()