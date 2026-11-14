/* test_analysis.c — teste les règles de P2 avec des valeurs inventées.
 * Pas de fork ni de mémoire partagée : on appelle directement les fonctions.
 * Compilation : gcc -Wall -Wextra -pthread -o test_analysis test_analysis.c analysis.c
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include "analysis.h"

static int reussis = 0, echoues = 0;

static void verifier(const char *nom, int obtenu, int attendu)
{
    if (obtenu == attendu) {
        printf("  OK     %-40s -> %s\n", nom, nom_etat(obtenu));
        reussis++;
    } else {
        printf("  ECHEC  %-40s -> %s (attendu %s)\n", nom, nom_etat(obtenu), nom_etat(attendu));
        echoues++;
    }
}

/* Une mesure "normale" de base, horodatée à l'instant t */
static struct mesures mesure_normale(struct timespec t)
{
    struct mesures m = {
        .sequence = 1, .timestamp = t,
        .temperature = 22.0f, .humidity = 45.0f,
        .smoke_value = 120, .flame_detected = 0,
        .sensor_status = CAPTEURS_TOUS_OK
    };
    return m;
}

int main(void)
{
    struct timespec t = { 1000, 0 };          /* instant de la mesure */
    struct timespec maintenant = { 1001, 0 }; /* 1 s plus tard */
    struct mesures m;

    printf("Regles d'etat (calculer_etat)\n");

    m = mesure_normale(t);
    verifier("Valeurs normales", calculer_etat(&m, &maintenant), ETAT_NORMAL);

    m = mesure_normale(t); m.temperature = 50.0f;
    verifier("Temperature haute seule", calculer_etat(&m, &maintenant), ETAT_ATTENTION);

    m = mesure_normale(t); m.smoke_value = 600;
    verifier("Fumee seule", calculer_etat(&m, &maintenant), ETAT_ATTENTION);

    m = mesure_normale(t); m.flame_detected = 1;
    verifier("Flamme seule", calculer_etat(&m, &maintenant), ETAT_ATTENTION);

    m = mesure_normale(t); m.temperature = 50.0f; m.smoke_value = 600;
    verifier("Temperature + fumee", calculer_etat(&m, &maintenant), ETAT_ALERTE);

    m = mesure_normale(t); m.smoke_value = 600; m.flame_detected = 1;
    verifier("Fumee + flamme", calculer_etat(&m, &maintenant), ETAT_ALERTE);

    m = mesure_normale(t); m.sensor_status = CAPTEUR_TEMP_OK | CAPTEUR_FLAMME_OK;
    verifier("Capteur de fumee en erreur", calculer_etat(&m, &maintenant), ETAT_DEFAUT);

    m = mesure_normale(t); m.temperature = 150.0f;
    verifier("Temperature hors plage", calculer_etat(&m, &maintenant), ETAT_DEFAUT);

    m = mesure_normale(t); m.humidity = -5.0f;
    verifier("Humidite hors plage", calculer_etat(&m, &maintenant), ETAT_DEFAUT);

    m = mesure_normale(t); m.sequence = 0;
    verifier("Aucune mesure publiee", calculer_etat(&m, &maintenant), ETAT_DEFAUT);

    struct timespec tard = { 1010, 0 };       /* 10 s apres la mesure */
    m = mesure_normale(t);
    verifier("Donnee perimee (10 s)", calculer_etat(&m, &tard), ETAT_DEFAUT);

    printf("\nConfirmation (filtre, %d mesures identiques)\n", NB_CONFIRMATION);
    struct filtre_etat f;
    filtre_init(&f);
    verifier("Demarrage", f.etat_confirme, ETAT_DEFAUT);
    filtre_mettre_a_jour(&f, ETAT_NORMAL);
    filtre_mettre_a_jour(&f, ETAT_NORMAL);
    verifier("2 mesures normales : pas encore", f.etat_confirme, ETAT_DEFAUT);
    verifier("3e mesure normale : confirme", filtre_mettre_a_jour(&f, ETAT_NORMAL), ETAT_NORMAL);
    verifier("1 alerte isolee : ignoree", filtre_mettre_a_jour(&f, ETAT_ALERTE), ETAT_NORMAL);
    verifier("Retour normal", filtre_mettre_a_jour(&f, ETAT_NORMAL), ETAT_NORMAL);
    filtre_mettre_a_jour(&f, ETAT_ALERTE);
    filtre_mettre_a_jour(&f, ETAT_ALERTE);
    verifier("3 alertes de suite : confirme", filtre_mettre_a_jour(&f, ETAT_ALERTE), ETAT_ALERTE);

    printf("\nResultat : %d reussis, %d echoues\n", reussis, echoues);
    return echoues == 0 ? 0 : 1;
}
