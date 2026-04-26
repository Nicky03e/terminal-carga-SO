#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#include "tipos.h"
#include "globals.h"
#include "utils.h"
#include "cola.h"
#include "planificador.h"

/* ─── Inicializa todos los mutexes, condiciones y semáforo ─── */
static void inicializar_sincronizacion(void) {
    pthread_mutex_init(&log_mutex,       NULL);
    pthread_mutex_init(&mutex_activos,   NULL);
    pthread_mutex_init(&mutex_inventario,NULL);
    pthread_mutex_init(&mutex_fifo,      NULL);
    pthread_cond_init(&cond_fifo,        NULL);
    sem_init(&muelles, 0, NUM_MUELLES);
}

/* ─── Destruye todos los recursos de sincronización ─── */
static void destruir_sincronizacion(void) {
    pthread_mutex_destroy(&log_mutex);
    pthread_mutex_destroy(&mutex_activos);
    pthread_mutex_destroy(&mutex_inventario);
    pthread_mutex_destroy(&mutex_fifo);
    pthread_cond_destroy(&cond_fifo);
    sem_destroy(&muelles);
}

/* ─── Reinicia el reloj de simulación ─── */
static void reiniciar_reloj(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t_inicio_sim = ts.tv_sec + ts.tv_nsec / 1e9;
}

/* ────────────────────────────────────────────────────────────────
   SIMULACIÓN FIFO
   Se crea un hilo por camión. Cada hilo ejecuta ciclo_fifo().
   Los hilos se crean con pequeño delay para simular llegadas.
   ──────────────────────────────────────────────────────────────── */
static void correr_fifo(Camion *fifo) {
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║   SIMULACION FIFO (con prioridad)        ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    cola_fifo_size = 0;
    fifo_turno     = 0;
    sem_init(&muelles, 0, NUM_MUELLES);
    reiniciar_reloj();

    pthread_t hilos[NUM_CAMIONES];
    for (int i = 0; i < NUM_CAMIONES; i++) {
        pthread_create(&hilos[i], NULL, ciclo_fifo, &fifo[i]);
        usleep(100000); /* 100ms entre llegadas */
    }
    for (int i = 0; i < NUM_CAMIONES; i++)
        pthread_join(hilos[i], NULL);

    sem_destroy(&muelles);

    pthread_mutex_lock(&log_mutex);
    printf("\n  [Inventario FIFO] Unidades cargadas totales: %d\n",
           inventario_global.unidades_cargadas);
    pthread_mutex_unlock(&log_mutex);
    inventario_global.unidades_cargadas = 0; /* resetear para RR */
}

/* ────────────────────────────────────────────────────────────────
   SIMULACIÓN ROUND ROBIN
   Se crean NUM_MUELLES hilos planificadores. Los camiones se
   insertan en cola_rr y los planificadores los atienden en quantum.
   ──────────────────────────────────────────────────────────────── */
static void correr_rr(Camion *rr) {
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║  SIMULACION ROUND ROBIN (prioridad)      ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    cola_rr.frente = cola_rr.final = cola_rr.cantidad = 0;
    pthread_mutex_init(&cola_rr.mutex,    NULL);
    pthread_cond_init(&cola_rr.no_vacia,  NULL);
    camiones_activos = NUM_CAMIONES;

    sem_init(&muelles, 0, NUM_MUELLES);
    reiniciar_reloj();

    for (int i = 0; i < NUM_CAMIONES; i++) {
        rr[i].t_llegada = tiempo_actual();
        rr[i].estado    = LISTO;
        log_evento(rr[i].id, rr[i].empresa_id, "En cola RR", LISTO);
        cola_agregar(&cola_rr, &rr[i]);
        usleep(100000);
    }

    pthread_t planificadores[NUM_MUELLES];
    for (int i = 0; i < NUM_MUELLES; i++)
        pthread_create(&planificadores[i], NULL, planificador_rr, NULL);
    for (int i = 0; i < NUM_MUELLES; i++)
        pthread_join(planificadores[i], NULL);

    sem_destroy(&muelles);
    pthread_mutex_destroy(&cola_rr.mutex);
    pthread_cond_destroy(&cola_rr.no_vacia);

    pthread_mutex_lock(&log_mutex);
    printf("\n  [Inventario RR] Unidades cargadas totales: %d\n",
           inventario_global.unidades_cargadas);
    pthread_mutex_unlock(&log_mutex);
}

/* ════════════════════════════════════════════════════════════════
   MAIN — Servidor de la Terminal
   ════════════════════════════════════════════════════════════════ */
int main(void) {
    Camion datos[NUM_CAMIONES] = {
        {1, 1, 2, 3, 3, 0,0,0,0, NUEVO},  
        {2, 1, 1, 2, 2, 0,0,0,0, NUEVO},  
        {3, 2, 2, 4, 4, 0,0,0,0, NUEVO},  
        {4, 2, 1, 1, 1, 0,0,0,0, NUEVO},  
        {5, 3, 2, 3, 3, 0,0,0,0, NUEVO},  
        {6, 3, 1, 2, 2, 0,0,0,0, NUEVO}, 
        {7, 4, 2, 1, 1, 0,0,0,0, NUEVO},  
        {8, 4, 1, 3, 3, 0,0,0,0, NUEVO},  
    };

    inicializar_sincronizacion();

    /* ── FIFO ── */
    Camion fifo[NUM_CAMIONES];
    memcpy(fifo, datos, sizeof(datos));
    correr_fifo(fifo);

    /* ── ROUND ROBIN ── */
    Camion rr[NUM_CAMIONES];
    memcpy(rr, datos, sizeof(datos));
    correr_rr(rr);

    /* ── TABLA COMPARATIVA FINAL ── */
    printf("\n\n╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                  TABLA COMPARATIVA FINAL                        ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    imprimir_tabla("FIFO",       fifo, NUM_CAMIONES);
    imprimir_tabla("RoundRobin", rr,   NUM_CAMIONES);

    destruir_sincronizacion();
    return 0;
}
