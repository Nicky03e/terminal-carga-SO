#ifndef GLOBALS_H
#define GLOBALS_H

#include <pthread.h>
#include <semaphore.h>
#include "tipos.h"

/* ─── Semáforo para controlar acceso a los muelles (recurso limitado) ─── */
extern sem_t muelles;

/* ─── Mutex para el log de operaciones (evitar race conditions en escritura) ─── */
extern pthread_mutex_t log_mutex;

/* ─── Mutex para el inventario compartido ─── */
extern pthread_mutex_t mutex_inventario;
extern Inventario inventario_global;

/* ─── Tiempo de inicio de la simulación ─── */
extern double t_inicio_sim;

/* ─── Cola Round Robin compartida ─── */
extern Cola cola_rr;

/* ─── Contador de camiones activos (para terminar el planificador RR) ─── */
extern int camiones_activos;
extern pthread_mutex_t mutex_activos;

/* ─── Cola FIFO compartida ─── */
extern Camion *cola_fifo[NUM_CAMIONES];
extern int     cola_fifo_size;
extern pthread_mutex_t mutex_fifo;
extern pthread_cond_t  cond_fifo;
extern int             fifo_turno;

#endif 