
from flask import Flask, render_template, jsonify
import random

app = Flask(__name__)

@app.route("/")
def accueil():
    return render_template("index.html")

@app.route("/api/capteurs")
def donnees_capteurs():

    temperature = round(random.uniform(20, 45), 1)
    humidite = round(random.uniform(30, 80), 1)
    fumee = random.randint(50, 500)
    flamme = random.choice([True, False])

    return jsonify({
        "temperature": temperature,
        "humidite": humidite,
        "fumee": fumee,
        "flamme": flamme
    })

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)

