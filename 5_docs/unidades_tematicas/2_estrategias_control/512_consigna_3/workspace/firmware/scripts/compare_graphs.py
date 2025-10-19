import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from scipy.fft import fft, fftfreq, rfft, rfftfreq

def load_csv(filename):
    """Carga un archivo CSV y devuelve timestamps y valores."""
    df = pd.read_csv(filename)
    timestamps = pd.to_datetime(df['Timestamp'])  # Convierte a datetime
    values = df['Value'].astype(float)           # Asegura que sean floats
    return timestamps, values

def plot_time_domain(t1, v1, t2, v2, label1, label2):
    """Grafica ambas señales en el dominio del tiempo."""
    plt.figure(figsize=(12, 5))
    plt.plot(t1, v1, 'b-', label=label1, alpha=0.7)
    plt.plot(t2, v2, 'r-', label=label2, alpha=0.7)
    plt.xlabel('Tiempo')
    plt.ylabel('Valor')
    plt.title('Comparación en el Dominio del Tiempo')
    plt.legend()
    plt.grid(True)
    plt.xticks(rotation=45)
    plt.tight_layout()

def plot_frequency_domain(v1, v2, label1, label2, sample_rate):
    """Calcula y grafica la FFT de ambas señales."""
    n = len(v1)
    # freq = fftfreq(n, d=1/sample_rate)[:n//2]  # Frecuencias positivas
    freq = rfftfreq(n, d=1/sample_rate)  # Frecuencias positivas para rfft

    fft1 = rfft(v1)  # FFT de la señal 1
    fft2 = rfft(v2)  # FFT de la señal 2
    # fft1 = np.abs(fft(v1)[:n//2])               # Magnitud FFT (señal 1)
    # fft2 = np.abs(fft(v2)[:n//2])               # Magnitud FFT (señal 2)

    plt.figure(figsize=(12, 5))
    plt.plot(freq, fft1, 'b-', label=f'FFT {label1}', alpha=0.7)
    plt.plot(freq, fft2, 'r-', label=f'FFT {label2}', alpha=0.7)
    plt.xlabel('Frecuencia (Hz)')
    plt.ylabel('Magnitud')
    plt.title('Comparación en el Dominio de la Frecuencia (FFT)')
    plt.legend()
    plt.grid(True)
    plt.tight_layout()

def main():
    # Configuración (cambia los nombres de archivo)
    file1 = 'led_raw_samples.csv'  # Primer archivo CSV
    file2 = 'led_samples_filter.csv'  # Segundo archivo CSV
    sample_rate = 1000          # Tasa de muestreo en Hz (ajusta según tus datos)

    # Cargar datos
    t1, v1 = load_csv(file1)
    t2, v2 = load_csv(file2)

    # Graficar dominio del tiempo
    plot_time_domain(t1, v1, t2, v2, file1, file2)

    # Graficar FFT (asume misma tasa de muestreo para ambos archivos)
    # plot_frequency_domain(v1, v2, file1, file2, sample_rate)

    plt.show()

if __name__ == "__main__":
    main()