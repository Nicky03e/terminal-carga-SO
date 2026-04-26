#ifndef PLANIFICADOR_H
#define PLANIFICADOR_H

#include "tipos.h"

/*
 * ─── ALGORITMO FIFO CON PRIORIDAD ───
 * Los camiones de prioridad alta se atienden antes,
 * pero dentro de cada prioridad se respeta el orden de llegada.
 */
void *ciclo_fifo(void *arg);

/*
 * ─── ALGORITMO ROUND ROBIN CON PRIORIDAD ───
 * Función del hilo planificador. Gestiona la cola RR:
 * cada camión usa el muelle por un máximo de QUANTUM segundos.
 * Si no termina, vuelve a la cola respetando su prioridad.
 * Se lanza un hilo planificador por cada muelle disponible.
 */
void *planificador_rr(void *arg);

#endif 