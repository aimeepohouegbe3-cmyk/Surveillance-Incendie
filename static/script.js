
// ==========================================
// VARIABLES GENERALES
// ==========================================

let requeteEnCours = false;
let graphique = null;
let derniereSequenceGraphique = null;

const historique = {
    labels: [],
    temperatures: [],
    fumees: []
};

const NOMBRE_MAX_MESURES = 20;


// ==========================================
// GESTION DE LA CONNEXION
// ==========================================

function actualiserConnexion(connecte) {

    const indicateur = document.getElementById("connection-indicator");
    const texte = document.getElementById("connection-text");

    if (connecte) {
        indicateur.classList.remove("offline");
        indicateur.classList.add("online");
        texte.textContent = "Connecté au système";
    } else {
        indicateur.classList.remove("online");
        indicateur.classList.add("offline");
        texte.textContent = "Connexion indisponible";
    }
}


// ==========================================
// DATE DE LA DERNIERE ACTUALISATION
// ==========================================

function actualiserDate() {

    const element = document.getElementById("last-update");

    const maintenant = new Date();

    element.textContent = maintenant.toLocaleString("fr-FR", {
        dateStyle: "short",
        timeStyle: "medium"
    });
}


// ==========================================
// INITIALISATION DU GRAPHIQUE
// ==========================================

function initialiserGraphique() {

    const canvas = document.getElementById("history-chart");

    if (!canvas || typeof Chart === "undefined") {
        console.warn("Chart.js ou le canvas est indisponible.");
        return;
    }

    graphique = new Chart(canvas, {
        type: "line",

        data: {
            labels: historique.labels,

            datasets: [
                {
                    label: "Température (°C)",
                    data: historique.temperatures,
                    borderColor: "#ef4444",
                    backgroundColor: "rgba(239, 68, 68, 0.10)",
                    borderWidth: 2,
                    tension: 0.3,
                    fill: false,
                    yAxisID: "y"
                },
                {
                    label: "Fumée / Gaz",
                    data: historique.fumees,
                    borderColor: "#2563eb",
                    backgroundColor: "rgba(37, 99, 235, 0.10)",
                    borderWidth: 2,
                    tension: 0.3,
                    fill: false,
                    yAxisID: "y1"
                }
            ]
        },

        options: {
            responsive: true,
            maintainAspectRatio: false,

            animation: {
                duration: 300
            },

            interaction: {
                intersect: false,
                mode: "index"
            },

            plugins: {
                legend: {
                    display: true,
                    position: "top"
                }
            },

            scales: {
                x: {
                    title: {
                        display: true,
                        text: "Mesures"
                    }
                },

                y: {
                    type: "linear",
                    position: "left",
                    beginAtZero: true,
                    title: {
                        display: true,
                        text: "Température (°C)"
                    }
                },

                y1: {
                    type: "linear",
                    position: "right",
                    beginAtZero: true,
                    title: {
                        display: true,
                        text: "Niveau de fumée"
                    },
                    grid: {
                        drawOnChartArea: false
                    }
                }
            }
        }
    });
}


// ==========================================
// AJOUT D'UNE MESURE AU GRAPHIQUE
// ==========================================

function ajouterMesureGraphique(donnees, dhtDisponible, mq2Disponible) {

    if (!graphique) return;

    // Eviter d'ajouter plusieurs fois la même mesure
    if (donnees.sequence === derniereSequenceGraphique) {
        return;
    }

    derniereSequenceGraphique = donnees.sequence;

    historique.labels.push("N° " + donnees.sequence);

    historique.temperatures.push(
        dhtDisponible ? donnees.temperature : null
    );

    historique.fumees.push(
        mq2Disponible ? donnees.fumee : null
    );

    // Garder uniquement les 20 dernières mesures
    if (historique.labels.length > NOMBRE_MAX_MESURES) {
        historique.labels.shift();
        historique.temperatures.shift();
        historique.fumees.shift();
    }

    graphique.update();
}


// ==========================================
// ACTUALISATION DES CAPTEURS
// ==========================================

