from flask import Flask, request, jsonify

app = Flask(__name__)

# Sample data storage
cart = [
    {"id": 1, "items": 0}
]
carts = []
counter = [
    {"id": 1, "no": 5}
]


# Handle GET requests
@app.route('/cart', methods=['GET'])
def get_data():
    return jsonify(cart), 200


# Handle POST requests
@app.route('/cart', methods=['POST'])
def add_data():
    if not request.is_json:
        return jsonify({"error": "Request body must be JSON"}), 400
    content = request.get_json()
    carts.append(content)
    print(content['items'])
    print(carts)
    counter[0]['no'] = 5 * int(content['items'])
    return jsonify(counter), 201


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)

