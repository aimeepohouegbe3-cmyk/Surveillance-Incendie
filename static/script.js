
async function actualiserCapteurs() {
    try {
        const reponse = await fetch("/api/capteurs");
        const donnees = await reponse.json();

        document.getElementById("temperature").textContent =
            donnees.temperature + " °C";

        document.getElementById("humidity").textContent =
            donnees.humidite + " %";

        document.getElementById("smoke").textContent =
            donnees.fumee;

        document.getElementById("flame").textContent =
            donnees.flamme ? "Détectée !" : "Aucune";

        const alerte = document.getElementById("alert-message");
        const statut = document.getElementById("system-status");

        if (donnees.flamme || donnees.fumee > 300 ||
            donnees.temperature > 40) {

            alerte.textContent = "⚠️ ALERTE : Risque détecté !";
            alerte.style.color = "red";

            statut.textContent = "ALERTE INCENDIE";
            statut.style.color = "red";

        } else {
            alerte.textContent = "Aucune alerte détectée.";
            alerte.style.color = "green";

            statut.textContent = "Système opérationnel";
            statut.style.color = "green";
        }

    } catch (erreur) {
        console.error("Erreur de récupération :", erreur);
    }
}

actualiserCapteurs();

setInterval(actualiserCapteurs, 2000);
