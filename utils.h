#ifndef UTILS_H
#define UTILS_H

#include "tipos.h"

/* ─── Retorna el tiempo transcurrido desde el inicio de la simulación ─── */
double tiempo_actual(void);

/* ─── Registra un evento en el log protegido por mutex ─── */
void log_evento(int id, int empresa_id, const char *evento, Estado estado);

/* ─── Actualiza el inventario compartido (sección crítica con mutex) ─── */
void registrar_carga_inventario(int cantidad_cargada);

/* ─── Imprime la tabla comparativa de resultados ─── */
void imprimir_tabla(const char *algoritmo, Camion *cams, int n);

#endif 