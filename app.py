from flask import Flask, request
import mysql.connector
import json

sql = mysql.connector.connect(user="AM80", password="@Ali2001", host="127.0.0.7",
                              database="Python")
cursor = sql.cursor()

app = Flask(__name__)


def saved(req_id):
    cursor.execute("SELECT id FROM SERVER;")
    resp = cursor.fetchall()
    queries = list()
    for i in resp:
        queries.append(i[0])
    if req_id in queries:
        return True
    else:
        return False


def update(iD, nAME, aUTH):
    cursor.execute(f"UPDATE SERVER SET NAME = '{nAME}', Auth = '{aUTH}' WHERE id = '{iD}';")
    sql.commit()

def saving(iD, nAME, aUTH):
    cursor.execute(f"INSERT INTO SERVER VALUES ('{nAME}', '{iD}', '{aUTH}')")
    sql.commit()

def deleting(iD):
    cursor.execute(f"DELETE FROM SERVER WHERE id = '{iD}'")
    sql.commit()


@app.route("/normal")
def run():
    ide = request.args.get("id")
    name = request.args.get("name")
    auth = request.args.get("auth")
    cursor.execute(f"INSERT INTO SERVER VALUES('{name}', '{ide}', '{auth}');")
    sql.commit()
    return "Saved to DB"


@app.route("/register")
def check():
    myids = request.args.get("id")
    cursor.execute("SELECT * FROM SERVER;")
    sync = cursor.fetchall()
    names = list()
    ids = list()
    auth = list()
    for i in sync:
        names.append(i[0])
        ids.append(i[1])
        auth.append(i[2])
    response = json.dumps({'Name':names, 'ids':ids, 'auth':auth})
    if myids in ids:
        return "True"
    else:
        return "False"


@app.route("/update")
def upd():
    ide = request.args.get("id")
    name = request.args.get("name")
    auth = request.args.get("auth")
    if saved(ide):
        update(ide, name, auth)
        return "Updated :)"
    else:
        return "ID does not exist"


@app.route("/save")
def sv():
    ide = request.args.get("id")
    name = request.args.get("name")
    auth = request.args.get("auth")
    if not saved(ide):
        saving(ide, name, auth)
        return "Card Saved :)"
    else:
        return "Card exists!!"


@app.route("/delete")
def dlt():
    ide = request.args.get("id")
    if saved(ide):
        deleting(ide)
        return "Card deleted :)"
    else:
        return "Card does not exists!!!"

if __name__ == "__main__":
    app.run(host="0.0.0.0", debug=True)
