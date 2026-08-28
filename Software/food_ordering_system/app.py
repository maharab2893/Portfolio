"""
app.py
------
Flask backend for the Online Food Ordering System.
Run with:  python app.py
"""

from flask import Flask, render_template, request, redirect, url_for, session, flash
from werkzeug.security import generate_password_hash, check_password_hash
from functools import wraps

import database as db

app = Flask(__name__)
app.secret_key = "dev-secret-key-change-me" 


# Startup: make sure the database + tables + view + trigger exist

db.init_db()



# Auth helper decorators


def customer_login_required(view):
    @wraps(view)
    def wrapped(*args, **kwargs):
        if session.get("user_type") != "customer":
            flash("Please log in as a customer to continue.", "warning")
            return redirect(url_for("customer_login"))
        return view(*args, **kwargs)
    return wrapped


def admin_login_required(view):
    @wraps(view)
    def wrapped(*args, **kwargs):
        if session.get("user_type") != "admin":
            flash("Please log in as a restaurant admin to continue.", "warning")
            return redirect(url_for("admin_login"))
        return view(*args, **kwargs)
    return wrapped


def get_cart():
    return session.setdefault("cart", [])

# HOME


@app.route("/")
def index():
    popular_items = db.get_most_popular_items(6)
    top_restaurants = db.get_top_restaurants_by_sales(5)
    return render_template(
        "index.html",
        popular_items=popular_items,
        top_restaurants=top_restaurants,
    )

# CUSTOMER AUTH


@app.route("/customer/register", methods=["GET", "POST"])
def customer_register():
    if request.method == "POST":
        name = request.form["name"].strip()
        address = request.form["address"].strip()
        phone = request.form["phone"].strip()
        email = request.form["email"].strip().lower()
        password = request.form["password"]

        if db.get_customer_by_email(email):
            flash("An account with that email already exists.", "danger")
            return redirect(url_for("customer_register"))

        password_hash = generate_password_hash(password)
        customer_id = db.create_customer(name, address, phone, email, password_hash)

        session.clear()
        session["user_type"] = "customer"
        session["customer_id"] = customer_id
        session["customer_name"] = name
        flash("Account created! Welcome, " + name + ".", "success")
        return redirect(url_for("customer_dashboard"))

    return render_template("customer_register.html")


@app.route("/customer/login", methods=["GET", "POST"])
def customer_login():
    if request.method == "POST":
        email = request.form["email"].strip().lower()
        password = request.form["password"]

        customer = db.get_customer_by_email(email)
        if customer and check_password_hash(customer["PasswordHash"], password):
            session.clear()
            session["user_type"] = "customer"
            session["customer_id"] = customer["CustomerID"]
            session["customer_name"] = customer["Name"]
            flash(f"Welcome back, {customer['Name']}!", "success")
            return redirect(url_for("customer_dashboard"))

        flash("Invalid email or password.", "danger")

    return render_template("customer_login.html")


@app.route("/customer/logout")
def customer_logout():
    session.clear()
    flash("You have been logged out.", "info")
    return redirect(url_for("index"))


# ADMIN (RESTAURANT OWNER) AUTH

@app.route("/admin/register", methods=["GET", "POST"])
def admin_register():
    """Register a new restaurant + its admin/owner account."""
    if request.method == "POST":
        rest_name = request.form["restaurant_name"].strip()
        location = request.form["location"].strip()
        username = request.form["username"].strip().lower()
        password = request.form["password"]

        if db.get_restaurant_by_username(username):
            flash("That admin username is already taken.", "danger")
            return redirect(url_for("admin_register"))

        password_hash = generate_password_hash(password)
        restaurant_id = db.create_restaurant(rest_name, location, username, password_hash)

        session.clear()
        session["user_type"] = "admin"
        session["restaurant_id"] = restaurant_id
        session["restaurant_name"] = rest_name
        flash(f"Restaurant '{rest_name}' registered! You're logged in.", "success")
        return redirect(url_for("admin_dashboard"))

    return render_template("admin_register.html")


@app.route("/admin/login", methods=["GET", "POST"])
def admin_login():
    if request.method == "POST":
        username = request.form["username"].strip().lower()
        password = request.form["password"]

        restaurant = db.get_restaurant_by_username(username)
        if restaurant and check_password_hash(restaurant["AdminPasswordHash"], password):
            session.clear()
            session["user_type"] = "admin"
            session["restaurant_id"] = restaurant["RestaurantID"]
            session["restaurant_name"] = restaurant["Name"]
            flash(f"Welcome back, {restaurant['Name']}!", "success")
            return redirect(url_for("admin_dashboard"))

        flash("Invalid username or password.", "danger")

    return render_template("admin_login.html")


@app.route("/admin/logout")
def admin_logout():
    session.clear()
    flash("Admin logged out.", "info")
    return redirect(url_for("index"))



# CUSTOMER: dashboard, search, menu browsing, cart, ordering, history


@app.route("/dashboard")
@customer_login_required
def customer_dashboard():
    history = db.get_customer_order_history(session["customer_id"])
    summary = db.get_customer_order_summary_for(session["customer_id"])
    return render_template("customer_dashboard.html", history=history, summary=summary)


