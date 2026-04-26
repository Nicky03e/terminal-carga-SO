#include <stdio.h>
#include <unistd.h>
#include "planificador.h"
#include "globals.h"
#include "utils.h"
#include "cola.h"

/* ════════════════════════════════════════════════════════════════
   ALGORITMO 1: FIFO CON PRIORIDAD
   - Todos los hilos se crean casi al mismo tiempo.
   - Se insertan en cola_fifo ordenada por prioridad (burbuja).
   - Cada hilo espera su turno (fifo_turno == mi_pos).
   - Al obtener el muelle, ejecuta su burst completo sin desalojo.
   ════════════════════════════════════════════════════════════════ */
void *ciclo_fifo(void *arg) {
    Camion *c = (Camion *)arg;

    /* Estado NUEVO: el hilo acaba de crearse */
    c->estado = NUEVO;
    log_evento(c->id, c->empresa_id, "Creado", NUEVO);

    /* Estado LISTO: el hilo entra a la cola de planificación */
    c->estado    = LISTO;
    c->t_llegada = tiempo_actual();
    log_evento(c->id, c->empresa_id, "En cola FIFO", LISTO);

    /* Registrar en la cola FIFO y encontrar posición tras ordenar */
    pthread_mutex_lock(&mutex_fifo);
    int mi_pos = cola_fifo_size;
    cola_fifo[cola_fifo_size++] = c;

    /* Ordenar por prioridad */
    for (int i = 0; i < cola_fifo_size - 1; i++) {
        for (int j = 0; j < cola_fifo_size - i - 1; j++) {
            if (cola_fifo[j]->prioridad > cola_fifo[j + 1]->prioridad) {
                Camion *tmp      = cola_fifo[j];
                cola_fifo[j]     = cola_fifo[j + 1];
                cola_fifo[j + 1] = tmp;
            }
        }
    }
    /* Ubicar posición real después de ordenar */
    for (int i = 0; i < cola_fifo_size; i++) {
        if (cola_fifo[i]->id == c->id) { mi_pos = i; break; }
    }
    pthread_mutex_unlock(&mutex_fifo);

    /* Estado BLOQUEADO: esperando que le toque el turno en el muelle */
    c->estado = BLOQUEADO;
    log_evento(c->id, c->empresa_id, "Esperando turno FIFO", BLOQUEADO);

    pthread_mutex_lock(&mutex_fifo);
    while (fifo_turno != mi_pos)
        pthread_cond_wait(&cond_fifo, &mutex_fifo);
    pthread_mutex_unlock(&mutex_fifo);

    /* Tomar el semáforo del muelle (recurso limitado) */
    sem_wait(&muelles);

    /* Estado EJECUCION: el hilo está dentro de la sección crítica */
    c->estado  = EJECUCION;
    c->t_inicio = tiempo_actual();
    c->t_espera = c->t_inicio - c->t_llegada;
    log_evento(c->id, c->empresa_id, "Usando muelle FIFO", EJECUCION);

    /* Simular la carga (burst completo, sin desalojo en FIFO) */
    sleep(c->burst);

    /* Actualizar inventario (sección crítica protegida por mutex) */
    registrar_carga_inventario(c->burst * 10);  /* 10 unidades por segundo */

    sem_post(&muelles);

    /* Avisar al siguiente camión en la cola */
    pthread_mutex_lock(&mutex_fifo);
    fifo_turno++;
    pthread_cond_broadcast(&cond_fifo);
    pthread_mutex_unlock(&mutex_fifo);

    /* Estado TERMINADO: el hilo completó su trabajo */
    c->estado = TERMINADO;
    c->t_fin  = tiempo_actual();
    log_evento(c->id, c->empresa_id, "Terminado FIFO", TERMINADO);

    pthread_mutex_lock(&log_mutex);
    printf("         Camion #%d (Empresa %d) | Prio: %d | Espera: %.2fs | Turnaround: %.2fs\n",
           c->id, c->empresa_id, c->prioridad,
           c->t_espera, c->t_fin - c->t_llegada);
    pthread_mutex_unlock(&log_mutex);

    return NULL;
}

/* ════════════════════════════════════════════════════════════════
   ALGORITMO 2: ROUND ROBIN CON PRIORIDAD
   - Un hilo planificador por cada muelle (NUM_MUELLES hilos).
   - Cada planificador saca un camión de la cola RR y le asigna
     el muelle por un máximo de QUANTUM segundos.
   - Si el camión no termina, vuelve a la cola (respetando prioridad).
   - Si termina, se decrementa camiones_activos.
   ════════════════════════════════════════════════════════════════ */
void *planificador_rr(void *arg) {
    (void)arg;

    while (1) {
        Camion *c = cola_sacar(&cola_rr);
        if (c == NULL) break;  /* No quedan camiones activos */

        /* Estado BLOQUEADO: esperando que se libere un muelle */
        c->estado = BLOQUEADO;
        log_evento(c->id, c->empresa_id, "Esperando muelle RR", BLOQUEADO);
        sem_wait(&muelles);

        /* Estado EJECUCION: ocupa la sección crítica */
        c->estado = EJECUCION;
        if (c->t_inicio == 0)
            c->t_inicio = tiempo_actual();
        log_evento(c->id, c->empresa_id, "Usando muelle RR", EJECUCION);

        /* Ejecutar por máximo QUANTUM segundos */
        int tiempo_uso = (c->restante > QUANTUM) ? QUANTUM : c->restante;
        sleep(tiempo_uso);
        c->restante -= tiempo_uso;

        /* Actualizar inventario por lo cargado en este quantum */
        registrar_carga_inventario(tiempo_uso * 10);

        sem_post(&muelles);

        if (c->restante > 0) {
            /* No terminó: vuelve a la cola (desalojo Round Robin) */
            c->estado = LISTO;
            log_evento(c->id, c->empresa_id, "Desalojado, vuelve a cola", LISTO);
            cola_agregar(&cola_rr, c);
        } else {
            /* Terminó: registrar tiempos y decrementar contador */
            c->estado   = TERMINADO;
            c->t_fin    = tiempo_actual();
            c->t_espera = c->t_fin - c->t_llegada - c->burst;
            log_evento(c->id, c->empresa_id, "Terminado RR", TERMINADO);

            pthread_mutex_lock(&log_mutex);
            printf("         Camion #%d (Empresa %d) | Prio: %d | Espera: %.2fs | Turnaround: %.2fs\n",
                   c->id, c->empresa_id, c->prioridad,
                   c->t_espera, c->t_fin - c->t_llegada);
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
