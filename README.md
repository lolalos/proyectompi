# Segmentación de Imágenes en Paralelo con MPI y OpenCV

Esta documentación explica paso a paso la implementación de un programa de segmentación de imágenes que utiliza paralelismo mediante MPI (Message Passing Interface) y OpenCV.

## Estructura General del Código

El programa implementa un sistema para segmentar imágenes distribuyendo el trabajo entre múltiples procesos, mejorando el rendimiento en imágenes grandes.

## Librerías y Dependencias

- **MPI**: Para comunicación entre procesos
- **OpenCV**: Para procesamiento de imágenes
- **Librerías estándar de C++**: Para funcionalidades de propósito general
- **Librerías específicas de plataforma**: Para medición de recursos del sistema

## Funciones Auxiliares

### 1. `segment_image`
- Aplica el algoritmo Mean Shift Filtering de OpenCV para segmentar la imagen
- Parámetros: imagen de entrada, imagen de salida, sigma (suavizado), k (umbral) y tamaño mínimo de componente

### 2. `get_memory_usage`
- Mide el uso de memoria del proceso actual
- Implementación específica para Windows y Linux

### 3. `get_cpu_cores`
- Detecta el número de núcleos de CPU disponibles en el sistema
- Implementación multiplataforma

### 4. `opencv_has_cuda`
- Verifica si OpenCV fue compilado con soporte para CUDA (aceleración GPU)

## Flujo de Ejecución Paso a Paso

### 1. Inicialización y Configuración
- Inicializa el entorno MPI
- Obtiene el rango (ID) y tamaño (número total de procesos)
- Procesa los argumentos de la línea de comandos:
    - sigma: nivel de suavizado
    - k: umbral de segmentación
    - min_size: tamaño mínimo de componente
    - rutas de imágenes de entrada y salida

### 2. Distribución de la Imagen
- **Proceso 0 (maestro)**:
    - Carga la imagen desde el archivo
    - Determina dimensiones y tipo de imagen
    - Transmite estos metadatos a todos los procesos
- **Todos los procesos**:
    - Calculan cuántas filas les corresponden procesar
    - Reservan memoria para sus porciones locales de imagen

### 3. División de Trabajo
- **Proceso 0**:
    - Divide la imagen en bloques de filas
    - Conserva su propio bloque
    - Envía los bloques restantes a los otros procesos
- **Procesos 1-N**:
    - Reciben sus bloques de imagen asignados

### 4. Procesamiento en Paralelo
- Establecimiento de barrera para sincronización
- Cada proceso mide:
    - Tiempo inicial y uso de memoria
    - Aplica el algoritmo de segmentación a su bloque
    - Tiempo final y memoria consumida
- Cada proceso calcula:
    - Tiempo de ejecución local
    - Memoria utilizada
    - Número de píxeles procesados

### 5. Recolección de Resultados
- **Procesos 1-N**:
    - Envían sus bloques procesados al proceso 0
- **Proceso 0**:
    - Recibe los bloques de todos los procesos
    - Reconstruye la imagen completa segmentada
    - Guarda la imagen resultante en el archivo de salida

### 6. Recopilación de Métricas
- Todos los procesos reportan sus métricas:
    - Tiempo de procesamiento
    - Memoria utilizada
    - Cantidad de píxeles procesados
- Proceso 0 recopila todas las métricas mediante MPI_Gather

### 7. Presentación de Resultados
- **Proceso 0**:
    - Calcula estadísticas agregadas (máximo, mínimo, promedio)
    - Muestra información completa del sistema y rendimiento
    - Visualiza las imágenes original y segmentada
    - Espera interacción del usuario

### 8. Finalización
- Libera recursos y finaliza MPI

## Aspectos Destacados de la Implementación

1. **Balanceo de carga**: Distribución equitativa de filas entre procesos
2. **Comunicación eficiente**: Uso de operaciones colectivas MPI para metadatos
3. **Medición precisa**: Captura de tiempos y uso de memoria para análisis de rendimiento
4. **Adaptabilidad**: Detección automática de recursos del sistema (CPU, GPU)
5. **Multiplataforma**: Compatibilidad con Windows y Linux

## Consideraciones de Rendimiento

- El programa aprovecha múltiples núcleos/procesadores para acelerar la segmentación
- La distribución por filas permite un buen balanceo de carga
- Se reportan métricas detalladas para evaluar el rendimiento
