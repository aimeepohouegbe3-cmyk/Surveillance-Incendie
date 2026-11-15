/* alert.c — processus P3 : affichage console, signalisation et journalisation. */
#include "alert.h"
#include "analysis.h" /* Réutilise nom_etat() défini par P2 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Codes couleurs ANSI pour la signalisation visuelle en console */
#define COULEUR_RESET   "\033[0m"
#define COULEUR_VERT    "\033[1;32m"
#define COULEUR_JAUNE   "\033[1;33m"
#define COULEUR_ROUGE   "\033[1;31m"
#define COULEUR_MAGENTA "\033[1;35m"

/* Drapeau d'arrêt propre sur réception de SIGINT ou SIGTERM */
static volatile sig_atomic_t arret_demande = 0;

static void gestionnaire_arret(int sig) {
    (void)sig;
    arret_demande = 1;
}

/* Formate la date et l'heure courantes (AAAA-MM-JJ HH:MM:SS) */
static void formater_horodatage(char *buf, size_t taille) {
    time_t maintenant = time(NULL);
    struct tm tm_info;
    if (localtime_r(&maintenant, &tm_info) != NULL) {
        strftime(buf, taille, "%Y-%m-%d %H:%M:%S", &tm_info);
    } else {
        snprintf(buf, taille, "0000-00-00 00:00:00");
    }
}

/* Écrit l'événement dans le fichier journal */
static void ecrire_journal(const char *chemin_log, const char *horodatage,
                           int etat, const struct mesures *m) {
    FILE *f = fopen(chemin_log, "a");
    if (f == NULL) {
        perror("[P3] Erreur ouverture fichier journal");
        return;
    }

    fprintf(f, "[%s] SEQ=%lu | ETAT=%-14s | Temp=%.1fC | Hum=%.1f%% | Fumee=%d | Flamme=%d | Status=0x%X\n",
            horodatage,
            m->sequence,
            nom_etat(etat),
            m->temperature,
            m->humidity,
            m->smoke_value,
            m->flame_detected,
            m->sensor_status);

    fclose(f);
}

/* Affiche l'état et active la signalisation de démonstration */
static void afficher_signalisation(const char *horodatage, int etat, const struct mesures *m) {
    switch (etat) {
        case ETAT_NORMAL:
            printf("%s[%s] [LED VERTE] %s (SEQ=%lu | Temp=%.1fC | Hum=%.1f%% | Fumee=%d)%s\n",
                   COULEUR_VERT, horodatage, nom_etat(etat),
                   m->sequence, m->temperature, m->humidity, m->smoke_value, COULEUR_RESET);
            break;

        case ETAT_ATTENTION:
            printf("%s[%s] [AVERTISSEMENT] %s (SEQ=%lu | Temp=%.1fC | Fumee=%d | Flamme=%d)%s\n",
                   COULEUR_JAUNE, horodatage, nom_etat(etat),
                   m->sequence, m->temperature, m->smoke_value, m->flame_detected, COULEUR_RESET);
            break;

        case ETAT_ALERTE:
            printf("\a%s[%s] [ALARME SONORE + LED ROUGE] %s ! (SEQ=%lu | Temp=%.1fC | Fumee=%d | Flamme=%d)%s\n",
                   COULEUR_ROUGE, horodatage, nom_etat(etat),
                   m->sequence, m->temperature, m->smoke_value, m->flame_detected, COULEUR_RESET);
            break;

        case ETAT_DEFAUT:
        default:
            printf("%s[%s] [SIGNAL DEFAUT] %s (SEQ=%lu | Status=0x%X)%s\n",
                   COULEUR_MAGENTA, horodatage, nom_etat(etat),
                   m->sequence, m->sensor_status, COULEUR_RESET);
            break;
    }
    fflush(stdout);
}

void processus_alerte(struct shared_data *shm, const char *fichier_log) {
    if (shm == NULL) {
        fprintf(stderr, "[P3] Erreur : pointeur shared_data NULL\n");
        return;
    }

    const char *chemin_log = (fichier_log != NULL) ? fichier_log : "journal_incendie.log";

    /* Installation des gestionnaires d'arrêt propre */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = gestionnaire_arret;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("[P3 - Alerte] Demarre. Journalisation dans '%s'. En attente de notifications...\n", chemin_log);

    while (!arret_demande) {
        /* 1. Attente bloquante d'une notification de changement d'état envoyée par P2 */
        if (sem_wait(&shm->notif_p3) == -1) {
            if (errno == EINTR && arret_demande) {
                break;
            }
            perror("[P3] Erreur sem_wait(notif_p3)");
            break;
        }

        if (arret_demande) {
            break;
        }

        /* 2. Copie locale cohérente protégée par le sémaphore mutex */
        if (sem_wait(&shm->mutex) == -1) {
            perror("[P3] Erreur sem_wait(mutex)");
            break;
        }
        struct mesures copie_mesures = shm->mesures;
        int etat_courant = shm->system_state;
        sem_post(&shm->mutex);

        /* 3. Signalisation visuelle/sonore et écriture dans le fichier journal */
        char horodatage[32];
        formater_horodatage(horodatage, sizeof(horodatage));

        afficher_signalisation(horodatage, etat_courant, &copie_mesures);
        ecrire_journal(chemin_log, horodatage, etat_courant, &copie_mesures);
    }

    printf("[P3 - Alerte] Arret propre du processus.\n");
}
