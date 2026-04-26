#ifndef COLA_H
#define COLA_H

#include "tipos.h"

/* ─── Inserta un camión en la cola respetando prioridad ─── */
void cola_agregar(Cola *c, Camion *cam);

/* ─── Extrae el próximo camión de la cola (bloquea si está vacía) ─── */
Camion *cola_sacar(Cola *c);

#endif 
