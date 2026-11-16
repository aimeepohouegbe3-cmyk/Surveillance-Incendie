#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <string.h>

#include "shared_data.h"
#include "sensors.h"
#include "analysis.h"
#include "alert.h"

void acquisition_process(struct shared_data *shared,
                         volatile sig_atomic_t *stop);
static volatile sig_atomic_t arret_superviseur = 0;
static volatile sig_atomic_t arret_p1 = 0;

/* Gestion des signaux du superviseur */
static void gestion_signal(int sig)
{
    (void)sig;
    arret_superviseur = 1;
}

/* Gestion de l'arrêt du processus P1 */
static void gestion_signal_p1(int sig)
{
    (void)sig;
    arret_p1 = 1;
}

int main(void)
{
    struct shared_data *shared;

    pid_t pid_p1 = -1;
    pid_t pid_p2 = -1;
    pid_t pid_p3 = -1;

    int mutex_initialise = 0;
    int notif_initialise = 0;

    /* 1. Installation des signaux du superviseur */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = gestion_signal;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("Erreur sigaction");
        return EXIT_FAILURE;
    }

    /* 2. Création de la mémoire partagée */
    shared = mmap(NULL, sizeof(*shared),
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS,
                  -1, 0);

    if (shared == MAP_FAILED)
    {
        perror("Erreur mmap");
        return EXIT_FAILURE;
    }

    memset(shared, 0, sizeof(*shared));

    /* 3. Initialisation des sémaphores */
    if (sem_init(&shared->mutex, 1, 1) == -1)
    {
        perror("Erreur sem_init mutex");
        goto nettoyage;
    }
    mutex_initialise = 1;

    if (sem_init(&shared->notif_p3, 1, 0) == -1)
    {
        perror("Erreur sem_init notif_p3");
        goto nettoyage;
    }
    notif_initialise = 1;

    shared->system_state = ETAT_DEFAUT;

    printf("[Superviseur] Memoire partagee et semaphores initialises.\n");

    /* 4. Création du processus P1 : acquisition */
    pid_p1 = fork();

    if (pid_p1 == -1)
    {
        perror("Erreur fork P1");
        goto nettoyage;
    }

    if (pid_p1 == 0)
    {
        struct sigaction sa_p1;
        memset(&sa_p1, 0, sizeof(sa_p1));
        sa_p1.sa_handler = gestion_signal_p1;
        sigemptyset(&sa_p1.sa_mask);

        sigaction(SIGINT, &sa_p1, NULL);
        sigaction(SIGTERM, &sa_p1, NULL);

        acquisition_process(shared, &arret_p1);
        _exit(EXIT_SUCCESS);
    }

    /* 5. Création du processus P2 : analyse */
    pid_p2 = fork();

    if (pid_p2 == -1)
    {
        perror("Erreur fork P2");
        goto nettoyage;
    }

    if (pid_p2 == 0)
    {
        processus_analyse(shared);
        _exit(EXIT_SUCCESS);
    }

    /* 6. Création du processus P3 : alertes */
    pid_p3 = fork();

    if (pid_p3 == -1)
    {
        perror("Erreur fork P3");
        goto nettoyage;
    }

    if (pid_p3 == 0)
    {
        processus_alerte(shared, "journal_incendie.log");
        _exit(EXIT_SUCCESS);
    }

    printf("[Superviseur] P1, P2 et P3 sont demarres.\n");
    printf("[Superviseur] Appuyez sur Ctrl+C pour arreter.\n");

    /* 7. Surveillance des processus enfants */
    while (!arret_superviseur)
    {
        int status;
        pid_t termine = waitpid(-1, &status, WNOHANG);

        if (termine > 0)
        {
            printf("[Superviseur] Un processus s'est termine (PID %d).\n",
                   (int)termine);

            if (termine == pid_p1) pid_p1 = -1;
            if (termine == pid_p2) pid_p2 = -1;
            if (termine == pid_p3) pid_p3 = -1;

            printf("[Superviseur] Arret du systeme demande.\n");
            break;
        }

        if (termine == -1 && errno != EINTR)
        {
            perror("Erreur waitpid");
            break;
        }

        sleep(1);
    }

    /* 8. Arrêt propre des processus encore actifs */
    printf("\n[Superviseur] Arret des processus...\n");

nettoyage:

    if (pid_p1 > 0) kill(pid_p1, SIGTERM);
    if (pid_p2 > 0) kill(pid_p2, SIGTERM);
    if (pid_p3 > 0) kill(pid_p3, SIGTERM);
/6
    if (pid_p1 > 0) waitpid(pid_p1, NULL, 0);
    if (pid_p2 > 0) waitpid(pid_p2, NULL, 0);
    if (pid_p3 > 0) waitpid(pid_p3, NULL, 0);

    /* 9. Libération des ressources */
    if (notif_initialise)
        sem_destroy(&shared->notif_p3);

    if (mutex_initialise)
        sem_destroy(&shared->mutex);

    munmap(shared, sizeof(*shared));

    printf("[Superviseur] Ressources liberees. Fin du programme.\n");

    return EXIT_SUCCESS;
}
