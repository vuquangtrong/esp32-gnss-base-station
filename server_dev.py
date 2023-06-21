from flask import Flask, render_template, request
from random import randint, uniform

newline = "\n"

app = Flask(__name__,
            static_url_path="",
            static_folder="data",
            template_folder="data"
            )

@app.route("/", methods=['GET'])
def index():
    return render_template("index.html")

@app.route("/status", methods=['GET'])
def status():
    return \
        "$GNGGA,072446.00," + \
            str(uniform(0, 9000)) + "," + (['N', 'S'])[randint(0, 1)] + "," + \
            str(uniform(0, 18000)) + "," + (['E', 'W'])[randint(0, 1)] + "," + \
            str(randint(0, 9)) + "," + \
            "27" + "," + "0.5" + "," + \
            str(uniform(-10, 10)) + ",M," + \
            str(uniform(-10, 10)) + ",M," + \
            "2.0,*44" + newline + \
        (["Rover", "Base-Survey", "Base-Fixed"])[randint(0, 2)] + newline + \
        (["Started", "Stopped", "Connected", "Disconnected", "192.168.5.249"])[randint(0, 4)] + newline + \
        ""

@app.route("/config", methods=['GET'])
def config():
    return \
        "hostname" + newline + \
        "version" + newline + \
        (["", "abcdefgh"])[randint(0, 1)] +  newline + \
        (["", "12345678"])[randint(0, 1)]
            
@app.route("/action", methods=['POST'])
def action():
    print(request.data)
    return "OK"

if __name__ == '__main__':
    app.run(host="0.0.0.0", debug=True)
