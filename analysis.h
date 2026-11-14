/* analysis.h — interface du processus P2 (analyse) */
#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "shared_data.h"

/* Filtre : un nouvel état n'est confirmé qu'après NB_CONFIRMATION résultats identiques */
struct filtre_etat {
    int etat_confirme;  /* état publié */
    int candidat;       /* état en cours de confirmation */
    int compteur;       /* nombre de fois consécutives où le candidat a été vu */
};

/* Calcule l'état "brut" d'une mesure (sans confirmation). Fonction pure, testable seule. */
int calculer_etat(const struct mesures *m, const struct timespec *maintenant);

/* Initialise le filtre. On démarre en DÉFAUT : pas de donnée = pas "normal". */
void filtre_init(struct filtre_etat *f);

/* Donne un état brut au filtre, renvoie l'état confirmé. */
int filtre_mettre_a_jour(struct filtre_etat *f, int etat_brut);

/* Boucle principale de P2. Revient quand SIGTERM ou SIGINT est reçu. */
void processus_analyse(struct shared_data *shm);

/* Nom lisible d'un état, pour l'affichage */
const char *nom_etat(int etat);

#endif
