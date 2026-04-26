# Sistema de Gestión de una Terminal de Carga Automatizada
# EIF 212 — Sistemas Operativos | I Ciclo 2026

---

## Integrantes
- Nicole Masis Brenes
- Brenda Serrano Jimenez 
- Mariana Madrigal Molina
- Joseph Elías Ulate Aguilar

---

## Descripción General

El sistema simula una terminal donde múltiples Camiones (hilos) compiten por acceder a Muelles de Carga (Sección Crítica). El programa implementa:

- Hilos POSIX (`pthreads`) con ciclo de vida completo de 5 estados
- Semáforos para controlar el acceso a los 3 muelles simultáneos
- Mutex para proteger el log de operaciones compartido
- Dos algoritmos de planificación: FIFO y Round Robin con prioridad

---

## Compilación y Ejecución

```bash
gcc -Wall *.c -o terminal_carga -I include -lpthread
./terminal_carga
```

---

## Componentes principales

| Componente | Descripción |
|------------|-------------|
| `main()` | Proceso principal. Inicializa recursos, lanza hilos y espera con `pthread_join` |
| Hilos (Camiones) | Creados con `pthread_create`. Cada uno tiene burst, prioridad y ciclo de vida |
| `sem_t muelles` | Semáforo inicializado en 3. Controla acceso simultáneo a los muelles |
| `pthread_mutex_t log_mutex` | Mutex que protege el log de operaciones compartido |
| Cola con prioridad | Ordena camiones: prioridad 1 (perecederos) antes que prioridad 2 (normal) |

## Ciclo de vida de cada Camión (hilo)

pthread_create()
      │
      ▼
   NUEVO ──► LISTO ──► BLOQUEADO ──► EJECUCIÓN ──► TERMINADO
                          │               │
                     sem_wait()      sem_post()
                     (espera muelle) (libera muelle)


| Estado | Descripción |
|--------|-------------|
| `NUEVO` | Hilo instanciado con `pthread_create` |
| `LISTO` | En cola de planificación esperando turno |
| `BLOQUEADO` | Esperando `sem_wait()` porque los 3 muelles están ocupados |
| `EJECUCIÓN` | Dentro de la Sección Crítica usando el muelle |
| `TERMINADO` | Finalizó, recolectado por el padre con `pthread_join` |

---

## Sincronización

### Semáforos — Control de Muelles

- sem_init(&muelles, 0, 3);  -  Máximo 3 camiones simultáneos
- sem_wait(&muelles);        - Entra al muelle (bloquea si están llenos)
- // --- Sección Crítica ---
- sem_post(&muelles);        - Sale del muelle (libera espacio)


### Mutex — Log de Operaciones

- pthread_mutex_lock(&log_mutex);   -  Entra a sección crítica del log
- printf("...");                    -  Escribe sin interferencia
- pthread_mutex_unlock(&log_mutex); -  Libera el log

### Mutex — Inventario Compartido

```c
pthread_mutex_lock(&mutex_inventario);
inventario_global.unidades_cargadas += cantidad_cargada;
pthread_mutex_unlock(&mutex_inventario);
```

---

##  Algoritmos de Planificación

### FIFO con Prioridad
- Los camiones se atienden en orden de llegada
- Los de prioridad 1 se insertan al frente de la cola
- Un camión ocupa el muelle hasta completar toda su carga
- Puede generar Efecto Convoy si un camión lento bloquea a los demás

### Round Robin con Prioridad
- Cada camión ocupa el muelle máximo QUANTUM = 2 segundos
- Si no termina, es desalojado y vuelve al final de la cola
- Respeta prioridades al reinsertar en la cola
- Mayor equidad pero más cambios de contexto

---

## Prevención de Deadlock

El sistema previene el interbloqueo mediante:

1. Orden fijo de adquisición Siempre se adquiere primero el semáforo del muelle y luego el mutex del log. Nunca se invierte este orden, eliminando la espera circular.

2. Secciones críticas mínimas El mutex del log se mantiene bloqueado solo el tiempo necesario para imprimir un mensaje y se libera de inmediato.

3. Sin espera circular: Ningún hilo espera un recurso que tiene otro hilo mientras ese otro espera un recurso del primero.

4. `pthread_join` garantiza limpieza: El proceso principal espera a todos los hilos antes de destruir semáforos y mutexes.

---

## Resultados — Tabla Comparativa

| Algoritmo | Camión | Empresa | Prioridad | Espera (s) | Turnaround (s) |
|-----------|--------|---------|-----------|------------|----------------|
| FIFO | #1 | Emp #1 | NORMAL | 0.00 | 3.00 |
| FIFO | #2 | Emp #1 | ALTA | 0.00 | 2.00 |
| FIFO | #3 | Emp #2 | NORMAL | 2.80 | 6.80 |
| FIFO | #4 | Emp #2 | ALTA | 1.80 | 2.80 |
| FIFO | #5 | Emp #3 | NORMAL | 4.60 | 7.60 |
| FIFO | #6 | Emp #3 | ALTA | 2.50 | 4.50 |
| FIFO | #7 | Emp #4 | NORMAL | 6.40 | 7.40 |
| FIFO | #8 | Emp #4 | ALTA | 2.40 | 5.40 |
| **FIFO** | **PROMEDIO** | — | — | **2.56** | **4.94** |
| Round Robin | #1 | Emp #1 | NORMAL | 2.81 | 5.81 |
| Round Robin | #2 | Emp #1 | ALTA | 0.70 | 2.70 |
| Round Robin | #3 | Emp #2 | NORMAL | 3.60 | 7.60 |
| Round Robin | #4 | Emp #2 | ALTA | 0.50 | 1.50 |
| Round Robin | #5 | Emp #3 | NORMAL | 3.40 | 6.40 |
| Round Robin | #6 | Emp #3 | ALTA | 0.30 | 2.30 |
| Round Robin | #7 | Emp #4 | NORMAL | 4.20 | 5.20 |
| Round Robin | #8 | Emp #4 | ALTA | 3.10 | 6.10 |
| **Round Robin** | **PROMEDIO** | — | — | **2.33** | **4.70** |


### Análisis

| Algoritmo | Espera Promedio | Turnaround Promedio | Observación |
|-----------|----------------|---------------------|-------------|
| FIFO | 2.56s | 4.94s | Efecto Convoy visible en camiones normales tardíos |
| Round Robin | 2.33s | 4.70s | Mayor equidad, más cambios de contexto |

- En las pruebas, FIFO mostró el Efecto Convoy claramente: el Camión #7 (normal) esperó 6.40 segundos porque los camiones de alta prioridad se atendieron primero. Eso es exactamente el problema que las prioridades buscan mitigar, pero dentro de la misma prioridad FIFO no ofrece equidad.
- Round Robin repartió mejor la espera entre todos los camiones. Ninguno superó los 4.20 segundos esperando, porque el quantum evita que un solo camión acapare el muelle. La consecuencia es un leve aumento en cambios de contexto.
- Lo que funcionó en ambos casos fue la prioridad: los camiones perecederos (prioridad 1) entraron antes que los normales, que era el objetivo principal del sistema.
