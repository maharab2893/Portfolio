-- =====================================================================
-- Online Food Ordering System - Database Schema (SQLite)
-- =====================================================================
-- Run this once to create all tables, the view, and the trigger.
-- The Flask app (database.py) also runs this automatically on first
-- launch if food_ordering.db does not exist yet.
-- =====================================================================

PRAGMA foreign_keys = ON;

-- ---------------------------------------------------------------------
-- 1. CUSTOMERS
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS Customers (
    CustomerID     INTEGER PRIMARY KEY AUTOINCREMENT,
    Name           TEXT NOT NULL,
    Address        TEXT NOT NULL,
    Phone          TEXT NOT NULL,
    Email          TEXT NOT NULL UNIQUE,
    PasswordHash   TEXT NOT NULL,
    CreatedAt      TEXT NOT NULL DEFAULT (datetime('now'))
);

-- ---------------------------------------------------------------------
-- 2. RESTAURANTS  (Restaurant = Admin account, one owner per restaurant)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS Restaurants (
    RestaurantID       INTEGER PRIMARY KEY AUTOINCREMENT,
    Name               TEXT NOT NULL,
    Location           TEXT NOT NULL,
    AdminUsername      TEXT NOT NULL UNIQUE,
    AdminPasswordHash  TEXT NOT NULL,
    CreatedAt          TEXT NOT NULL DEFAULT (datetime('now'))
);

-- ---------------------------------------------------------------------
-- 3. MENU ITEMS
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS MenuItems (
    ItemID               INTEGER PRIMARY KEY AUTOINCREMENT,
    RestaurantID         INTEGER NOT NULL,
    Name                 TEXT NOT NULL,
    Price                REAL NOT NULL CHECK (Price >= 0),
    AverageMakingTime   INTEGER NOT NULL CHECK (AverageMakingTime >= 0), -- minutes
    FOREIGN KEY (RestaurantID) REFERENCES Restaurants(RestaurantID) ON DELETE CASCADE
);

-- ---------------------------------------------------------------------
-- 4. ORDERS
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS Orders (
    OrderID       INTEGER PRIMARY KEY AUTOINCREMENT,
    CustomerID    INTEGER NOT NULL,
    OrderDate     TEXT NOT NULL DEFAULT (datetime('now')),
    TotalAmount   REAL NOT NULL DEFAULT 0,
    FOREIGN KEY (CustomerID) REFERENCES Customers(CustomerID) ON DELETE CASCADE
);

-- ---------------------------------------------------------------------
-- 5. ORDER DETAILS  (line items of an order)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS OrderDetails (
    OrderID    INTEGER NOT NULL,
    ItemID     INTEGER NOT NULL,
    Quantity   INTEGER NOT NULL CHECK (Quantity > 0),
    PRIMARY KEY (OrderID, ItemID),
    FOREIGN KEY (OrderID) REFERENCES Orders(OrderID) ON DELETE CASCADE,
    FOREIGN KEY (ItemID)  REFERENCES MenuItems(ItemID) ON DELETE CASCADE
);

-- Helpful indexes
CREATE INDEX IF NOT EXISTS idx_menuitems_restaurant ON MenuItems(RestaurantID);
CREATE INDEX IF NOT EXISTS idx_menuitems_name ON MenuItems(Name);
CREATE INDEX IF NOT EXISTS idx_orders_customer ON Orders(CustomerID);
CREATE INDEX IF NOT EXISTS idx_orderdetails_item ON OrderDetails(ItemID);

-- =====================================================================
-- ADVANCED FEATURE 1: VIEW - CustomerOrderSummary
-- Consolidated report: customer name, total orders placed, total spent.
-- =====================================================================
DROP VIEW IF EXISTS CustomerOrderSummary;
CREATE VIEW CustomerOrderSummary AS
SELECT
    c.CustomerID                          AS CustomerID,
    c.Name                                AS CustomerName,
    COUNT(DISTINCT o.OrderID)             AS TotalOrders,
    COALESCE(SUM(o.TotalAmount), 0)       AS TotalSpent
FROM Customers c
LEFT JOIN Orders o ON o.CustomerID = c.CustomerID
GROUP BY c.CustomerID, c.Name;

-- =====================================================================
-- ADVANCED FEATURE 2: TRIGGER - auto-update Orders.TotalAmount
-- Fires whenever a line item is inserted into OrderDetails; it
-- recalculates the parent order's total from scratch so it is always
-- consistent even if items are added across multiple statements.
-- =====================================================================
DROP TRIGGER IF EXISTS trg_update_order_total_on_insert;
CREATE TRIGGER trg_update_order_total_on_insert
AFTER INSERT ON OrderDetails
BEGIN
    UPDATE Orders
    SET TotalAmount = (
        SELECT COALESCE(SUM(od.Quantity * mi.Price), 0)
        FROM OrderDetails od
        JOIN MenuItems mi ON mi.ItemID = od.ItemID
        WHERE od.OrderID = NEW.OrderID
    )
    WHERE OrderID = NEW.OrderID;
END;

-- Bonus: keep totals correct if a line item is ever updated or removed
DROP TRIGGER IF EXISTS trg_update_order_total_on_update;
CREATE TRIGGER trg_update_order_total_on_update
AFTER UPDATE ON OrderDetails
BEGIN
    UPDATE Orders
    SET TotalAmount = (
        SELECT COALESCE(SUM(od.Quantity * mi.Price), 0)
        FROM OrderDetails od
        JOIN MenuItems mi ON mi.ItemID = od.ItemID
        WHERE od.OrderID = NEW.OrderID
    )
    WHERE OrderID = NEW.OrderID;
END;

DROP TRIGGER IF EXISTS trg_update_order_total_on_delete;
CREATE TRIGGER trg_update_order_total_on_delete
AFTER DELETE ON OrderDetails
BEGIN
    UPDATE Orders
    SET TotalAmount = (
        SELECT COALESCE(SUM(od.Quantity * mi.Price), 0)
        FROM OrderDetails od
        JOIN MenuItems mi ON mi.ItemID = od.ItemID
        WHERE od.OrderID = OLD.OrderID
    )
    WHERE OrderID = OLD.OrderID;
END;
