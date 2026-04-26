#ifndef TIPOS_H
#define TIPOS_H

/* ─── Variables globales ─── */
#define NUM_MUELLES     3
#define NUM_CAMIONES    8
#define QUANTUM         2

/* ─── Estados del hilo (ciclo de vida del proceso) ─── */
typedef enum {
    NUEVO,
    LISTO,
    EJECUCION,
    BLOQUEADO,
    TERMINADO
} Estado;

/* ─── Estructura de cada Camión (hilo) ─── */
typedef struct {
    int    id;
    int    empresa_id;   // ID de la empresa (proceso) que lo envía
    int    prioridad;    // 1=alta (perecedero), 2=normal
    int    burst;
    int    restante;
    double t_llegada;
    double t_inicio;
    double t_fin;
    double t_espera;
    Estado estado;
} Camion;

/* ─── Cola circular con prioridad para Round Robin ─── */
#include <pthread.h>
typedef struct {
    Camion *camiones[NUM_CAMIONES];
    int frente, final, cantidad;
    pthread_mutex_t mutex;
    pthread_cond_t  no_vacia;
} Cola;

/* ─── Inventario compartido (sección crítica adicional) ─── */
typedef struct {
    int unidades_cargadas;   // total acumulado de todas las cargas
    int muelles_ocupados;    // contador de muelles en uso actualmente
} Inventario;

#endif 
