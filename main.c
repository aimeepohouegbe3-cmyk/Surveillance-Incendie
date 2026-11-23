
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

void dashboard_publisher(struct shared_data *shared);

static volatile sig_atomic_t arret_superviseur = 0;
static volatile sig_atomic_t arret_p1 = 0;

static void gestion_signal(int sig)
{
    (void)sig;
    arret_superviseur = 1;
}

static void gestion_signal_p1(int sig)
{
    (void)sig;
    arret_p1 = 1;
}

/* Les enfants ne doivent pas réagir directement à Ctrl+C. */
static void ignorer_sigint(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
}

/* Arrêt et récupération d'un processus enfant. */
static void arreter_processus(pid_t *pid, const char *nom)
{
    if (*pid <= 0)
        return;

    printf("[Superviseur] Arret de %s...\n", nom);
    fflush(stdout);

    if (kill(*pid, SIGTERM) == -1 && errno != ESRCH)
        perror("[Superviseur] Erreur kill");

    while (waitpid(*pid, NULL, 0) == -1)
    {
        if (errno == EINTR)
            continue;

        if (errno != ECHILD)
            perror("[Superviseur] Erreur waitpid");

        break;
    }

    *pid = -1;
}

int main(void)
{
    struct shared_data *shared;

    pid_t pid_p1 = -1;
    pid_t pid_p2 = -1;
    pid_t pid_p3 = -1;
    pid_t pid_p4 = -1;

    int mutex_initialise = 0;
    int notif_initialise = 0;

    /* 1. Installation des signaux du superviseur. */
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

    /* 2. Création de la mémoire partagée. */
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

    /* 3. Initialisation des sémaphores. */
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
    shared->analysed_sequence = 0;

    printf("[Superviseur] Memoire partagee et semaphores initialises.\n");

    /* 4. Création de P1 : acquisition. */
    pid_p1 = fork();

    if (pid_p1 == -1)
    {
        perror("Erreur fork P1");
        goto nettoyage;
    }

    if (pid_p1 == 0)
    {
        ignorer_sigint();

        struct sigaction sa_p1;
        memset(&sa_p1, 0, sizeof(sa_p1));

        sa_p1.sa_handler = gestion_signal_p1;
        sigemptyset(&sa_p1.sa_mask);

        sigaction(SIGTERM, &sa_p1, NULL);

        acquisition_process(shared, &arret_p1);
        _exit(EXIT_SUCCESS);
    }

    /* 5. Création de P2 : analyse. */
    pid_p2 = fork();

    if (pid_p2 == -1)
    {
        perror("Erreur fork P2");
        goto nettoyage;
    }

    if (pid_p2 == 0)
    {
        ignorer_sigint();
        processus_analyse(shared);
        _exit(EXIT_SUCCESS);
    }

    /* 6. Création de P3 : alertes. */
    pid_p3 = fork();

    if (pid_p3 == -1)
    {
        perror("Erreur fork P3");
        goto nettoyage;
    }

    if (pid_p3 == 0)
    {
        ignorer_sigint();
        processus_alerte(shared, "journal_incendie.log");
        _exit(EXIT_SUCCESS);
    }

    /* 7. Création de P4 : tableau de bord. */
    pid_p4 = fork();

    if (pid_p4 == -1)
    {
        perror("Erreur fork P4");
        goto nettoyage;
    }

    if (pid_p4 == 0)
    {
        ignorer_sigint();
        dashboard_publisher(shared);
        _exit(EXIT_SUCCESS);
    }

    printf("[Superviseur] P1, P2, P3 et P4 sont demarres.\n");
    printf("[Superviseur] Appuyez sur Ctrl+C pour arreter.\n");

    /* 8. Surveillance des processus enfants. */
    while (!arret_superviseur)
    {
        int status;
        pid_t termine = waitpid(-1, &status, WNOHANG);

        if (termine > 0)
        {
            const char *nom_processus = "inconnu";

            if (termine == pid_p1)
            {
                nom_processus = "P1 - Acquisition";
                pid_p1 = -1;
            }
            else if (termine == pid_p2)
            {
                nom_processus = "P2 - Analyse";
                pid_p2 = -1;
            }
            else if (termine == pid_p3)
            {
                nom_processus = "P3 - Alertes";
                pid_p3 = -1;
            }
            else if (termine == pid_p4)
            {
                nom_processus = "P4 - Tableau de bord";
                pid_p4 = -1;
            }

            printf("\n[Superviseur] %s s'est termine.\n",
                   nom_processus);

            if (WIFEXITED(status))
                printf("[Superviseur] Code de sortie : %d\n",
                       WEXITSTATUS(status));

            else if (WIFSIGNALED(status))
                printf("[Superviseur] Termine par le signal : %d\n",
                       WTERMSIG(status));

            printf("[Superviseur] Arret du systeme demande.\n");
            break;
        }

        if (termine == -1)
        {
            if (errno == EINTR)
                continue;

            perror("[Superviseur] Erreur waitpid");
            break;
        }

        sleep(1);
    }

    /* 9. Arrêt coordonné des processus. */
    printf("\n[Superviseur] Arret coordonne des processus...\n");
    fflush(stdout);

nettoyage:

    /* P1 termine d'abord ses acquisitions. */
    arreter_processus(&pid_p1, "P1 - Acquisition");

    /* P2 analyse ensuite la dernière mesure publiée par P1. */
    arreter_processus(&pid_p2, "P2 - Analyse");

    /* P4 publie le dernier résultat analysé par P2. */
    arreter_processus(&pid_p4, "P4 - Tableau de bord");

    /* P3 termine après les éventuelles notifications de P2. */
    arreter_processus(&pid_p3, "P3 - Alertes");

    /* 10. Libération des ressources. */
    if (notif_initialise)
        sem_destroy(&shared->notif_p3);

    if (mutex_initialise)
        sem_destroy(&shared->mutex);

    munmap(shared, sizeof(*shared));

    printf("[Superviseur] Ressources liberees. Fin du programme.\n");

    return EXIT_SUCCESS;
}
