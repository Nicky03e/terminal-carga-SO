#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

/* ─── Constantes ─── */
#define NUM_MUELLES     3
#define NUM_CAMIONES    8
#define QUANTUM         2

/* ─── Estados del hilo ─── */
typedef enum {
    NUEVO, LISTO, EJECUCION, BLOQUEADO, TERMINADO
} Estado;

/* ─── Estructura de cada Camión ─── */
typedef struct {
    int    id;
    int    prioridad;   // 1=alta (perecedero), 2=normal
    int    burst;
    int    restante;
    double t_llegada;
    double t_inicio;
    double t_fin;
    double t_espera;
    Estado estado;
} Camion;

/* ─── Cola con prioridad para Round Robin ─── */
typedef struct {
    Camion *camiones[NUM_CAMIONES];
    int frente, final, cantidad;
    pthread_mutex_t mutex;
    pthread_cond_t  no_vacia;
} Cola;

/* ─── Variables globales ─── */
sem_t           muelles;
pthread_mutex_t log_mutex;
double          t_inicio_sim;
Cola            cola_rr;
int             camiones_activos = NUM_CAMIONES;
pthread_mutex_t mutex_activos;

/* ─── Tiempo transcurrido ─── */
double tiempo_actual() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9 - t_inicio_sim;
}

/* ─── Log protegido por mutex ─── */
void log_evento(int id, const char *evento, Estado estado) {
    const char *nombres[] = {"NUEVO","LISTO","EJECUCION","BLOQUEADO","TERMINADO"};
    pthread_mutex_lock(&log_mutex);
printf("[t=%06.2fs] Camion #%02d | %-24s | Estado: %-10s | Prioridad: %-16s\n",           tiempo_actual(), id, evento, nombres[estado],
           (id % 2 == 0) ? "ALTA(perecedero)" : "NORMAL");
    fflush(stdout);
    pthread_mutex_unlock(&log_mutex);
}

/* ════════════════════════════════════════
   COLA CON PRIORIDAD
   Inserta ordenado: prioridad 1 antes que 2
   ════════════════════════════════════════ */
void cola_agregar(Cola *c, Camion *cam) {
    pthread_mutex_lock(&c->mutex);

    // Insertar en posición correcta según prioridad
    int pos = c->final;
    int cantidad_actual = c->cantidad;

    // Mover elementos de menor prioridad hacia atrás
    // para insertar el nuevo camión en su lugar correcto
    c->camiones[c->final] = cam;
    c->final = (c->final + 1) % NUM_CAMIONES;
    c->cantidad++;

    // Ordenar por prioridad (burbuja simple sobre la cola circular)
    for (int i = 0; i < cantidad_actual; i++) {
        int idx_actual = (c->frente + i) % NUM_CAMIONES;
        int idx_siguiente = (c->frente + i + 1) % NUM_CAMIONES;
        if (c->camiones[idx_actual]->prioridad > c->camiones[idx_siguiente]->prioridad) {
            Camion *tmp = c->camiones[idx_actual];
            c->camiones[idx_actual] = c->camiones[idx_siguiente];
            c->camiones[idx_siguiente] = tmp;
        }
    }

    pthread_cond_signal(&c->no_vacia);
    pthread_mutex_unlock(&c->mutex);
}

Camion *cola_sacar(Cola *c) {
    pthread_mutex_lock(&c->mutex);
    while (c->cantidad == 0) {
        pthread_mutex_lock(&mutex_activos);
        int activos = camiones_activos;
        pthread_mutex_unlock(&mutex_activos);
        if (activos == 0) {
            pthread_mutex_unlock(&c->mutex);
            return NULL;
        }
        pthread_cond_wait(&c->no_vacia, &c->mutex);
    }
    Camion *cam = c->camiones[c->frente];
    c->frente = (c->frente + 1) % NUM_CAMIONES;
    c->cantidad--;
    pthread_mutex_unlock(&c->mutex);
    return cam;
}

/* ════════════════════════════════════════
   ALGORITMO 1: FIFO con prioridad
   ════════════════════════════════════════ */

// Arreglo compartido para ordenar FIFO por prioridad
Camion *cola_fifo[NUM_CAMIONES];
int     cola_fifo_size = 0;
pthread_mutex_t mutex_fifo;
pthread_cond_t  cond_fifo;
int             fifo_turno = 0;  // índice del próximo en ser atendido

