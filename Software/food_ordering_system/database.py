"""
database.py
------------
All database access for the Online Food Ordering System lives here as
plain, readable SQL wrapped in small Python functions. No ORM is used,
per the project requirements, so every query below is the exact SQL
that gets executed against SQLite.
"""

import sqlite3
import os
from datetime import datetime

DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "food_ordering.db")
SCHEMA_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "schema.sql")


def get_connection():
    """Open a new connection with foreign keys enforced and dict-like rows."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


def init_db():
    """Create the database from schema.sql if it does not exist yet."""
    first_time = not os.path.exists(DB_PATH)
    conn = get_connection()
    with open(SCHEMA_PATH, "r") as f:
        conn.executescript(f.read())
    conn.commit()
    conn.close()
    return first_time



# CUSTOMERS: auth + profile


def create_customer(name, address, phone, email, password_hash):
    sql = """
        INSERT INTO Customers (Name, Address, Phone, Email, PasswordHash)
        VALUES (?, ?, ?, ?, ?)
    """
    conn = get_connection()
    cur = conn.execute(sql, (name, address, phone, email, password_hash))
    conn.commit()
    new_id = cur.lastrowid
    conn.close()
    return new_id


def get_customer_by_email(email):
    sql = "SELECT * FROM Customers WHERE Email = ?"
    conn = get_connection()
    row = conn.execute(sql, (email,)).fetchone()
    conn.close()
    return row


def get_customer_by_id(customer_id):
    sql = "SELECT * FROM Customers WHERE CustomerID = ?"
    conn = get_connection()
    row = conn.execute(sql, (customer_id,)).fetchone()
    conn.close()
    return row


# RESTAURANTS / ADMINS: auth + profile


def create_restaurant(name, location, admin_username, admin_password_hash):
    sql = """
        INSERT INTO Restaurants (Name, Location, AdminUsername, AdminPasswordHash)
        VALUES (?, ?, ?, ?)
    """
    conn = get_connection()
    cur = conn.execute(sql, (name, location, admin_username, admin_password_hash))
    conn.commit()
    new_id = cur.lastrowid
    conn.close()
    return new_id


def get_restaurant_by_username(username):
    sql = "SELECT * FROM Restaurants WHERE AdminUsername = ?"
    conn = get_connection()
    row = conn.execute(sql, (username,)).fetchone()
    conn.close()
    return row


def get_restaurant_by_id(restaurant_id):
    sql = "SELECT * FROM Restaurants WHERE RestaurantID = ?"
    conn = get_connection()
    row = conn.execute(sql, (restaurant_id,)).fetchone()
    conn.close()
    return row


def get_all_restaurants():
    sql = "SELECT * FROM Restaurants ORDER BY Name"
    conn = get_connection()
    rows = conn.execute(sql).fetchall()
    conn.close()
    return rows



# SEARCH


def search_restaurants_by_name(keyword):
    """Search by Restaurant Name (partial, case-insensitive)."""
    sql = """
        SELECT * FROM Restaurants
        WHERE Name LIKE ? COLLATE NOCASE
        ORDER BY Name
    """
    conn = get_connection()
    rows = conn.execute(sql, (f"%{keyword}%",)).fetchall()
    conn.close()
    return rows


def search_items_by_name(keyword):
    """
    Search by Food Item Name. Returns matching items joined with the
    restaurant name so the customer can see who sells each dish.
    """
    sql = """
        SELECT mi.ItemID, mi.Name AS ItemName, mi.Price, mi.AverageMakingTime,
               r.RestaurantID, r.Name AS RestaurantName, r.Location
        FROM MenuItems mi
        JOIN Restaurants r ON r.RestaurantID = mi.RestaurantID
        WHERE mi.Name LIKE ? COLLATE NOCASE
        ORDER BY mi.Name
    """
    conn = get_connection()
    rows = conn.execute(sql, (f"%{keyword}%",)).fetchall()
    conn.close()
    return rows


def get_menu_for_restaurant(restaurant_id):
    sql = """
        SELECT * FROM MenuItems
        WHERE RestaurantID = ?
        ORDER BY Name
    """
    conn = get_connection()
    rows = conn.execute(sql, (restaurant_id,)).fetchall()
    conn.close()
    return rows


def get_item_by_id(item_id):
    sql = """
        SELECT mi.*, r.Name AS RestaurantName
        FROM MenuItems mi
        JOIN Restaurants r ON r.RestaurantID = mi.RestaurantID
        WHERE mi.ItemID = ?
    """
    conn = get_connection()
    row = conn.execute(sql, (item_id,)).fetchone()
    conn.close()
    return row



# ADMIN: menu management


def add_menu_item(restaurant_id, name, price, avg_making_time):
    sql = """
        INSERT INTO MenuItems (RestaurantID, Name, Price, AverageMakingTime)
        VALUES (?, ?, ?, ?)
    """
    conn = get_connection()
    cur = conn.execute(sql, (restaurant_id, name, price, avg_making_time))
    conn.commit()
    new_id = cur.lastrowid
    conn.close()
    return new_id



# REQUIRED SQL FUNCTIONALITY #1: Place a new order
# Inserts one row into Orders, then one row per cart line into
# OrderDetails. The AFTER INSERT trigger on OrderDetails automatically
# recalculates Orders.TotalAmount every time a line item is added, so
# we do not compute or set the total ourselves here.


def place_order(customer_id, cart_items):
    """
    cart_items: list of {"item_id": int, "quantity": int}
    Returns the new OrderID.
    """
    conn = get_connection()
    try:
        cur = conn.execute(
            "INSERT INTO Orders (CustomerID, OrderDate, TotalAmount) VALUES (?, ?, 0)",
            (customer_id, datetime.now().strftime("%Y-%m-%d %H:%M:%S")),
        )
        order_id = cur.lastrowid

        for line in cart_items:
            conn.execute(
                "INSERT INTO OrderDetails (OrderID, ItemID, Quantity) VALUES (?, ?, ?)",
                (order_id, line["item_id"], line["quantity"]),
            )
            # ^ trg_update_order_total_on_insert fires here automatically

        conn.commit()
        return order_id
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()


# REQUIRED SQL FUNCTIONALITY #2: Calculate the total bill for an order
# In normal operation the trigger keeps Orders.TotalAmount correct, but
# this query independently recomputes the bill straight from the line
# items -- useful for a printable receipt or for verifying the trigger.

def calculate_order_bill(order_id):
    sql = """
        SELECT
            o.OrderID,
            o.OrderDate,
            o.TotalAmount AS StoredTotal,
            COALESCE(SUM(od.Quantity * mi.Price), 0) AS ComputedTotal
        FROM Orders o
        LEFT JOIN OrderDetails od ON od.OrderID = o.OrderID
        LEFT JOIN MenuItems mi ON mi.ItemID = od.ItemID
        WHERE o.OrderID = ?
        GROUP BY o.OrderID
    """
    conn = get_connection()
    row = conn.execute(sql, (order_id,)).fetchone()
    conn.close()
    return row


def get_order_line_items(order_id):
    sql = """
        SELECT mi.Name AS ItemName, mi.Price, od.Quantity,
               (mi.Price * od.Quantity) AS LineTotal,
               r.Name AS RestaurantName
        FROM OrderDetails od
        JOIN MenuItems mi ON mi.ItemID = od.ItemID
        JOIN Restaurants r ON r.RestaurantID = mi.RestaurantID
        WHERE od.OrderID = ?
    """
    conn = get_connection()
    rows = conn.execute(sql, (order_id,)).fetchall()
    conn.close()
    return rows


# REQUIRED SQL FUNCTIONALITY #3: Full order history of a customer

def get_customer_order_history(customer_id):
    sql = """
        SELECT o.OrderID, o.OrderDate, o.TotalAmount
        FROM Orders o
        WHERE o.CustomerID = ?
        ORDER BY o.OrderDate DESC
    """
    conn = get_connection()
    rows = conn.execute(sql, (customer_id,)).fetchall()
    conn.close()
    return rows


# REQUIRED SQL FUNCTIONALITY #4: Most popular food items
# (ranked by total quantity ordered across all customers/restaurants)

def get_most_popular_items(limit=10):
    sql = """
        SELECT mi.ItemID, mi.Name AS ItemName, r.Name AS RestaurantName,
               SUM(od.Quantity) AS TotalQuantityOrdered
        FROM OrderDetails od
        JOIN MenuItems mi ON mi.ItemID = od.ItemID
        JOIN Restaurants r ON r.RestaurantID = mi.RestaurantID
        GROUP BY mi.ItemID
        ORDER BY TotalQuantityOrdered DESC
        LIMIT ?
    """
    conn = get_connection()
    rows = conn.execute(sql, (limit,)).fetchall()
    conn.close()
    return rows


def get_most_popular_items_for_restaurant(restaurant_id, limit=10):
    sql = """
        SELECT mi.ItemID, mi.Name AS ItemName,
               SUM(od.Quantity) AS TotalQuantityOrdered,
               SUM(od.Quantity * mi.Price) AS Revenue
        FROM OrderDetails od
        JOIN MenuItems mi ON mi.ItemID = od.ItemID
        WHERE mi.RestaurantID = ?
        GROUP BY mi.ItemID
        ORDER BY TotalQuantityOrdered DESC
        LIMIT ?
    """
    conn = get_connection()
    rows = conn.execute(sql, (restaurant_id, limit)).fetchall()
    conn.close()
    return rows


# REQUIRED SQL FUNCTIONALITY #5: Restaurants with highest overall sales

def get_top_restaurants_by_sales(limit=10):
    sql = """
        SELECT r.RestaurantID, r.Name AS RestaurantName, r.Location,
               COALESCE(SUM(od.Quantity * mi.Price), 0) AS TotalSales
        FROM Restaurants r
        LEFT JOIN MenuItems mi ON mi.RestaurantID = r.RestaurantID
        LEFT JOIN OrderDetails od ON od.ItemID = mi.ItemID
        GROUP BY r.RestaurantID
        ORDER BY TotalSales DESC
        LIMIT ?
    """
    conn = get_connection()
    rows = conn.execute(sql, (limit,)).fetchall()
    conn.close()
    return rows


# ADMIN DASHBOARD: order tracking
# Shows, for a given restaurant, which customer ordered which item:
# Customer Name, Customer ID, and detailed order info.

def get_orders_for_restaurant(restaurant_id):
    sql = """
        SELECT
            o.OrderID, o.OrderDate,
            c.CustomerID, c.Name AS CustomerName, c.Phone, c.Address,
            mi.Name AS ItemName, od.Quantity, mi.Price,
            (od.Quantity * mi.Price) AS LineTotal
        FROM OrderDetails od
        JOIN MenuItems mi ON mi.ItemID = od.ItemID
        JOIN Orders o ON o.OrderID = od.OrderID
        JOIN Customers c ON c.CustomerID = o.CustomerID
        WHERE mi.RestaurantID = ?
        ORDER BY o.OrderDate DESC
    """
    conn = get_connection()
    rows = conn.execute(sql, (restaurant_id,)).fetchall()
    conn.close()
    return rows


# ADVANCED FEATURE: VIEW usage - CustomerOrderSummary


def get_customer_order_summary():
    sql = "SELECT * FROM CustomerOrderSummary ORDER BY TotalSpent DESC"
    conn = get_connection()
    rows = conn.execute(sql).fetchall()
    conn.close()
    return rows


def get_customer_order_summary_for(customer_id):
    sql = "SELECT * FROM CustomerOrderSummary WHERE CustomerID = ?"
    conn = get_connection()
    row = conn.execute(sql, (customer_id,)).fetchone()
    conn.close()
    return row


# ADVANCED FEATURE: Sales Analytics (GROUP BY + aggregation)

def get_daily_revenue_for_restaurant(restaurant_id, days=30):
    sql = """
        SELECT
            DATE(o.OrderDate) AS SaleDay,
            SUM(od.Quantity * mi.Price) AS Revenue,
            SUM(od.Quantity) AS ItemsSold,
            COUNT(DISTINCT o.OrderID) AS OrderCount
        FROM OrderDetails od
        JOIN MenuItems mi ON mi.ItemID = od.ItemID
        JOIN Orders o ON o.OrderID = od.OrderID
        WHERE mi.RestaurantID = ?
          AND DATE(o.OrderDate) >= DATE('now', ?)
        GROUP BY SaleDay
        ORDER BY SaleDay DESC
    """
    conn = get_connection()
    rows = conn.execute(sql, (restaurant_id, f"-{days} days")).fetchall()
    conn.close()
    return rows


def get_monthly_revenue_for_restaurant(restaurant_id):
    sql = """
        SELECT
            strftime('%Y-%m', o.OrderDate) AS SaleMonth,
            SUM(od.Quantity * mi.Price) AS Revenue,
            SUM(od.Quantity) AS ItemsSold,
            COUNT(DISTINCT o.OrderID) AS OrderCount
        FROM OrderDetails od
        JOIN MenuItems mi ON mi.ItemID = od.ItemID
        JOIN Orders o ON o.OrderID = od.OrderID
        WHERE mi.RestaurantID = ?
        GROUP BY SaleMonth
        ORDER BY SaleMonth DESC
    """
    conn = get_connection()
    rows = conn.execute(sql, (restaurant_id,)).fetchall()
    conn.close()
    return rows


def get_restaurant_totals(restaurant_id):
    """Quick stats block for the admin dashboard header."""
    sql = """
        SELECT
            COUNT(DISTINCT o.OrderID) AS TotalOrders,
            COALESCE(SUM(od.Quantity * mi.Price), 0) AS TotalRevenue,
            COUNT(DISTINCT mi.ItemID) AS MenuItemCount
        FROM MenuItems mi
        LEFT JOIN OrderDetails od ON od.ItemID = mi.ItemID
        LEFT JOIN Orders o ON o.OrderID = od.OrderID
        WHERE mi.RestaurantID = ?
    """
    conn = get_connection()
    row = conn.execute(sql, (restaurant_id,)).fetchone()
    conn.close()
    return row
