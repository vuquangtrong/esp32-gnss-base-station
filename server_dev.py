from flask import Flask, render_template, request
from random import randint

newline = "\n"

app = Flask(__name__,
            static_url_path="",
            static_folder="data",
            template_folder="data"
            )

@app.route("/", methods=['GET'])
def index():
    return render_template("index.html")

@app.route("/status", methods=['GET', 'POST'])
def status():
    if request.method == 'GET':
        return \
            (["Started", "Stopped", "Connected", "Disconnected", "192.168.5.249"])[randint(0, 4)]
    elif request.method == 'POST':
        print(request.data)
        return "OK"

@app.route("/config", methods=['GET', 'POST'])
def config():
    if request.method == 'GET':
        return \
            (["", "abcdefgh"])[randint(0, 1)] +  newline + \
            (["", "12345678"])[randint(0, 1)]
            
    elif request.method == 'POST':
        print(request.data)
        return "OK"
    
if __name__ == '__main__':
    app.run(host="0.0.0.0", debug=True)
    