@app.route("/search/restaurant", methods=["GET"])
@customer_login_required
def search_restaurant():
    keyword = request.args.get("q", "").strip()
    results = db.search_restaurants_by_name(keyword) if keyword else []
    return render_template("search_restaurant.html", keyword=keyword, results=results)


@app.route("/search/item", methods=["GET"])
@customer_login_required
def search_item():
    keyword = request.args.get("q", "").strip()
    results = db.search_items_by_name(keyword) if keyword else []
    return render_template("search_item.html", keyword=keyword, results=results)


@app.route("/restaurant/<int:restaurant_id>/menu")
@customer_login_required
def restaurant_menu(restaurant_id):
    restaurant = db.get_restaurant_by_id(restaurant_id)
    if not restaurant:
        flash("Restaurant not found.", "danger")
        return redirect(url_for("search_restaurant"))
    items = db.get_menu_for_restaurant(restaurant_id)
    return render_template("restaurant_menu.html", restaurant=restaurant, items=items)


@app.route("/cart/add/<int:item_id>", methods=["POST"])
@customer_login_required
def cart_add(item_id):
    quantity = max(1, int(request.form.get("quantity", 1)))
    item = db.get_item_by_id(item_id)
    if not item:
        flash("Item not found.", "danger")
        return redirect(request.referrer or url_for("index"))

    cart = get_cart()
    for line in cart:
        if line["item_id"] == item_id:
            line["quantity"] += quantity
            break
    else:
        cart.append({
            "item_id": item["ItemID"],
            "name": item["Name"],
            "price": item["Price"],
            "quantity": quantity,
            "restaurant_id": item["RestaurantID"],
            "restaurant_name": item["RestaurantName"],
        })
    session["cart"] = cart
    session.modified = True
    flash(f"Added {quantity} x {item['Name']} to your cart.", "success")
    return redirect(request.referrer or url_for("index"))


@app.route("/cart")
@customer_login_required
def view_cart():
    cart = get_cart()
    total = sum(line["price"] * line["quantity"] for line in cart)
    return render_template("cart.html", cart=cart, total=total)


@app.route("/cart/remove/<int:item_id>", methods=["POST"])
@customer_login_required
def cart_remove(item_id):
    cart = get_cart()
    session["cart"] = [line for line in cart if line["item_id"] != item_id]
    session.modified = True
    return redirect(url_for("view_cart"))


@app.route("/cart/clear", methods=["POST"])
@customer_login_required
def cart_clear():
    session["cart"] = []
    session.modified = True
    return redirect(url_for("view_cart"))


@app.route("/order/place", methods=["POST"])
@customer_login_required
def order_place():
    cart = get_cart()
    if not cart:
        flash("Your cart is empty.", "warning")
        return redirect(url_for("view_cart"))

    cart_items = [{"item_id": line["item_id"], "quantity": line["quantity"]} for line in cart]
    order_id = db.place_order(session["customer_id"], cart_items)

    session["cart"] = []
    session.modified = True
    flash(f"Order #{order_id} placed successfully!", "success")
    return redirect(url_for("order_detail", order_id=order_id))


@app.route("/order/<int:order_id>")
@customer_login_required
def order_detail(order_id):
    bill = db.calculate_order_bill(order_id)
    lines = db.get_order_line_items(order_id)
    return render_template("order_detail.html", bill=bill, lines=lines)


@app.route("/orders/history")
@customer_login_required
def order_history():
    history = db.get_customer_order_history(session["customer_id"])
    return render_template("order_history.html", history=history)


# ADMIN: dashboard, menu management, order tracking, analytics


@app.route("/admin/dashboard")
@admin_login_required
def admin_dashboard():
    restaurant_id = session["restaurant_id"]
    totals = db.get_restaurant_totals(restaurant_id)
    recent_orders = db.get_orders_for_restaurant(restaurant_id)[:10]
    menu_items = db.get_menu_for_restaurant(restaurant_id)
    return render_template(
        "admin_dashboard.html",
        totals=totals,
        recent_orders=recent_orders,
        menu_items=menu_items,
    )


@app.route("/admin/menu/add", methods=["GET", "POST"])
@admin_login_required
def admin_add_item():
    if request.method == "POST":
        name = request.form["name"].strip()
        price = float(request.form["price"])
        avg_time = int(request.form["avg_time"])
        db.add_menu_item(session["restaurant_id"], name, price, avg_time)
        flash(f"Added '{name}' to your menu.", "success")
        return redirect(url_for("admin_dashboard"))

    return render_template("admin_add_item.html")


@app.route("/admin/orders")
@admin_login_required
def admin_orders():
    orders = db.get_orders_for_restaurant(session["restaurant_id"])
    return render_template("admin_orders.html", orders=orders)


@app.route("/admin/analytics")
@admin_login_required
def admin_analytics():
    restaurant_id = session["restaurant_id"]
    daily = db.get_daily_revenue_for_restaurant(restaurant_id, days=30)
    monthly = db.get_monthly_revenue_for_restaurant(restaurant_id)
    top_items = db.get_most_popular_items_for_restaurant(restaurant_id, limit=10)
    return render_template(
        "admin_analytics.html",
        daily=daily,
        monthly=monthly,
        top_items=top_items,
    )


if __name__ == "__main__":
    app.run(debug=True)