void *ciclo_fifo(void *arg) {
    Camion *c = (Camion *)arg;

    c->estado = NUEVO;
    log_evento(c->id, "Creado", NUEVO);

    c->estado = LISTO;
    c->t_llegada = tiempo_actual();
    log_evento(c->id, "En cola FIFO", LISTO);

    // Registrar en cola FIFO ordenada por prioridad
    pthread_mutex_lock(&mutex_fifo);
    int mi_pos = cola_fifo_size;
    cola_fifo[cola_fifo_size++] = c;

    // Ordenar cola por prioridad
    for (int i = 0; i < cola_fifo_size - 1; i++) {
        for (int j = 0; j < cola_fifo_size - i - 1; j++) {
            if (cola_fifo[j]->prioridad > cola_fifo[j+1]->prioridad) {
                Camion *tmp   = cola_fifo[j];
                cola_fifo[j]  = cola_fifo[j+1];
                cola_fifo[j+1]= tmp;
            }
        }
    }
    // Encontrar mi nueva posición después de ordenar
    for (int i = 0; i < cola_fifo_size; i++) {
        if (cola_fifo[i]->id == c->id) { mi_pos = i; break; }
    }
    pthread_mutex_unlock(&mutex_fifo);

    // Esperar hasta que sea mi turno
    c->estado = BLOQUEADO;
    log_evento(c->id, "Esperando muelle", BLOQUEADO);

    pthread_mutex_lock(&mutex_fifo);
    while (fifo_turno != mi_pos)
        pthread_cond_wait(&cond_fifo, &mutex_fifo);
    pthread_mutex_unlock(&mutex_fifo);

    sem_wait(&muelles);

    c->estado = EJECUCION;
    c->t_inicio = tiempo_actual();
    c->t_espera = c->t_inicio - c->t_llegada;
    log_evento(c->id, "Usando muelle", EJECUCION);

    sleep(c->burst);

    sem_post(&muelles);

    // Avisar al siguiente
    pthread_mutex_lock(&mutex_fifo);
    fifo_turno++;
    pthread_cond_broadcast(&cond_fifo);
    pthread_mutex_unlock(&mutex_fifo);

    c->estado = TERMINADO;
    c->t_fin = tiempo_actual();
    log_evento(c->id, "Terminado", TERMINADO);

    pthread_mutex_lock(&log_mutex);
    printf("         Camion #%d | Prioridad: %d | Espera: %.2fs | Turnaround: %.2fs\n",
           c->id, c->prioridad, c->t_espera, c->t_fin - c->t_llegada);
    pthread_mutex_unlock(&log_mutex);

    return NULL;
}

/* ════════════════════════════════════════
   ALGORITMO 2: ROUND ROBIN con prioridad
   ════════════════════════════════════════ */
void *planificador_rr(void *arg) {
    (void)arg;

    while (1) {
        Camion *c = cola_sacar(&cola_rr);
        if (c == NULL) break;

        c->estado = BLOQUEADO;
        log_evento(c->id, "Esperando muelle RR", BLOQUEADO);
        sem_wait(&muelles);

        c->estado = EJECUCION;
        if (c->t_inicio == 0)
            c->t_inicio = tiempo_actual();
        log_evento(c->id, "Usando muelle RR", EJECUCION);

        int tiempo_uso = (c->restante > QUANTUM) ? QUANTUM : c->restante;
        sleep(tiempo_uso);
        c->restante -= tiempo_uso;

        sem_post(&muelles);

        if (c->restante > 0) {
            c->estado = LISTO;
            log_evento(c->id, "Desalojado, vuelve cola", LISTO);
            cola_agregar(&cola_rr, c);  // Reinserta respetando prioridad
        } else {
            c->estado = TERMINADO;
            c->t_fin = tiempo_actual();
            c->t_espera = c->t_fin - c->t_llegada - c->burst;
            log_evento(c->id, "Terminado RR", TERMINADO);

            pthread_mutex_lock(&log_mutex);
            printf("         Camion #%d | Prioridad: %d | Espera: %.2fs | Turnaround: %.2fs\n",
                   c->id, c->prioridad, c->t_espera, c->t_fin - c->t_llegada);
            pthread_mutex_unlock(&log_mutex);

            pthread_mutex_lock(&mutex_activos);
            camiones_activos--;
            if (camiones_activos == 0)
                pthread_cond_broadcast(&cola_rr.no_vacia);
            pthread_mutex_unlock(&mutex_activos);
        }
    }
    return NULL;
}

/* ─── Tabla comparativa ─── */
void imprimir_tabla(const char *algoritmo, Camion *cams, int n) {
    double suma_espera = 0, suma_turnaround = 0;
    printf("\n%-12s | %-6s | %-10s | %-12s | %-12s\n",
           "Algoritmo","Camion","Prioridad","Espera(s)","Turnaround(s)");
    printf("────────────────────────────────────────────────────────\n");
    for (int i = 0; i < n; i++) {
        double ta = cams[i].t_fin - cams[i].t_llegada;
        const char *prio = (cams[i].prioridad == 1) ? "ALTA" : "NORMAL";
        printf("%-12s | Cam #%-2d| %-10s | %-12.2f| %-12.2f\n",
               algoritmo, cams[i].id, prio, cams[i].t_espera, ta);
        suma_espera     += cams[i].t_espera;
        suma_turnaround += ta;
    }
    printf("────────────────────────────────────────────────────────\n");
    printf("%-12s | PROMEDIO            | %-12.2f| %-12.2f\n",
           algoritmo, suma_espera/n, suma_turnaround/n);
}

