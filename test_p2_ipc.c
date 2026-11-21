/* test_p2_ipc.c — teste P2 dans de vraies conditions : mmap + sémaphores + fork.
 * Le parent joue le rôle d'un faux P1 (il publie des mesures) et observe l'état.
 * Compilation : gcc -Wall -Wextra -pthread -o test_p2_ipc test_p2_ipc.c analysis.c
 */
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "analysis.h"

static struct shared_data *shm;

/* Faux P1 : publie une mesure sous sémaphore */
static void publier(float temp, int fumee, int flamme)
{
    sem_wait(&shm->mutex);
    shm->mesures.sequence++;
    clock_gettime(CLOCK_MONOTONIC, &shm->mesures.timestamp);
    shm->mesures.temperature = temp;
    shm->mesures.humidity = 45.0f;
    shm->mesures.smoke_value = fumee;
    shm->mesures.flame_detected = flamme;
    shm->mesures.sensor_status = CAPTEURS_TOUS_OK;
    sem_post(&shm->mutex);
}

static int lire_etat(void)
{
    sem_wait(&shm->mutex);
    int e = shm->system_state;
    sem_post(&shm->mutex);
    return e;
}

static void phase(const char *titre, int n, float temp, int fumee, int flamme)
{
    printf("\n--- %s ---\n", titre);
    for (int i = 0; i < n; i++) {
        publier(temp, fumee, flamme);
        sleep(1);
        printf("[P1 simule] mesure %lu publiee, etat lu : %s\n",
               shm->mesures.sequence, nom_etat(lire_etat()));
    }
}

int main(void)
{
    /* Ce que fera le superviseur (personne A) */
    shm = mmap(NULL, sizeof *shm, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) { perror("mmap"); return 1; }
    sem_init(&shm->mutex, 1, 1);     /* 1 = partagé entre processus */
    sem_init(&shm->notif_p3, 1, 0);
    shm->mesures.sequence = 0;
    shm->system_state = ETAT_DEFAUT;

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {                  /* enfant = P2 */
        processus_analyse(shm);
        _exit(0);
    }

    phase("Valeurs normales", 4, 22.0f, 120, 0);
    phase("Temperature + fumee (alerte)", 4, 55.0f, 650, 0);
    phase("Retour a la normale", 4, 22.0f, 120, 0);

    printf("\n--- P1 arrete de publier (donnee perimee) ---\n");
    for (int i = 0; i < 8; i++) {
        sleep(1);
        printf("[P1 simule] silence %d s, etat lu : %s\n", i + 1, nom_etat(lire_etat()));
    }

    /* Arrêt propre */
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    sem_destroy(&shm->mutex);
    sem_destroy(&shm->notif_p3);
    munmap(shm, sizeof *shm);
    printf("\nRessources liberees, fin du test.\n");
    return 0;
}
