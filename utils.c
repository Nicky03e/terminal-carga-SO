#include <stdio.h>
#include <time.h>
#include "utils.h"
#include "globals.h"

double tiempo_actual(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9 - t_inicio_sim;
}

/*
 * El mutex evita condiciones de carrera cuando múltiples hilos
 * intentan escribir en el log simultáneamente.
 */
void log_evento(int id, int empresa_id, const char *evento, Estado estado) {
    const char *nombres[] = {"NUEVO", "LISTO", "EJECUCION", "BLOQUEADO", "TERMINADO"};

    pthread_mutex_lock(&log_mutex);
    printf("[t=%06.2fs] Camion #%02d (Empresa %d) | %-24s | Estado: %-10s | Prioridad: %s\n",
           tiempo_actual(), id, empresa_id, evento, nombres[estado],
           (id % 2 == 0) ? "ALTA(perecedero)" : "NORMAL");
    fflush(stdout);
    pthread_mutex_unlock(&log_mutex);
}
/*
 * Sección crítica: protegida por mutex_inventario para evitar
 * que dos hilos modifiquen el inventario al mismo tiempo.
 *
 * Prevención de deadlock: log_mutex y mutex_inventario
 * no se toman anidados; cada función los adquiere y libera de
 * forma independiente, eliminando la posibilidad de espera circular.
 */
void registrar_carga_inventario(int cantidad_cargada) {
    pthread_mutex_lock(&mutex_inventario);
    inventario_global.unidades_cargadas += cantidad_cargada;
    pthread_mutex_unlock(&mutex_inventario);
}


void imprimir_tabla(const char *algoritmo, Camion *cams, int n) {
    double suma_espera = 0, suma_turnaround = 0;

    printf("\n%-12s | %-6s | %-8s | %-10s | %-12s | %-12s\n",
           "Algoritmo", "Camion", "Empresa", "Prioridad", "Espera(s)", "Turnaround(s)");
    printf("────────────────────────────────────────────────────────────────\n");

    for (int i = 0; i < n; i++) {
        double ta = cams[i].t_fin - cams[i].t_llegada;
        const char *prio = (cams[i].prioridad == 1) ? "ALTA" : "NORMAL";
        printf("%-12s | Cam #%-2d| Emp #%-3d| %-10s | %-12.2f| %-12.2f\n",
               algoritmo, cams[i].id, cams[i].empresa_id,
               prio, cams[i].t_espera, ta);
        suma_espera     += cams[i].t_espera;
        suma_turnaround += ta;
    }

    printf("────────────────────────────────────────────────────────────────\n");
    printf("%-12s | PROMEDIO                    | %-12.2f| %-12.2f\n",
           algoritmo, suma_espera / n, suma_turnaround / n);
}