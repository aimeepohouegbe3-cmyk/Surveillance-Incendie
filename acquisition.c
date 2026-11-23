
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>

#include "shared_data.h"
#include "sensors.h"

void acquisition_process(struct shared_data *shared,
                         volatile sig_atomic_t *stop)
{
    unsigned long sequence = 0;
    int scenario = 0;
    int compteur_scenario = 0;

    while (!(*stop))
    {
        /* 1. Lecture des capteurs. */
        struct mesures m = read_simulated_sensors(scenario);

        /* 2. Horodatage. */
        if (clock_gettime(CLOCK_MONOTONIC, &m.timestamp) == -1)
        {
            perror("Erreur clock_gettime");
            continue;
        }

        /* 3. Attente du sémaphore. */
        while (sem_wait(&shared->mutex) == -1)
        {
            if (errno == EINTR)
                continue;

            perror("Erreur sem_wait");
            return;
        }

        /* 4. Publication des mesures. */
        m.sequence = ++sequence;
        shared->mesures = m;

        /* 5. Libération du sémaphore. */
        if (sem_post(&shared->mutex) == -1)
        {
            perror("Erreur sem_post");
            return;
        }

        /* 6. Affichage. */
        printf("P1 - Mesure numero %lu\n", m.sequence);
        printf("Temperature : %.1f C\n", m.temperature);
        printf("Humidite : %.1f %%\n", m.humidity);
        printf("Fumee : %d\n", m.smoke_value);
        printf("Flamme : %d\n", m.flame_detected);
        printf("Etat des capteurs : %u\n\n", m.sensor_status);

        fflush(stdout);

        /* Scénario suivant après trois mesures. */
        compteur_scenario++;

        if (compteur_scenario >= 3)
        {
            scenario = (scenario + 1) % 3;
            compteur_scenario = 0;
        }

        sleep(2);
    }

    printf("[P1] Arret propre de l'acquisition.\n");
    fflush(stdout);
}