async function actualiserCapteurs() {

    if (requeteEnCours) return;

    requeteEnCours = true;

    const alerte = document.getElementById("alert-message");
    const statut = document.getElementById("system-status");

    try {

        const reponse = await fetch("/api/capteurs", {
            cache: "no-store"
        });

        if (!reponse.ok) {
            throw new Error("Erreur HTTP : " + reponse.status);
        }

        const donnees = await reponse.json();

        if (donnees.erreur) {
            throw new Error(donnees.erreur);
        }

        // La réception des données a réussi
        actualiserConnexion(true);
        actualiserDate();


        // ==================================
        // DISPONIBILITE DES CAPTEURS
        // ==================================

        const capteurs = Number(donnees.sensor_status) || 0;

        const dhtDisponible = (capteurs & 1) !== 0;
        const mq2Disponible = (capteurs & 2) !== 0;
        const flammeDisponible = (capteurs & 4) !== 0;


        // ==================================
        // TEMPERATURE
        // ==================================

        const temperature = document.getElementById("temperature");
        const tempStatus = document.getElementById("temperature-status");

        if (
            dhtDisponible &&
            Number.isFinite(Number(donnees.temperature))
        ) {

            temperature.textContent =
                Number(donnees.temperature).toFixed(1) + " °C";

            if (donnees.temperature >= 45) {
                tempStatus.textContent = "Température élevée";
                tempStatus.style.color = "red";
            } else {
                tempStatus.textContent = "Température normale";
                tempStatus.style.color = "green";
            }

        } else {

            temperature.textContent = "—";
            tempStatus.textContent = "Capteur indisponible";
            tempStatus.style.color = "red";
        }


        // ==================================
        // HUMIDITE
        // ==================================

        const humidity = document.getElementById("humidity");
        const humidityStatus = document.getElementById("humidity-status");

        if (
            dhtDisponible &&
            Number.isFinite(Number(donnees.humidite))
        ) {

            humidity.textContent = donnees.humidite + " %";

            if (donnees.humidite >= 0 && donnees.humidite <= 100) {
                humidityStatus.textContent = "Mesure disponible";
                humidityStatus.style.color = "green";
            } else {
                humidityStatus.textContent = "Mesure invalide";
                humidityStatus.style.color = "red";
            }

        } else {

            humidity.textContent = "—";
            humidityStatus.textContent = "Capteur indisponible";
            humidityStatus.style.color = "red";
        }


        // ==================================
        // FUMEE / GAZ
        // ==================================

        const smoke = document.getElementById("smoke");
        const smokeStatus = document.getElementById("smoke-status");

        if (
            mq2Disponible &&
            Number.isFinite(Number(donnees.fumee))
        ) {

            smoke.textContent = donnees.fumee;

            if (donnees.fumee >= 400) {
                smokeStatus.textContent = "Niveau élevé";
                smokeStatus.style.color = "red";
            } else {
                smokeStatus.textContent = "Niveau normal";
                smokeStatus.style.color = "green";
            }

        } else {

            smoke.textContent = "—";
            smokeStatus.textContent = "Capteur indisponible";
            smokeStatus.style.color = "red";
        }


        // ==================================
        // FLAMME
        // ==================================

        const flame = document.getElementById("flame");
        const flameStatus = document.getElementById("flame-status");

        if (flammeDisponible) {

            flame.textContent = donnees.flamme ? "Détectée !" : "Aucune";

            if (donnees.flamme) {
                flameStatus.textContent = "Flamme détectée";
                flameStatus.style.color = "red";
            } else {
                flameStatus.textContent = "Aucune flamme";
                flameStatus.style.color = "green";
            }

        } else {

            flame.textContent = "—";
            flameStatus.textContent = "Capteur indisponible";
            flameStatus.style.color = "red";
        }


        // ==================================
        // MISE A JOUR DU GRAPHIQUE
        // ==================================

        ajouterMesureGraphique(
            donnees,
            dhtDisponible,
            mq2Disponible
        );


        // ==================================
        // ETAT GENERAL ET ALERTES
        // ==================================

        switch (donnees.etat_nom) {

            case "NORMAL":

                alerte.textContent = "Aucune alerte détectée.";
                alerte.style.color = "green";

                statut.textContent = "Système opérationnel";
                statut.style.color = "green";

                break;


            case "ATTENTION":

                alerte.textContent =
                    "Attention : situation à surveiller !";

                alerte.style.color = "orange";

                statut.textContent = "ATTENTION";
                statut.style.color = "orange";

                break;


            case "ALERTE":

                alerte.textContent =
                    "ALERTE : risque d'incendie détecté !";

                alerte.style.color = "red";

                statut.textContent = "ALERTE INCENDIE";
                statut.style.color = "red";

                break;


            case "DEFAUT CAPTEUR":

                alerte.textContent =
                    "Défaut détecté : vérifiez les capteurs !";

                alerte.style.color = "red";

                statut.textContent = "DÉFAUT CAPTEUR";
                statut.style.color = "red";

                break;


            default:

                alerte.textContent = "État du système inconnu.";
                alerte.style.color = "gray";

                statut.textContent = "ÉTAT INCONNU";
                statut.style.color = "gray";
        }

    } catch (erreur) {

        console.error("Erreur de récupération :", erreur);

        actualiserConnexion(false);

        alerte.textContent =
            "Impossible de récupérer les données.";

        alerte.style.color = "red";

        statut.textContent = "CONNEXION INDISPONIBLE";
        statut.style.color = "red";

    } finally {

        requeteEnCours = false;
    }
}


// ==========================================
// DEMARRAGE DU TABLEAU DE BORD
// ==========================================

initialiserGraphique();

actualiserCapteurs();

setInterval(actualiserCapteurs, 2000);

