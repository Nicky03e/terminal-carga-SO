# terminal-carga-SO
Proyecto 1 - Sistemas Operativos EIF212

```
# 🚛 Terminal de Carga Automatizada
### EIF 212 — Sistemas Operativos | I Ciclo 2026

Simulador de una terminal logística implementado en **C con hilos POSIX**, semáforos y mutex. Modela el acceso concurrente de camiones a muelles de carga con dos algoritmos de planificación.

---

## 👥 Integrantes
Nicole Masis Brenes
| Integrante 2 | 
| Integrante 3 |

---

## 📋 Descripción General

El sistema simula una terminal donde múltiples **Camiones (hilos)** compiten por acceder a **Muelles de Carga (Sección Crítica)**. El programa implementa:

- **Hilos POSIX** (`pthreads`) con ciclo de vida completo de 5 estados
- **Semáforos** para controlar el acceso a los 3 muelles simultáneos
- **Mutex** para proteger el log de operaciones compartido
- **Dos algoritmos de planificación:** FIFO y Round Robin con prioridad

---

## ⚙️ Requisitos

- Linux o WSL (Ubuntu)
- GCC con soporte para pthreads

```bash
gcc --version
```

---

## 🔧 Compilación y Ejecución

```bash
gcc -o terminal terminal.c -lpthread
./terminal
```

---

## 🏗️ Arquitectura del Sistema

### Componentes principales

| Componente | Descripción |
|------------|-------------|
| `main()` | Proceso principal. Inicializa recursos, lanza hilos y espera con `pthread_join` |
| Hilos (Camiones) | Creados con `pthread_create`. Cada uno tiene burst, prioridad y ciclo de vida |
| `sem_t muelles` | Semáforo inicializado en 3. Controla acceso simultáneo a los muelles |
| `pthread_mutex_t log_mutex` | Mutex que protege el log de operaciones compartido |
| Cola con prioridad | Ordena camiones: prioridad 1 (perecederos) antes que prioridad 2 (normal) |

### Ciclo de vida de cada Camión (hilo)

```
pthread_create()
      │
      ▼
   NUEVO ──► LISTO ──► BLOQUEADO ──► EJECUCIÓN ──► TERMINADO
                          │               │
                     sem_wait()      sem_post()
                     (espera muelle) (libera muelle)
```

| Estado | Descripción |
|--------|-------------|
| `NUEVO` | Hilo instanciado con `pthread_create` |
| `LISTO` | En cola de planificación esperando turno |
| `BLOQUEADO` | Esperando `sem_wait()` porque los 3 muelles están ocupados |
| `EJECUCIÓN` | Dentro de la Sección Crítica usando el muelle |
| `TERMINADO` | Finalizó, recolectado por el padre con `pthread_join` |

---

## 🔒 Sincronización

### Semáforos — Control de Muelles
```c
sem_init(&muelles, 0, 3);   // Máximo 3 camiones simultáneos
sem_wait(&muelles);          // Entra al muelle (bloquea si están llenos)
// --- Sección Crítica ---
sem_post(&muelles);          // Sale del muelle (libera espacio)
```

### Mutex — Log de Operaciones
```c
pthread_mutex_lock(&log_mutex);    // Entra a sección crítica del log
printf("...");                      // Escribe sin interferencia
pthread_mutex_unlock(&log_mutex);  // Libera el log
```

---

## 📅 Algoritmos de Planificación

### FIFO con Prioridad
- Los camiones se atienden **en orden de llegada**
- Los de **prioridad 1 (perecederos)** se insertan al frente de la cola
- Un camión ocupa el muelle hasta **completar toda su carga** (sin desalojo)
- Puede generar **Efecto Convoy** si un camión lento bloquea a los demás

### Round Robin con Prioridad
- Cada camión ocupa el muelle máximo **QUANTUM = 2 segundos**
- Si no termina, es **desalojado** y vuelve al final de la cola
- Respeta prioridades al reinsertar en la cola
- Mayor **equidad** pero más cambios de contexto

---

## 🛡️ Prevención de Deadlock

El sistema previene el interbloqueo mediante:

1. **Orden fijo de adquisición:** Siempre se adquiere primero el semáforo del muelle y luego el mutex del log. Nunca se invierte este orden, eliminando la espera circular.

2. **Secciones críticas mínimas:** El mutex del log se mantiene bloqueado solo el tiempo necesario para imprimir un mensaje y se libera de inmediato.

3. **Sin espera circular:** Ningún hilo espera un recurso que tiene otro hilo mientras ese otro espera un recurso del primero.

4. **`pthread_join` garantiza limpieza:** El proceso principal espera a todos los hilos antes de destruir semáforos y mutexes.

---

## 📊 Resultados — Tabla Comparativa

| Algoritmo | Camión | Prioridad | Espera (s) | Turnaround (s) |
|-----------|--------|-----------|-----------|----------------|
| FIFO | #1 | NORMAL | 0.00 | 3.01 |
| FIFO | #2 | ALTA | 0.00 | 2.00 |
| FIFO | #3 | NORMAL | 0.00 | 4.00 |
| FIFO | #4 | ALTA | 1.80 | 2.80 |
| FIFO | #5 | NORMAL | 2.61 | 5.61 |
| FIFO | #6 | ALTA | 2.60 | 4.62 |
| FIFO | #7 | NORMAL | 3.60 | 4.60 |
| FIFO | #8 | ALTA | 4.41 | 7.41 |
| **FIFO** | **PROMEDIO** | — | **1.88** | **4.26** |
| Round Robin | #1 | NORMAL | 2.83 | 5.83 |
| Round Robin | #2 | ALTA | 0.71 | 2.71 |
| Round Robin | #3 | NORMAL | 2.64 | 6.64 |
| Round Robin | #4 | ALTA | 2.51 | 3.51 |
| Round Robin | #5 | NORMAL | 3.43 | 6.43 |
| Round Robin | #6 | ALTA | 2.32 | 4.32 |
| Round Robin | #7 | NORMAL | 3.21 | 4.21 |
| Round Robin | #8 | ALTA | 4.12 | 7.12 |
| **Round Robin** | **PROMEDIO** | — | **2.72** | **5.10** |

### Análisis

| Algoritmo | Espera Promedio | Turnaround Promedio | Observación |
|-----------|----------------|---------------------|-------------|
| FIFO | 1.88s | 4.26s | Efecto Convoy en camiones tardíos |
| Round Robin | 2.72s | 5.10s | Mayor equidad, más cambios de contexto |

- **FIFO** fue más eficiente en tiempo promedio para cargas cortas, pero el camión #8 esperó 4.41s atrapado detrás de otros (Efecto Convoy).
- **Round Robin** distribuyó la espera más equitativamente. Ningún camión superó los 4.12s de espera.
- Los camiones con **prioridad ALTA** obtuvieron menores tiempos en ambos algoritmos, validando el sistema de prioridades.

---

## 📁 Estructura del Proyecto

```
terminal-carga-SO/
│
├── terminal.c      # Código fuente principal
└── README.md       # Documentación del proyecto
```
```
