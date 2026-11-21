/* analysis.c — P2 : lit les mesures, les vérifie, calcule et publie l'état. */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "analysis.h"

#define PERIODE_NS 500000000L   /* P2 vérifie toutes les 0,5 s */

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

/* Écart en secondes entre deux instants */
static double ecart_s(const struct timespec *debut, const struct timespec *fin)
{
    return (double)(fin->tv_sec - debut->tv_sec)
         + (double)(fin->tv_nsec - debut->tv_nsec) / 1e9;
}

/* Est-ce que la mesure est trop vieille (ou absente) ? */
static int est_perimee(const struct mesures *m, const struct timespec *maintenant)
{
    if (m->sequence == 0)
        return 1;                              /* P1 n'a encore rien publié */
    return ecart_s(&m->timestamp, maintenant) > DELAI_PERIME_S;
}

int calculer_etat(const struct mesures *m, const struct timespec *maintenant)
{
    /* 1. Validité : un capteur signalé en erreur par P1 */
    if ((m->sensor_status & CAPTEURS_TOUS_OK) != CAPTEURS_TOUS_OK)
        return ETAT_DEFAUT;

    /* 2. Fraîcheur : donnée absente ou trop ancienne */
    if (est_perimee(m, maintenant))
        return ETAT_DEFAUT;

    /* 3. Plages physiques (DHT22 : -40..80 °C, 0..100 %) */
    if (m->temperature < -40.0f || m->temperature > 80.0f)
        return ETAT_DEFAUT;
    if (m->humidity < 0.0f || m->humidity > 100.0f)
        return ETAT_DEFAUT;
    if (m->smoke_value < 0 || m->smoke_value > FUMEE_MAX)
        return ETAT_DEFAUT;

    /* 4. Règles : on compte les indicateurs anormaux */
    int nb = 0;
    if (m->temperature >= TEMP_ATTENTION) nb++;
    if (m->smoke_value >= FUMEE_ATTENTION) nb++;
    if (m->flame_detected)                 nb++;

    if (nb == 0) return ETAT_NORMAL;
    if (nb == 1) return ETAT_ATTENTION;
    return ETAT_ALERTE;                        /* plusieurs indices concordent */
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
        f->candidat = etat_brut;               /* rien ne change, on oublie le candidat */
        f->compteur = 0;
        return f->etat_confirme;
    }

    if (etat_brut == f->candidat) {
        f->compteur++;
    } else {
        f->candidat = etat_brut;               /* nouveau candidat */
        f->compteur = 1;
    }

    if (f->compteur >= NB_CONFIRMATION) {
        f->etat_confirme = f->candidat;        /* confirmé ! */
        f->compteur = 0;
    }
    return f->etat_confirme;
}

/* Publie l'état dans la mémoire partagée et prévient P3 s'il a changé */
static void publier_etat(struct shared_data *shm, int etat, int ancien)
{
    while (sem_wait(&shm->mutex) == -1 && errno == EINTR)
        if (arret) return;
    shm->system_state = etat;
    sem_post(&shm->mutex);

    if (etat != ancien) {
        printf("[P2] Changement d'etat : %s -> %s\n", nom_etat(ancien), nom_etat(etat));
        fflush(stdout);
        sem_post(&shm->notif_p3);
    }
}

void processus_analyse(struct shared_data *shm)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = gestion_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    struct filtre_etat filtre;
    filtre_init(&filtre);
    unsigned long derniere_seq = 0;
    int etat_publie = ETAT_DEFAUT;
    publier_etat(shm, etat_publie, etat_publie);

    printf("[P2] Analyse demarree (PID %d)\n", (int)getpid());
    fflush(stdout);

    while (!arret) {
        /* 1. Copie locale sous sémaphore, section critique la plus courte possible */
        if (sem_wait(&shm->mutex) == -1) {
            if (errno == EINTR) continue;      /* interrompu par un signal */
            perror("[P2] sem_wait");
            break;
        }
        struct mesures m = shm->mesures;
        sem_post(&shm->mutex);

        struct timespec maintenant;
        clock_gettime(CLOCK_MONOTONIC, &maintenant);

        /* 2. Nouvelle mesure -> on l'analyse.
         *    Pas de nouvelle mesure mais donnée périmée -> P1 est peut-être bloqué. */
        int nouvel_etat = etat_publie;
        if (m.sequence != derniere_seq) {
            derniere_seq = m.sequence;
            nouvel_etat = filtre_mettre_a_jour(&filtre, calculer_etat(&m, &maintenant));
        } else if (est_perimee(&m, &maintenant)) {
            nouvel_etat = filtre_mettre_a_jour(&filtre, ETAT_DEFAUT);
        }

        /* 3. Publication */
        if (nouvel_etat != etat_publie) {
            publier_etat(shm, nouvel_etat, etat_publie);
            etat_publie = nouvel_etat;
        }

        /* 4. Attente du prochain cycle */
        struct timespec pause = { 0, PERIODE_NS };
        nanosleep(&pause, NULL);
    }

    printf("[P2] Arret demande, fin de l'analyse\n");
    fflush(stdout);
}
