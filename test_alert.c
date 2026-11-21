/* test_alert.c — test unitaire IPC du processus P3 avec analysis.c */
#include "alert.h"
#include "shared_data.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static void simuler_changement_etat(struct shared_data *shm, unsigned long seq,
                                    float temp, float hum, int fumee, int flamme,
                                    unsigned int status, int nouvel_etat) {
    sem_wait(&shm->mutex);
    shm->mesures.sequence = seq;
    clock_gettime(CLOCK_MONOTONIC, &shm->mesures.timestamp);
    shm->mesures.temperature = temp;
    shm->mesures.humidity = hum;
    shm->mesures.smoke_value = fumee;
    shm->mesures.flame_detected = flamme;
    shm->mesures.sensor_status = status;
    shm->system_state = nouvel_etat;
    sem_post(&shm->mutex);

    sem_post(&shm->notif_p3);
    sleep(1);
}

int main(void) {
    const char *log_test = "test_journal_p3.log";
    remove(log_test);

    struct shared_data *shm = mmap(NULL, sizeof(*shm),
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    sem_init(&shm->mutex, 1, 1);
    sem_init(&shm->notif_p3, 1, 0);
    shm->system_state = ETAT_DEFAUT;

    pid_t pid_p3 = fork();
    if (pid_p3 < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid_p3 == 0) {
        processus_alerte(shm, log_test);
        exit(EXIT_SUCCESS);
    }

    /* Simulation des transitions d'état envoyées par P2 */
    simuler_changement_etat(shm, 1, 23.0f, 45.0f, 100, 0, CAPTEURS_TOUS_OK, ETAT_NORMAL);
    simuler_changement_etat(shm, 2, 48.5f, 38.0f, 420, 0, CAPTEURS_TOUS_OK, ETAT_ATTENTION);
    simuler_changement_etat(shm, 3, 67.0f, 25.0f, 810, 1, CAPTEURS_TOUS_OK, ETAT_ALERTE);
    simuler_changement_etat(shm, 4, -1.0f, -1.0f, 0,   0, 0x0u,            ETAT_DEFAUT);
    simuler_changement_etat(shm, 5, 24.5f, 44.0f, 110, 0, CAPTEURS_TOUS_OK, ETAT_NORMAL);

    /* Arrêt propre de P3 */
    kill(pid_p3, SIGTERM);
    waitpid(pid_p3, NULL, 0);

    sem_destroy(&shm->notif_p3);
    sem_destroy(&shm->mutex);
    munmap(shm, sizeof(*shm));

    printf("\nTest termine. Contenu de '%s' :\n", log_test);
    FILE *f = fopen(log_test, "r");
    if (f) {
        char ligne[256];
        while (fgets(ligne, sizeof(ligne), f)) {
            printf("  %s", ligne);
        }
        fclose(f);
    }
    remove(log_test);

    return EXIT_SUCCESS;
}
