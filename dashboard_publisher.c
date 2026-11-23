
/* dashboard_publisher.c — P4 : publication des données JSON */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include "shared_data.h"

#define FICHIER_JSON "dashboard_data.json"
#define FICHIER_TEMP "dashboard_data.tmp"

static volatile sig_atomic_t arret_p4 = 0;

static void gestion_signal_p4(int sig)
{
    (void)sig;
    arret_p4 = 1;
}

static const char *nom_etat(int etat)
{
    switch (etat) {
        case ETAT_NORMAL:    return "NORMAL";
        case ETAT_ATTENTION: return "ATTENTION";
        case ETAT_ALERTE:    return "ALERTE";
        case ETAT_DEFAUT:    return "DEFAUT CAPTEUR";
        default:             return "INCONNU";
    }
}

void dashboard_publisher(struct shared_data *shared)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = gestion_signal_p4;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGTERM, &sa, NULL);

    unsigned long derniere_sequence = 0;
    int dernier_etat = -1;

    while (1)
    {
        struct mesures m;
        int etat;
        unsigned long sequence_analysee;

        /* 1. Lecture protégée. */
        while (sem_wait(&shared->mutex) == -1)
        {
            if (errno == EINTR)
                continue;

            perror("[P4] Erreur sem_wait");
            goto fin;
        }

        m = shared->mesures;
        etat = shared->system_state;
        sequence_analysee = shared->analysed_sequence;

        if (sem_post(&shared->mutex) == -1)
        {
            perror("[P4] Erreur sem_post");
            goto fin;
        }

        /* 2. Attendre l'analyse de P2. */
        if (m.sequence == 0 ||
            sequence_analysee != m.sequence)
        {
            if (arret_p4)
            {
                /*
                 * En fonctionnement normal, P2 a déjà terminé
                 * avant que le superviseur ne signale P4.
                 * Cette attente protège néanmoins contre une
                 * lecture momentanément non synchronisée.
                 */
                struct timespec pause = {0, 100000000L};
                nanosleep(&pause, NULL);
            }
            else
            {
                sleep(1);
            }

            continue;
        }

        /* 3. Vérifier si une publication est nécessaire. */
        if (m.sequence != derniere_sequence ||
            etat != dernier_etat)
        {
            FILE *f = fopen(FICHIER_TEMP, "w");

            if (f == NULL)
            {
                perror("[P4] Erreur ouverture fichier temporaire");

                struct timespec pause = {1, 0};
                nanosleep(&pause, NULL);
                continue;
            }

            /* 4. Écriture du JSON. */
            int resultat = fprintf(
                f,
                "{\n"
                "  \"sequence\": %lu,\n"
                "  \"temperature\": %.1f,\n"
                "  \"humidite\": %.1f,\n"
                "  \"fumee\": %d,\n"
                "  \"flamme\": %s,\n"
                "  \"sensor_status\": %u,\n"
                "  \"etat\": %d,\n"
                "  \"etat_nom\": \"%s\"\n"
                "}\n",
                m.sequence,
                m.temperature,
                m.humidity,
                m.smoke_value,
                m.flame_detected ? "true" : "false",
                m.sensor_status,
                etat,
                nom_etat(etat)
            );

            if (resultat < 0)
            {
                perror("[P4] Erreur ecriture JSON");
                fclose(f);
                remove(FICHIER_TEMP);

                struct timespec pause = {1, 0};
                nanosleep(&pause, NULL);
                continue;
            }

            /* 5. Fermeture du fichier. */
            if (fclose(f) == EOF)
            {
                perror("[P4] Erreur fermeture fichier");
                remove(FICHIER_TEMP);

                struct timespec pause = {1, 0};
                nanosleep(&pause, NULL);
                continue;
            }

            /* 6. Remplacement atomique du fichier JSON. */
            if (rename(FICHIER_TEMP, FICHIER_JSON) == -1)
            {
                perror("[P4] Erreur rename");
                remove(FICHIER_TEMP);

                struct timespec pause = {1, 0};
                nanosleep(&pause, NULL);
                continue;
            }

            /* 7. Mémorisation après publication réussie. */
            derniere_sequence = m.sequence;
            dernier_etat = etat;

            printf("[P4] Donnees publiees - sequence %lu - etat %s\n",
                   m.sequence,
                   nom_etat(etat));

            fflush(stdout);
        }

        /*
         * 8. Si P2 a terminé et que son résultat est publié,
         * P4 peut terminer.
         */
        if (arret_p4)
            break;

        sleep(1);
    }

fin:
    remove(FICHIER_TEMP);

    printf("[P4] Derniere publication terminee.\n");
    printf("[P4] Arret du processus de transmission.\n");
    fflush(stdout);
}
