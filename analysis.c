
/* analysis.c — P2 : analyse les mesures et publie l'état */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "analysis.h"

#define PERIODE_NS 500000000L

static volatile sig_atomic_t arret = 0;

static void gestion_signal(int sig)
{
    (void)sig;
    arret = 1;
}

const char *nom_etat(int etat)
{
    switch (etat) {
        case ETAT_NORMAL:    return "NORMAL";
        case ETAT_ATTENTION: return "ATTENTION";
        case ETAT_ALERTE:    return "ALERTE";
        case ETAT_DEFAUT:    return "DEFAUT CAPTEUR";
        default:             return "INCONNU";
    }
}

static double ecart_s(const struct timespec *debut,
                      const struct timespec *fin)
{
    return (double)(fin->tv_sec - debut->tv_sec)
         + (double)(fin->tv_nsec - debut->tv_nsec) / 1e9;
}

static int est_perimee(const struct mesures *m,
                       const struct timespec *maintenant)
{
    if (m->sequence == 0)
        return 1;

    return ecart_s(&m->timestamp, maintenant) > DELAI_PERIME_S;
}

int calculer_etat(const struct mesures *m,
                  const struct timespec *maintenant)
{
    if ((m->sensor_status & CAPTEURS_TOUS_OK) != CAPTEURS_TOUS_OK)
        return ETAT_DEFAUT;

    if (est_perimee(m, maintenant))
        return ETAT_DEFAUT;

    if (m->temperature < -40.0f || m->temperature > 80.0f)
        return ETAT_DEFAUT;

    if (m->humidity < 0.0f || m->humidity > 100.0f)
        return ETAT_DEFAUT;

    if (m->smoke_value < 0 || m->smoke_value > FUMEE_MAX)
        return ETAT_DEFAUT;

    int nb = 0;

    if (m->temperature >= TEMP_ATTENTION)
        nb++;

    if (m->smoke_value >= FUMEE_ATTENTION)
        nb++;

    if (m->flame_detected)
        nb++;

    if (nb == 0)
        return ETAT_NORMAL;

    if (nb == 1)
        return ETAT_ATTENTION;

    return ETAT_ALERTE;
}

void filtre_init(struct filtre_etat *f)
{
    f->etat_confirme = ETAT_DEFAUT;
    f->candidat = ETAT_DEFAUT;
    f->compteur = 0;
}

int filtre_mettre_a_jour(struct filtre_etat *f, int etat_brut)
{
    if (etat_brut == f->etat_confirme) {
        f->candidat = etat_brut;
        f->compteur = 0;
        return f->etat_confirme;
    }

    if (etat_brut == f->candidat) {
        f->compteur++;
    } else {
        f->candidat = etat_brut;
        f->compteur = 1;
    }

    if (f->compteur >= NB_CONFIRMATION) {
        f->etat_confirme = f->candidat;
        f->compteur = 0;
    }

    return f->etat_confirme;
}

/* Publication de l'état et de la séquence analysée. */
static void publier_resultat(struct shared_data *shm,
                             int etat,
                             int ancien,
                             unsigned long sequence)
{
    while (sem_wait(&shm->mutex) == -1) {
        if (errno == EINTR)
            continue;

        perror("[P2] Erreur sem_wait");
        return;
    }

    shm->system_state = etat;
    shm->analysed_sequence = sequence;

    if (sem_post(&shm->mutex) == -1) {
        perror("[P2] Erreur sem_post");
        return;
    }

    if (etat != ancien) {
        printf("[P2] Changement d'etat : %s -> %s\n",
               nom_etat(ancien),
               nom_etat(etat));

        fflush(stdout);

        if (sem_post(&shm->notif_p3) == -1)
            perror("[P2] Erreur notification P3");
    }
}

void processus_analyse(struct shared_data *shm)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = gestion_signal;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGTERM, &sa, NULL);

    struct filtre_etat filtre;
    filtre_init(&filtre);

    unsigned long derniere_seq = 0;
    int etat_publie = ETAT_DEFAUT;

    publier_resultat(shm, etat_publie, etat_publie, 0);

    printf("[P2] Analyse demarree (PID %d)\n", (int)getpid());
    fflush(stdout);

    /*
     * La boucle continue même après SIGTERM,
     * le temps de traiter la dernière mesure de P1.
     */
    while (1)
    {
        /* 1. Lecture protégée de la mémoire partagée. */
        while (sem_wait(&shm->mutex) == -1) {
            if (errno == EINTR)
                continue;

            perror("[P2] sem_wait");
            goto fin;
        }

        struct mesures m = shm->mesures;

        if (sem_post(&shm->mutex) == -1) {
            perror("[P2] sem_post");
            goto fin;
        }

        /* 2. Heure actuelle. */
        struct timespec maintenant;

        if (clock_gettime(CLOCK_MONOTONIC, &maintenant) == -1) {
            perror("[P2] clock_gettime");
            goto fin;
        }

        /* 3. Traitement d'une nouvelle mesure. */
        if (m.sequence != 0 && m.sequence != derniere_seq)
        {
            int etat_brut = calculer_etat(&m, &maintenant);

            int nouvel_etat =
                filtre_mettre_a_jour(&filtre, etat_brut);

            publier_resultat(
                shm,
                nouvel_etat,
                etat_publie,
                m.sequence
            );

            derniere_seq = m.sequence;
            etat_publie = nouvel_etat;
        }

        /* 4. Vérification d'une mesure devenue périmée. */
        else if (m.sequence != 0 &&
                 est_perimee(&m, &maintenant))
        {
            int nouvel_etat =
                filtre_mettre_a_jour(&filtre, ETAT_DEFAUT);

            if (nouvel_etat != etat_publie)
            {
                publier_resultat(
                    shm,
                    nouvel_etat,
                    etat_publie,
                    m.sequence
                );

                etat_publie = nouvel_etat;
            }
        }

        /*
         * 5. Si l'arrêt est demandé et que la dernière
         * séquence est analysée, P2 peut terminer.
         */
        if (arret && m.sequence == derniere_seq)
            break;

        /* 6. Attente normale uniquement hors arrêt. */
        if (!arret)
        {
            struct timespec pause = {0, PERIODE_NS};
            nanosleep(&pause, NULL);
        }
    }

fin:
    printf("[P2] Derniere mesure analysee : %lu\n", derniere_seq);
    printf("[P2] Arret propre du processus d'analyse.\n");
    fflush(stdout);
}
