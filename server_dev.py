from flask import Flask, render_template

app = Flask(__name__,
            static_url_path="",
            static_folder="data",
            template_folder="data"
            )

@app.route("/", methods=['GET'])
def index():
    return render_template("index.html")

if __name__ == '__main__':
    app.run(host="0.0.0.0", debug=True)
    