/* ════════════════════════════════════════
   MAIN
   ════════════════════════════════════════ */
int main() {
    //  prioridad 1 = alta (perecedero), 2 = normal
    // {id, prioridad, burst, restante, t_llegada, t_inicio, t_fin, t_espera, estado}
    Camion datos[NUM_CAMIONES] = {
        {1, 2, 3, 3, 0,0,0,0, NUEVO},  // normal
        {2, 1, 2, 2, 0,0,0,0, NUEVO},  // perecedero
        {3, 2, 4, 4, 0,0,0,0, NUEVO},  // normal
        {4, 1, 1, 1, 0,0,0,0, NUEVO},  // perecedero
        {5, 2, 3, 3, 0,0,0,0, NUEVO},  // normal
        {6, 1, 2, 2, 0,0,0,0, NUEVO},  // perecedero
        {7, 2, 1, 1, 0,0,0,0, NUEVO},  // normal
        {8, 1, 3, 3, 0,0,0,0, NUEVO},  // perecedero
    };

    pthread_mutex_init(&log_mutex,    NULL);
    pthread_mutex_init(&mutex_activos,NULL);
    pthread_mutex_init(&mutex_fifo,   NULL);
    pthread_cond_init(&cond_fifo,     NULL);

    /* ══════════════
       FIFO
       ══════════════ */
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║   SIMULACION FIFO (con prioridad)    ║\n");
    printf("╚══════════════════════════════════════╝\n");

    Camion fifo[NUM_CAMIONES];
    memcpy(fifo, datos, sizeof(datos));
    cola_fifo_size = 0;
    fifo_turno     = 0;

    sem_init(&muelles, 0, NUM_MUELLES);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t_inicio_sim = ts.tv_sec + ts.tv_nsec / 1e9;

    pthread_t hilos_fifo[NUM_CAMIONES];
    for (int i = 0; i < NUM_CAMIONES; i++) {
        pthread_create(&hilos_fifo[i], NULL, ciclo_fifo, &fifo[i]);
        usleep(100000);
    }
    for (int i = 0; i < NUM_CAMIONES; i++)
        pthread_join(hilos_fifo[i], NULL);

    sem_destroy(&muelles);

    /* ══════════════
       ROUND ROBIN
       ══════════════ */
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║  SIMULACION ROUND ROBIN (prioridad)  ║\n");
    printf("╚══════════════════════════════════════╝\n");

    Camion rr[NUM_CAMIONES];
    memcpy(rr, datos, sizeof(datos));

    cola_rr.frente = cola_rr.final = cola_rr.cantidad = 0;
    pthread_mutex_init(&cola_rr.mutex, NULL);
    pthread_cond_init(&cola_rr.no_vacia, NULL);
    camiones_activos = NUM_CAMIONES;

    sem_init(&muelles, 0, NUM_MUELLES);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t_inicio_sim = ts.tv_sec + ts.tv_nsec / 1e9;

    for (int i = 0; i < NUM_CAMIONES; i++) {
        rr[i].t_llegada = tiempo_actual();
        rr[i].estado = LISTO;
        log_evento(rr[i].id, "En cola RR", LISTO);
        cola_agregar(&cola_rr, &rr[i]);
        usleep(100000);
    }

    pthread_t planificadores[NUM_MUELLES];
    for (int i = 0; i < NUM_MUELLES; i++)
        pthread_create(&planificadores[i], NULL, planificador_rr, NULL);
    for (int i = 0; i < NUM_MUELLES; i++)
        pthread_join(planificadores[i], NULL);

    sem_destroy(&muelles);

    /* ══════════════
       TABLA FINAL
       ══════════════ */
    printf("\n\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║              TABLA COMPARATIVA FINAL                    ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    imprimir_tabla("FIFO",       fifo, NUM_CAMIONES);
    imprimir_tabla("RoundRobin", rr,   NUM_CAMIONES);

    pthread_mutex_destroy(&log_mutex);
    pthread_mutex_destroy(&mutex_activos);
    pthread_mutex_destroy(&mutex_fifo);
    pthread_cond_destroy(&cond_fifo);
    pthread_mutex_destroy(&cola_rr.mutex);
    pthread_cond_destroy(&cola_rr.no_vacia);

    return 0;
}