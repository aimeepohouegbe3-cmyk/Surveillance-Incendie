
from flask import Flask, render_template, jsonify
from pathlib import Path
import json

app = Flask(__name__)

# Chemin du fichier JSON généré par le processus P4
FICHIER_DONNEES = Path(__file__).resolve().parent / "dashboard_data.json"


@app.route("/")
def accueil():
    return render_template("index.html")


@app.route("/api/capteurs")
def donnees_capteurs():

    try:
        # Lecture du fichier JSON
        with FICHIER_DONNEES.open("r", encoding="utf-8") as fichier:
            donnees = json.load(fichier)

        # Transmission des données au format JSON
        return jsonify(donnees)

    except FileNotFoundError:
        return jsonify({
            "erreur": "Le fichier dashboard_data.json est introuvable."
        }), 404

    except json.JSONDecodeError:
        return jsonify({
            "erreur": "Le fichier dashboard_data.json contient un JSON invalide."
        }), 500

    except OSError as erreur:
        return jsonify({
            "erreur": f"Erreur de lecture du fichier : {erreur}"
        }), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
