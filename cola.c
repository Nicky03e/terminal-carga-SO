#include "cola.h"
#include "globals.h"

/* ════════════════════════════════════════════════════════════════
   COLA CIRCULAR CON PRIORIDAD
   - Inserción: el nuevo camión se pone al final y luego se ordena
     con burbuja sobre el arreglo circular.
   - Prioridad 1 (perecedero) siempre queda delante de prioridad 2.
   - El mutex protege la cola de accesos concurrentes.
   ════════════════════════════════════════════════════════════════ */

void cola_agregar(Cola *c, Camion *cam) {
    pthread_mutex_lock(&c->mutex);

    int cantidad_actual = c->cantidad;

    c->camiones[c->final] = cam;
    c->final    = (c->final + 1) % NUM_CAMIONES;
    c->cantidad++;

    /* Ordenar por prioridad (burbuja sobre cola circular) */
    for (int i = 0; i < cantidad_actual; i++) {
        int idx_a = (c->frente + i)     % NUM_CAMIONES;
        int idx_b = (c->frente + i + 1) % NUM_CAMIONES;
        if (c->camiones[idx_a]->prioridad > c->camiones[idx_b]->prioridad) {
            Camion *tmp        = c->camiones[idx_a];
            c->camiones[idx_a] = c->camiones[idx_b];
            c->camiones[idx_b] = tmp;
        }
    }

    pthread_cond_signal(&c->no_vacia);
    pthread_mutex_unlock(&c->mutex);
}

Camion *cola_sacar(Cola *c) {
    pthread_mutex_lock(&c->mutex);

    /* Esperar mientras la cola esté vacía, pero salir si ya no
     * quedan camiones activos. */
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
    c->frente   = (c->frente + 1) % NUM_CAMIONES;
    c->cantidad--;

    pthread_mutex_unlock(&c->mutex);
    return cam;
}
