#include <pthread.h>
#include <semaphore.h>
#include "globals.h"

/* ─── Definición de todas las variables globales declaradas en globals.h ─── */

sem_t           muelles;
pthread_mutex_t log_mutex;
pthread_mutex_t mutex_inventario;
Inventario      inventario_global = {0, 0};
double          t_inicio_sim;

Cola            cola_rr;
int             camiones_activos = NUM_CAMIONES;
pthread_mutex_t mutex_activos;

Camion         *cola_fifo[NUM_CAMIONES];
int             cola_fifo_size = 0;
pthread_mutex_t mutex_fifo;
pthread_cond_t  cond_fifo;
int             fifo_turno = 0;
